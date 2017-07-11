// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/gltf_asset.h"

#include <unordered_map>

#include "base/logging.h"

namespace vr {

namespace gltf {

namespace {

const std::unordered_map<std::string, Type> kTypeMap = {
    {"SCALAR", SCALAR}, {"VEC2", VEC2}, {"VEC3", VEC3}, {"VEC4", VEC4},
    {"MAT2", MAT2},     {"MAT3", MAT3}, {"MAT4", MAT4},
};

const std::vector<int> kTypeComponents = {
    0,
    1,   // SCALAR
    2,   // VEC2
    3,   // VEC3
    4,   // VEC4
    4,   // MAT2
    9,   // MAT3
    16,  // MAT4
};

}  // namespace

Type GetType(const std::string& type) {
  auto it = kTypeMap.find(type);
  if (it == kTypeMap.end())
    return UNKNOWN;
  return it->second;
}

GLint GetTypeComponents(Type type) {
  return kTypeComponents[type];
}

Mesh::Primitive::Primitive() : indices(nullptr), mode(4) {}

Mesh::Primitive::~Primitive() = default;

Mesh::Mesh() {}

Mesh::~Mesh() = default;

Node::Node() {}

Node::~Node() = default;

Scene::Scene() {}

Scene::~Scene() = default;

Asset::Asset() : scene_(nullptr) {}

Asset::~Asset() = default;

std::size_t Asset::AddBufferView(std::unique_ptr<BufferView> buffer_view) {
  auto index = buffer_views_.size();
  buffer_views_.push_back(std::move(buffer_view));
  return index;
}

std::size_t Asset::AddAccessor(std::unique_ptr<Accessor> accessor) {
  auto index = accessors_.size();
  accessors_.push_back(std::move(accessor));
  return index;
}

std::size_t Asset::AddMesh(std::unique_ptr<Mesh> mesh) {
  auto index = meshes_.size();
  meshes_.push_back(std::move(mesh));
  return index;
}

std::size_t Asset::AddNode(std::unique_ptr<Node> node) {
  auto index = nodes_.size();
  nodes_.push_back(std::move(node));
  return index;
}

std::size_t Asset::AddScene(std::unique_ptr<Scene> scene) {
  auto index = scenes_.size();
  scenes_.push_back(std::move(scene));
  return index;
}

const BufferView* Asset::GetBufferView(std::size_t id) const {
  return id < buffer_views_.size() ? buffer_views_[id].get() : nullptr;
}

const Accessor* Asset::GetAccessor(std::size_t id) const {
  return id < accessors_.size() ? accessors_[id].get() : nullptr;
}

const Mesh* Asset::GetMesh(std::size_t id) const {
  return id < meshes_.size() ? meshes_[id].get() : nullptr;
}

const Node* Asset::GetNode(std::size_t id) const {
  return id < nodes_.size() ? nodes_[id].get() : nullptr;
}

const Scene* Asset::GetScene(std::size_t id) const {
  return id < scenes_.size() ? scenes_[id].get() : nullptr;
}

}  // namespace gltf

}  // namespace vr
