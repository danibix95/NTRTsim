/*
 * Copyright © 2012, United States Government, as represented by the
 * Administrator of the National Aeronautics and Space Administration.
 * All rights reserved.
 * 
 * The NASA Tensegrity Robotics Toolkit (NTRT) v1 platform is licensed
 * under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * http://www.apache.org/licenses/LICENSE-2.0.
 * 
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
 * either express or implied. See the License for the specific language
 * governing permissions and limitations under the License.
*/

/**
 * @file TensegrityModel.cpp
 * @brief Contains the definition of the members of the class TensegrityModel.
 * $Id$
 */

// This module
#include "TensegrityModel.h"
// This library
#include "core/tgBasicActuator.h"
#include "core/tgRod.h"
#include "tgcreator/tgBuildSpec.h"
#include "tgcreator/tgBasicActuatorInfo.h"
#include "tgcreator/tgRodInfo.h"
#include "tgcreator/tgStructure.h"
#include "tgcreator/tgStructureInfo.h"
// The Bullet Physics library
#include "LinearMath/btVector3.h"
// The C++ Standard Library
#include <stdexcept>
#include "helpers/FileHelpers.h"
#include <json/json.h>
#include <iostream>
#include <string>

using namespace std;

string TensegrityModel::jsonPath;

TensegrityModel::TensegrityModel(string j) :
tgModel() 
{
    jsonPath = j;
}

TensegrityModel::~TensegrityModel()
{
}

void TensegrityModel::addNodes(tgStructure& s, Json::Value root)
{
    Json::Value nodes = root["structure"]["nodes"];
    for (unsigned int i = 0; i < nodes.size(); i++) {
        double x = nodes[i]["coordinates"][0].asDouble();
        double y = nodes[i]["coordinates"][1].asDouble();
        double z = nodes[i]["coordinates"][2].asDouble();
        s.addNode(x, y, z);
    }
}

void TensegrityModel::addRods(tgStructure& s, Json::Value root)
{
    Json::Value rods = root["structure"]["rods"];
    for (unsigned int i = 0; i < rods.size(); i++) {
        int n1 = rods[i][0].asInt();
        int n2 = rods[i][1].asInt();
        s.addPair(n1 - 1, n2 - 1, "rod");
    } 
}

void TensegrityModel::addMuscles(tgStructure& s, Json::Value root)
{
    Json::Value muscles = root["structure"]["muscles"];
    for (unsigned int i = 0; i < muscles.size(); i++) {
        int n1 = muscles[i][0].asInt();
        int n2 = muscles[i][1].asInt();
        s.addPair(n1 - 1, n2 - 1, "muscle");
    }
}

void TensegrityModel::setup(tgWorld& world)
{
    // Read in JSON file
    Json::Value root; // will contains the root value after parsing.
    Json::Reader reader;
    bool parsingSuccessful = reader.parse( FileHelpers::getFileString(jsonPath), root );
    if ( !parsingSuccessful )
    {
        // report to the user the failure and their locations in the document.
        std::cout << "Failed to parse configuration\n"
            << reader.getFormattedErrorMessages();
        return;
    }

    Json::Value rodParams = root["parameters"]["rods"];
    Json::Value muscleParams = root["parameters"]["muscles"];

    double radius = rodParams["radius"].asDouble();
    double density = rodParams["density"].asDouble();
    double stiffness = muscleParams["stiffness"].asDouble();
    double damping = muscleParams["damping"].asDouble();
    double pretension = muscleParams["pretension"].asDouble();

    // Define the configurations of the rods and strings
    // Note that pretension is defined for this string
    const tgRod::Config rodConfig(radius, density);
    const tgSpringCableActuator::Config muscleConfig(stiffness, damping, pretension);
    
    // Create a structure that will hold the details of this model
    tgStructure s;
    
    // Add nodes to the structure
    addNodes(s, root);
    
    // Add rods to the structure
    addRods(s, root);
    
    // Add muscles to the structure
    addMuscles(s, root);
    
    // Move the structure so it doesn't start in the ground
    s.move(btVector3(0, 10, 0));
    
    // Create the build spec that uses tags to turn the structure into a real model
    tgBuildSpec spec;
    spec.addBuilder("rod", new tgRodInfo(rodConfig));
    spec.addBuilder("muscle", new tgBasicActuatorInfo(muscleConfig));
    
    // Create your structureInfo
    tgStructureInfo structureInfo(s, spec);

    // Use the structureInfo to build ourselves
    structureInfo.buildInto(*this, world);

    // We could now use tgCast::filter or similar to pull out the
    // models (e.g. muscles) that we want to control. 
    allActuators = tgCast::filter<tgModel, tgSpringCableActuator> (getDescendants());
    
    // Notify controllers that setup has finished.
    notifySetup();
    
    // Actually setup the children
    tgModel::setup(world);
}

void TensegrityModel::step(double dt)
{
    // Precondition
    if (dt <= 0.0)
    {
        throw std::invalid_argument("dt is not positive");
    }
    else
    {
        // Notify observers (controllers) of the step so that they can take action
        notifyStep(dt);
        tgModel::step(dt);  // Step any children
    }
}

void TensegrityModel::onVisit(tgModelVisitor& r)
{
    // Example: m_rod->getRigidBody()->dosomething()...
    tgModel::onVisit(r);
}

const std::vector<tgSpringCableActuator*>& TensegrityModel::getAllActuators() const
{
    return allActuators;
}
    
void TensegrityModel::teardown()
{
    notifyTeardown();
    tgModel::teardown();
}
