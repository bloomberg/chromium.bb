// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/gltf_parser.h"

#include <memory>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/json_file_value_serializer.h"
#include "base/memory/ptr_util.h"
#include "base/values.h"
#include "chrome/browser/vr/test/paths.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace vr {

class DataDrivenTest : public testing::Test {
 protected:
  void SetUp() override { test::GetTestDataPath(&data_dir_); }

  base::FilePath data_dir_;
};

class GltfParserTest : public DataDrivenTest {
 protected:
  std::unique_ptr<base::DictionaryValue> Deserialize(
      const base::FilePath& gltf_path);
};

class BinaryGltfParserTest : public DataDrivenTest {};

std::unique_ptr<base::DictionaryValue> GltfParserTest::Deserialize(
    const base::FilePath& gltf_path) {
  int error_code;
  std::string error_msg;
  JSONFileValueDeserializer json_deserializer(gltf_path);
  auto asset_value = json_deserializer.Deserialize(&error_code, &error_msg);
  EXPECT_NE(nullptr, asset_value);
  base::DictionaryValue* asset;
  EXPECT_TRUE(asset_value->GetAsDictionary(&asset));
  asset_value.release();
  return std::unique_ptr<base::DictionaryValue>(asset);
}

TEST_F(GltfParserTest, Parse) {
  auto asset = Deserialize(data_dir_.Append("sample_inline.gltf"));
  GltfParser parser;
  std::vector<std::unique_ptr<gltf::Buffer>> buffers;

  std::unique_ptr<gltf::Asset> gltf_model = parser.Parse(*asset, &buffers);
  EXPECT_TRUE(gltf_model);
  EXPECT_EQ(1u, buffers.size());

  const gltf::Buffer* buffer = buffers[0].get();
  EXPECT_EQ("HELLO WORLD!", *buffer);
  EXPECT_EQ(12u, buffer->length());

  const gltf::BufferView* buffer_view = gltf_model->GetBufferView(0);
  EXPECT_NE(nullptr, buffer_view);
  EXPECT_EQ(nullptr, gltf_model->GetBufferView(1));
  EXPECT_EQ(0, buffer_view->buffer);
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

TEST_F(GltfParserTest, ParseUnknownBuffer) {
  auto asset = Deserialize(data_dir_.Append("sample_inline.gltf"));
  GltfParser parser;
  std::vector<std::unique_ptr<gltf::Buffer>> buffers;

  // Parsing succeeds.
  EXPECT_NE(nullptr, parser.Parse(*asset, &buffers));

  // Parsing fails when a referenced buffer is removed.
  std::unique_ptr<base::Value> value;
  asset->Remove("buffers.dummyBuffer", &value);
  EXPECT_EQ(nullptr, parser.Parse(*asset, &buffers));

  // Parsing fails when the buffer is reinserted with a different ID.
  asset->Set("buffers.anotherDummyBuffer", std::move(value));
  EXPECT_EQ(nullptr, parser.Parse(*asset, &buffers));
}

TEST_F(GltfParserTest, ParseMissingRequired) {
  auto asset = Deserialize(data_dir_.Append("sample_inline.gltf"));
  GltfParser parser;
  std::vector<std::unique_ptr<gltf::Buffer>> buffers;

  std::unique_ptr<base::Value> value;
  asset->Remove("buffers.dummyBuffer.uri", &value);
  EXPECT_EQ(nullptr, parser.Parse(*asset, &buffers));
}

TEST_F(GltfParserTest, ParseExternal) {
  auto gltf_path = data_dir_.Append("sample_external.gltf");
  GltfParser parser;
  std::vector<std::unique_ptr<gltf::Buffer>> buffers;

  std::unique_ptr<gltf::Asset> gltf_model = parser.Parse(gltf_path, &buffers);
  EXPECT_NE(nullptr, gltf_model);
  EXPECT_EQ(1u, buffers.size());
  const gltf::Buffer* buffer = buffers[0].get();
  EXPECT_NE(nullptr, buffer);
  EXPECT_EQ("HELLO WORLD!", *buffer);
  EXPECT_EQ(12u, buffer->length());
}

TEST_F(GltfParserTest, ParseExternalNoPath) {
  auto asset = Deserialize(data_dir_.Append("sample_external.gltf"));
  GltfParser parser;
  std::vector<std::unique_ptr<gltf::Buffer>> buffers;

  // Parsing fails when no path is provided.
  EXPECT_EQ(nullptr, parser.Parse(*asset, &buffers));
}

TEST_F(BinaryGltfParserTest, ParseBinary) {
  std::string data;
  EXPECT_TRUE(base::ReadFileToString(data_dir_.Append("sample.glb"), &data));
  base::StringPiece glb_data(data);
  std::vector<std::unique_ptr<gltf::Buffer>> buffers;
  std::unique_ptr<gltf::Asset> asset =
      BinaryGltfParser::Parse(glb_data, &buffers);
  EXPECT_TRUE(asset);
  EXPECT_EQ(1u, buffers.size());
  const gltf::BufferView* buffer_view = asset->GetBufferView(0);
  EXPECT_TRUE(buffer_view);
  EXPECT_EQ(0, buffer_view->buffer);
}

}  // namespace vr
