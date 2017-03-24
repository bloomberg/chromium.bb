// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/vr_shell/gltf_parser.h"

#include <memory>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/json_file_value_serializer.h"
#include "base/memory/ptr_util.h"
#include "base/path_service.h"
#include "base/values.h"
#include "chrome/browser/android/vr_shell/test/paths.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace vr_shell {

TEST(GltfParser, Parse) {
  test::RegisterPathProvider();
  base::FilePath gltf_path;
  PathService::Get(test::DIR_TEST_DATA, &gltf_path);
  gltf_path = gltf_path.Append("sample_inline.gltf");

  int error_code;
  std::string error_msg;
  JSONFileValueDeserializer json_deserializer(gltf_path);
  auto asset_value = json_deserializer.Deserialize(&error_code, &error_msg);
  EXPECT_NE(nullptr, asset_value);
  base::DictionaryValue* asset;
  EXPECT_TRUE(asset_value->GetAsDictionary(&asset));

  GltfParser parser;
  auto gltf_model = parser.Parse(*asset);
  EXPECT_TRUE(gltf_model);

  const gltf::Buffer* buffer = gltf_model->GetBuffer(0);
  EXPECT_NE(nullptr, buffer);
  EXPECT_EQ(nullptr, gltf_model->GetBuffer(1));
  EXPECT_EQ("HELLO WORLD!", *buffer);
  EXPECT_EQ(12u, buffer->length());

  const gltf::BufferView* buffer_view = gltf_model->GetBufferView(0);
  EXPECT_NE(nullptr, buffer_view);
  EXPECT_EQ(nullptr, gltf_model->GetBufferView(1));
  EXPECT_EQ(buffer, buffer_view->buffer);
  EXPECT_EQ(20, buffer_view->byte_length);
  EXPECT_EQ(10, buffer_view->byte_offset);
  EXPECT_EQ(1, buffer_view->target);

  const gltf::Accessor* accessor = gltf_model->GetAccessor(0);
  EXPECT_NE(nullptr, accessor);
  EXPECT_EQ(nullptr, gltf_model->GetAccessor(1));
  EXPECT_EQ(buffer_view, accessor->buffer_view);
  EXPECT_EQ(10, accessor->byte_offset);
  EXPECT_EQ(16, accessor->byte_stride);
  EXPECT_EQ(5, accessor->component_type);
  EXPECT_EQ(24, accessor->count);
  EXPECT_EQ(gltf::VEC3, accessor->type);

  const gltf::Mesh* mesh = gltf_model->GetMesh(0);
  EXPECT_NE(nullptr, mesh);
  EXPECT_EQ(nullptr, gltf_model->GetAccessor(1));
  EXPECT_EQ(1u, mesh->primitives.size());
  gltf::Mesh::Primitive* primitive = mesh->primitives[0].get();
  EXPECT_EQ(1u, primitive->attributes.size());
  auto attr_it = primitive->attributes.begin();
  EXPECT_EQ("POSITION", attr_it->first);
  EXPECT_EQ(accessor, attr_it->second);
  EXPECT_EQ(accessor, primitive->indices);
  EXPECT_EQ(4, primitive->mode);

  const gltf::Node* node_1 = gltf_model->GetNode(0);
  const gltf::Node* node_2 = gltf_model->GetNode(1);
  EXPECT_NE(nullptr, node_1);
  EXPECT_NE(nullptr, node_2);
  EXPECT_EQ(nullptr, gltf_model->GetNode(2));
  EXPECT_EQ(1u, node_1->children.size());
  EXPECT_EQ(0u, node_1->meshes.size());
  EXPECT_EQ(node_2, node_1->children[0]);
  EXPECT_EQ(0u, node_2->children.size());
  EXPECT_EQ(1u, node_2->meshes.size());
  EXPECT_EQ(mesh, node_2->meshes[0]);

  const gltf::Scene* scene = gltf_model->GetScene(0);
  EXPECT_NE(nullptr, scene);
  EXPECT_EQ(nullptr, gltf_model->GetNode(2));
  EXPECT_EQ(1u, scene->nodes.size());
  EXPECT_EQ(node_1, scene->nodes[0]);

  EXPECT_EQ(scene, gltf_model->GetMainScene());
}

}  // namespace vr_shell
