//  -------------------------------------------------------------------------
//  Copyright (C) 2017 BMW Car IT GmbH
//  -------------------------------------------------------------------------
//  This Source Code Form is subject to the terms of the Mozilla Public
//  License, v. 2.0. If a copy of the MPL was not distributed with this
//  file, You can obtain one at https://mozilla.org/MPL/2.0/.
//  -------------------------------------------------------------------------

#include "ramses-client.h"

#include <cstdio>
#include <thread>
#include <chrono>
#include <cmath>
#include <vector>

/**
 * @example ramses-example-data-buffers-vertices/src/main.cpp
 * @brief Basic Data Buffer Example
 */

int main(int argc, char* argv[])
{
    // register at RAMSES daemon
    ramses::RamsesFramework framework(argc, argv);
    ramses::RamsesClient& ramses(*framework.createClient("ramses-example-data-buffers-vertices"));
    framework.connect();

    // create a scene for distributing content
    ramses::Scene* scene = ramses.createScene(ramses::sceneId_t(123u), ramses::SceneConfig(), "triangle scene");

    // every scene needs a render pass with camera
    ramses::Camera* camera = scene->createRemoteCamera("my camera");
    camera->setTranslation(0.0f, 0.0f, 35.0f);
    ramses::RenderPass* renderPass = scene->createRenderPass("my render pass");
    renderPass->setClearFlags(ramses::EClearFlags_None);
    renderPass->setCamera(*camera);
    ramses::RenderGroup* renderGroup = scene->createRenderGroup();
    renderPass->addRenderGroup(*renderGroup);

    // create an appearance for triangles
    ramses::EffectDescription effectDesc;
    effectDesc.setVertexShaderFromFile("res/ramses-example-data-buffers-vertices.vert");
    effectDesc.setFragmentShaderFromFile("res/ramses-example-data-buffers-vertices.frag");
    effectDesc.setUniformSemantic("mvpMatrix", ramses::EEffectUniformSemantic_ModelViewProjectionMatrix);

    ramses::Effect* effect = ramses.createEffect(effectDesc, ramses::ResourceCacheFlag_DoNotCache, "glsl shader");
    ramses::Appearance* appearance = scene->createAppearance(*effect, "triangle appearance data buffer");
    ramses::GeometryBinding* geometry = scene->createGeometryBinding(*effect, "triangle geometry data buffer");

    // get input data of appearance and set color values
    ramses::UniformInput colorInput;
    effect->findUniformInput("color", colorInput);
    appearance->setInputValueVector4f(colorInput, 0.0f, 1.0f, 1.0f, 1.0f);

    // the raw data for vertices and indices
    const float vertexPositions[] = {
        -1.0,  4.0, -1.0,    0.0,  4.0, -1.0,    1.0,  4.0, -1.0,
        -2.0,  2.0, -1.0,                        2.0,  2.0, -1.0,
        -4.0,  1.0, -1.0,                        4.0,  1.0, -1.0,
        -4.0,  0.0, -1.0,                        4.0,  0.0, -1.0,
        -4.0, -1.0, -1.0,                        4.0, -1.0, -1.0,
        -2.0, -2.0, -1.0,                        2.0, -2.0, -1.0,
        -1.0, -4.0, -1.0,    0.0, -4.0, -1.0,    1.0, -4.0, -1.0  };


    const uint16_t indexData[] = {
        3,   1,  0,     4,  2,  1,     4,  1,  3,
        8,   6,  4,    12, 10,  8,    12,  8,  4,
        14, 15, 12,    11, 13, 14,    11, 14, 12,
        7,   9, 11,     3,  5,  7,     3,  7, 11,
        4,   3, 11,    11, 12,  4
    };

    /// [Data Buffer Example Setup]
    /// Creating a shape with DataBuffer vertices and constantly changing them afterwards
    //
    // IMPORTANT NOTE: For simplicity and readability the example code does not check return values from API calls.
    //                 This should not be the case for real applications.

    // Create the DataBuffers
    // The data buffers need more information about size
    const uint32_t SizeOfVerticesInBytes = 16 * 3 * sizeof(float);
    const uint32_t SizeOfIndicesInBytes  = 42 * sizeof(uint16_t);

    // then create the buffers via the _scene_
    ramses::VertexDataBuffer* vertices = scene->createVertexDataBuffer( SizeOfVerticesInBytes, ramses::EDataType_Vector3F, "some varying vertices");
    ramses::IndexDataBuffer*  indices  = scene->createIndexDataBuffer(  SizeOfIndicesInBytes,  ramses::EDataType_UInt16,   "some varying indices");

    // finally set/update the data
    vertices->setData(reinterpret_cast<const char*>(vertexPositions), SizeOfVerticesInBytes);
    indices->setData( reinterpret_cast<const char*>(indexData), SizeOfIndicesInBytes);

    /// [Data Buffer Example Setup]

    // applying the vertex/index data to the geometry binding is the same for both
    ramses::AttributeInput positionsInput;
    effect->findAttributeInput("a_position", positionsInput);
    geometry->setInputBuffer(positionsInput, *vertices);
    geometry->setIndices(*indices);


    // create a mesh node to define the triangle with chosen appearance
    ramses::MeshNode* meshNode = scene->createMeshNode("triangle mesh node data buffers");

    meshNode->setAppearance(*appearance);
    meshNode->setGeometryBinding(*geometry);

    // mesh needs to be added to a render group that belongs to a render pass with camera in order to be rendered
    renderGroup->addMeshNode(*meshNode);

    // signal the scene it is in a state that can be rendered
    scene->flush();

    // distribute the scene to RAMSES
    scene->publish();

    // application logic
    auto translateVertex = [&vertexPositions](float* updatedValues, uint32_t i, uint64_t timeStamp, float scaleFactor){
        const float currentFactor = static_cast<float>(std::sin(0.005f*timeStamp));
        const uint32_t index      = i * 3u;
        const float    xValue     = vertexPositions[index+0];
        const float    yValue     = vertexPositions[index+1];

        updatedValues[0] = xValue + scaleFactor * xValue * currentFactor;
        updatedValues[1] = yValue + scaleFactor * yValue * currentFactor;
    };

    /// [Data Buffer Example Loop]
    const std::vector<uint32_t> ridges  = { 1, 7,  8, 14 };
    const std::vector<uint32_t> notches = { 3, 4, 11, 12 };

    for (uint64_t timeStamp = 0u; timeStamp < 10000u; timeStamp += 20u)
    {
        for(auto i: ridges)
        {
            float updatedValues[2];
            const uint32_t updatedValuesLength = 2 * sizeof(float);
            const uint32_t offsetInDataBuffer  = i * 3u * sizeof(float);

            translateVertex(updatedValues, i, timeStamp, 0.25f);
            vertices->setData(reinterpret_cast<const char*>(updatedValues), updatedValuesLength, offsetInDataBuffer);
        }

        for(auto i: notches)
        {
            float updatedValues[2];
            const uint32_t updatedValuesLength = 2 * sizeof(float);
            const uint32_t offsetInDataBuffer  = i * 3u * sizeof(float);

            translateVertex(updatedValues, i, timeStamp, -0.4f);
            vertices->setData(reinterpret_cast<const char*>(updatedValues), updatedValuesLength, offsetInDataBuffer);
        }

        // signal the scene it is in a state that can be rendered
        scene->flush();

        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    /// [Data Buffer Example Loop]

    // shutdown: stop distribution, free resources, unregister
    scene->unpublish();
    ramses.destroy(*scene);
    framework.disconnect();

    return 0;
}
