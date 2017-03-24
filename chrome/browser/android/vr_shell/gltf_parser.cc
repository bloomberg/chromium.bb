// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/vr_shell/gltf_parser.h"

#include "base/base64.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"

namespace vr_shell {

constexpr char kBase64Header[] = "data:application/octet-stream;base64,";
constexpr size_t kBase64HeaderSize = 37;

GltfParser::GltfParser() {}

GltfParser::~GltfParser() = default;

std::unique_ptr<gltf::Asset> GltfParser::Parse(
    const base::DictionaryValue& dict) {
  std::string gltf_version;
  if (!dict.GetString("asset.version", &gltf_version) || gltf_version != "1.0")
    return nullptr;

  asset_ = base::MakeUnique<gltf::Asset>();

  if (!ParseInternal(dict)) {
    asset_.reset();
    return nullptr;
  }

  return std::move(asset_);
}

bool GltfParser::ParseInternal(const base::DictionaryValue& dict) {
  const base::DictionaryValue* sub_dict;
  if (dict.GetDictionary("buffers", &sub_dict) && !SetBuffers(*sub_dict))
    return false;
  if (dict.GetDictionary("bufferViews", &sub_dict) &&
      !SetBufferViews(*sub_dict))
    return false;
  if (dict.GetDictionary("accessors", &sub_dict) && !SetAccessors(*sub_dict))
    return false;
  if (dict.GetDictionary("meshes", &sub_dict) && !SetMeshes(*sub_dict))
    return false;
  if (dict.GetDictionary("nodes", &sub_dict) && !SetNodes(*sub_dict))
    return false;
  if (dict.GetDictionary("scenes", &sub_dict) && !SetScenes(*sub_dict))
    return false;

  std::string scene_key;
  if (dict.GetString("scene", &scene_key)) {
    auto scene_it = scene_ids_.find(scene_key);
    if (scene_it == scene_ids_.end())
      return false;
    asset_->SetMainScene(asset_->GetScene(scene_it->second));
  }

  return true;
}

bool GltfParser::SetBuffers(const base::DictionaryValue& dict) {
  for (base::DictionaryValue::Iterator it(dict); !it.IsAtEnd(); it.Advance()) {
    const base::DictionaryValue* buffer_dict;
    if (!it.value().GetAsDictionary(&buffer_dict))
      return false;

    std::string uri;
    // TODO(acondor): Support files. Only inline data is supported now.
    if (!buffer_dict->GetString("uri", &uri) ||
        uri.substr(0, kBase64HeaderSize) != kBase64Header)
      return false;

    auto buffer = base::MakeUnique<gltf::Buffer>();
    if (!base::Base64Decode(uri.substr(kBase64HeaderSize), buffer.get()))
      return false;

    int byte_length;
    if (buffer_dict->GetInteger("byteLength", &byte_length) &&
        static_cast<int>(buffer->length()) != byte_length)
      return false;

    buffer_ids_[it.key()] = asset_->AddBuffer(std::move(buffer));
  }
  return true;
}

bool GltfParser::SetBufferViews(const base::DictionaryValue& dict) {
  for (base::DictionaryValue::Iterator it(dict); !it.IsAtEnd(); it.Advance()) {
    const base::DictionaryValue* buffer_view_dict;
    if (!it.value().GetAsDictionary(&buffer_view_dict))
      return false;

    auto buffer_view = base::MakeUnique<gltf::BufferView>();
    std::string buffer_key;
    if (!buffer_view_dict->GetString("buffer", &buffer_key))
      return false;
    auto buffer_it = buffer_ids_.find(buffer_key);
    if (buffer_it == buffer_ids_.end())
      return false;
    buffer_view->buffer = asset_->GetBuffer(buffer_it->second);
    if (!buffer_view_dict->GetInteger("byteOffset", &buffer_view->byte_offset))
      return false;
    buffer_view_dict->GetInteger("byteLength", &buffer_view->byte_length);
    buffer_view_dict->GetInteger("target", &buffer_view->target);

    buffer_view_ids_[it.key()] = asset_->AddBufferView(std::move(buffer_view));
  }
  return true;
}

bool GltfParser::SetAccessors(const base::DictionaryValue& dict) {
  for (base::DictionaryValue::Iterator it(dict); !it.IsAtEnd(); it.Advance()) {
    const base::DictionaryValue* accessor_dict;
    if (!it.value().GetAsDictionary(&accessor_dict))
      return false;

    auto accessor = base::MakeUnique<gltf::Accessor>();
    std::string buffer_view_key;
    std::string type_str;
    if (!accessor_dict->GetString("bufferView", &buffer_view_key))
      return false;
    auto buffer_view_it = buffer_view_ids_.find(buffer_view_key);
    if (buffer_view_it == buffer_view_ids_.end())
      return false;
    accessor->buffer_view = asset_->GetBufferView(buffer_view_it->second);
    if (!accessor_dict->GetInteger("byteOffset", &accessor->byte_offset))
      return false;
    accessor_dict->GetInteger("byteStride", &accessor->byte_stride);
    if (!accessor_dict->GetInteger("componentType", &accessor->component_type))
      return false;
    if (!accessor_dict->GetInteger("count", &accessor->count))
      return false;
    if (!accessor_dict->GetString("type", &type_str))
      return false;
    gltf::Type type = gltf::GetType(type_str);
    if (type == gltf::UNKNOWN)
      return false;
    accessor->type = type;

    accessor_ids_[it.key()] = asset_->AddAccessor(std::move(accessor));
  }
  return true;
}

bool GltfParser::SetMeshes(const base::DictionaryValue& dict) {
  for (base::DictionaryValue::Iterator it(dict); !it.IsAtEnd(); it.Advance()) {
    const base::DictionaryValue* mesh_dict;
    if (!it.value().GetAsDictionary(&mesh_dict))
      return false;

    auto mesh = base::MakeUnique<gltf::Mesh>();
    const base::ListValue* list;
    if (mesh_dict->GetList("primitives", &list)) {
      for (const auto& primitive_value : *list) {
        const base::DictionaryValue* primitive_dict;
        if (!primitive_value->GetAsDictionary(&primitive_dict))
          return false;

        auto primitive = ProcessPrimitive(*primitive_dict);
        if (!primitive)
          return false;
        mesh->primitives.push_back(std::move(primitive));
      }
    }

    mesh_ids_[it.key()] = asset_->AddMesh(std::move(mesh));
  }
  return true;
}

std::unique_ptr<gltf::Mesh::Primitive> GltfParser::ProcessPrimitive(
    const base::DictionaryValue& dict) {
  auto primitive = base::MakeUnique<gltf::Mesh::Primitive>();
  std::string indices_key;
  const base::DictionaryValue* attributes;
  if (dict.GetString("indices", &indices_key)) {
    auto accessor_it = accessor_ids_.find(indices_key);
    if (accessor_it == accessor_ids_.end())
      return nullptr;
    primitive->indices = asset_->GetAccessor(accessor_it->second);
  }
  dict.GetInteger("mode", &primitive->mode);
  if (dict.GetDictionary("attributes", &attributes)) {
    for (base::DictionaryValue::Iterator it(*attributes); !it.IsAtEnd();
         it.Advance()) {
      std::string accessor_key;
      if (!it.value().GetAsString(&accessor_key))
        return nullptr;
      auto accessor_it = accessor_ids_.find(accessor_key);
      if (accessor_it == accessor_ids_.end())
        return nullptr;
      primitive->attributes[it.key()] =
          asset_->GetAccessor(accessor_it->second);
    }
  }
  return primitive;
}

bool GltfParser::SetNodes(const base::DictionaryValue& dict) {
  std::unordered_map<std::string, gltf::Node*> nodes;
  for (base::DictionaryValue::Iterator it(dict); !it.IsAtEnd(); it.Advance()) {
    const base::DictionaryValue* node_dict;
    if (!it.value().GetAsDictionary(&node_dict))
      return false;

    auto node = base::MakeUnique<gltf::Node>();
    const base::ListValue* list;
    if (node_dict->GetList("meshes", &list)) {
      std::string mesh_key;
      for (const auto& mesh_value : *list) {
        if (!mesh_value->GetAsString(&mesh_key))
          return false;
        auto mesh_it = mesh_ids_.find(mesh_key);
        if (mesh_it == mesh_ids_.end())
          return false;
        node->meshes.push_back(asset_->GetMesh(mesh_it->second));
      }
    }

    nodes[it.key()] = node.get();
    node_ids_[it.key()] = asset_->AddNode(std::move(node));
  }

  // Processing children after all nodes have been added to the asset.
  for (base::DictionaryValue::Iterator it(dict); !it.IsAtEnd(); it.Advance()) {
    const base::DictionaryValue* node_dict;
    it.value().GetAsDictionary(&node_dict);

    gltf::Node* node = nodes[it.key()];
    const base::ListValue* list;
    if (node_dict->GetList("children", &list)) {
      std::string node_key;
      for (const auto& mesh_value : *list) {
        if (!mesh_value->GetAsString(&node_key))
          return false;
        auto node_it = nodes.find(node_key);
        if (node_it == nodes.end())
          return false;
        node->children.push_back(node_it->second);
      }
    }
  }

  return true;
}

bool GltfParser::SetScenes(const base::DictionaryValue& dict) {
  for (base::DictionaryValue::Iterator it(dict); !it.IsAtEnd(); it.Advance()) {
    const base::DictionaryValue* scene_dict;
    if (!it.value().GetAsDictionary(&scene_dict))
      return false;

    auto scene = base::MakeUnique<gltf::Scene>();
    const base::ListValue* list;
    if (scene_dict->GetList("nodes", &list)) {
      std::string node_key;
      for (const auto& node_value : *list) {
        if (!node_value->GetAsString(&node_key))
          return false;
        auto node_it = node_ids_.find(node_key);
        if (node_it == node_ids_.end())
          return false;
        scene->nodes.push_back(asset_->GetNode(node_it->second));
      }
    }

    scene_ids_[it.key()] = asset_->AddScene(std::move(scene));
  }
  return true;
}

}  // namespace vr_shell
