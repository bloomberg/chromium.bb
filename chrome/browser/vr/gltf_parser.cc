// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/gltf_parser.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback_helpers.h"
#include "base/files/file_util.h"
#include "base/json/json_file_value_serializer.h"
#include "base/json/json_string_value_serializer.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_util.h"
#include "base/sys_byteorder.h"
#include "net/base/data_url.h"
#include "net/base/filename_util.h"
#include "url/gurl.h"

namespace vr {

namespace {
constexpr const char kFailedtoReadBinaryGltfMsg[] =
    "Failed to read binary glTF: ";
constexpr const char kGltfMagic[] = "glTF";
constexpr const char kBinaryGltfBufferName[] = "binary_glTF";
constexpr uint32_t kJsonGltfFormat = 0;
constexpr size_t kVersionStart = 4;
constexpr size_t kLengthStart = 8;
constexpr size_t kContentLengthStart = 12;
constexpr size_t kContentFormatStart = 16;
constexpr size_t kContentStart = 20;

inline uint32_t GetLE32(const char* data) {
  return base::ByteSwapToLE32(*reinterpret_cast<const uint32_t*>(data));
}

}  // namespace

GltfParser::GltfParser() {}

GltfParser::~GltfParser() = default;

std::unique_ptr<gltf::Asset> GltfParser::Parse(
    const base::DictionaryValue& dict,
    std::vector<std::unique_ptr<gltf::Buffer>>* buffers,
    const base::FilePath& path) {
  DCHECK(buffers && buffers->size() <= 1);
  path_ = path;
  asset_ = base::MakeUnique<gltf::Asset>();

  base::ScopedClosureRunner runner(
      base::Bind(&GltfParser::Clear, base::Unretained(this)));

  if (!ParseInternal(dict, buffers))
    return nullptr;

  return std::move(asset_);
}

std::unique_ptr<gltf::Asset> GltfParser::Parse(
    const base::FilePath& gltf_path,
    std::vector<std::unique_ptr<gltf::Buffer>>* buffers) {
  DCHECK(buffers && buffers->size() <= 1);
  JSONFileValueDeserializer json_deserializer(gltf_path);
  int error_code;
  std::string error_msg;
  auto asset_value = json_deserializer.Deserialize(&error_code, &error_msg);
  if (!asset_value)
    return nullptr;
  base::DictionaryValue* asset;
  if (!asset_value->GetAsDictionary(&asset))
    return nullptr;
  return Parse(*asset, buffers, gltf_path);
}

bool GltfParser::ParseInternal(
    const base::DictionaryValue& dict,
    std::vector<std::unique_ptr<gltf::Buffer>>* buffers) {
  std::string gltf_version;
  if (!dict.GetString("asset.version", &gltf_version) || gltf_version != "1.0")
    return false;

  const base::DictionaryValue* sub_dict;
  if (dict.GetDictionary("buffers", &sub_dict) &&
      !SetBuffers(*sub_dict, buffers))
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

bool GltfParser::SetBuffers(
    const base::DictionaryValue& dict,
    std::vector<std::unique_ptr<gltf::Buffer>>* buffers) {
  size_t buffer_count = 0;
  for (base::DictionaryValue::Iterator it(dict); !it.IsAtEnd(); it.Advance()) {
    const base::DictionaryValue* buffer_dict;
    if (!it.value().GetAsDictionary(&buffer_dict))
      return false;

    if (it.key() == kBinaryGltfBufferName) {
      if (buffers->size() == buffer_count)
        return false;
      buffer_ids_[it.key()] = 0;
      continue;
    }

    std::string uri_str;
    if (!buffer_dict->GetString("uri", &uri_str))
      return false;
    auto buffer = ProcessUri(uri_str);
    if (!buffer)
      return false;

    int byte_length;
    if (buffer_dict->GetInteger("byteLength", &byte_length) &&
        static_cast<int>(buffer->length()) != byte_length)
      return false;

    buffer_ids_[it.key()] = buffers->size();
    buffers->push_back(std::move(buffer));
    ++buffer_count;
  }
  return true;
}

std::unique_ptr<gltf::Buffer> GltfParser::ProcessUri(
    const std::string& uri_str) {
  auto uri = path_.empty() ? GURL(uri_str)
                           : net::FilePathToFileURL(path_).Resolve(uri_str);
  if (!uri.is_valid())
    return nullptr;
  if (uri.SchemeIs(url::kDataScheme)) {
    std::string mime_type;
    std::string charset;
    auto data = base::MakeUnique<gltf::Buffer>();
    if (!net::DataURL::Parse(uri, &mime_type, &charset, data.get()))
      return nullptr;
    return data;
  }
  if (uri.SchemeIsFile()) {
    auto data = base::MakeUnique<gltf::Buffer>();
    if (!base::ReadFileToString(base::FilePath(uri.path()), data.get()))
      return nullptr;
    return data;
  }
  // No other schemes are supported yet.
  return nullptr;
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
    buffer_view->buffer = buffer_it->second;
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
        if (!primitive_value.GetAsDictionary(&primitive_dict))
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
        if (!mesh_value.GetAsString(&mesh_key))
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
        if (!mesh_value.GetAsString(&node_key))
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
        if (!node_value.GetAsString(&node_key))
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

void GltfParser::Clear() {
  asset_.reset();
  path_.clear();
  buffer_ids_.clear();
  buffer_view_ids_.clear();
  accessor_ids_.clear();
  node_ids_.clear();
  mesh_ids_.clear();
  scene_ids_.clear();
}

std::unique_ptr<gltf::Asset> BinaryGltfParser::Parse(
    base::StringPiece glb_content,
    std::vector<std::unique_ptr<gltf::Buffer>>* buffers,
    const base::FilePath& path) {
  DCHECK(buffers && buffers->empty());
  if (glb_content.length() < kContentStart) {
    DLOG(ERROR) << kFailedtoReadBinaryGltfMsg << "Incomplete data";
    return nullptr;
  }
  if (!glb_content.starts_with(kGltfMagic)) {
    DLOG(ERROR) << kFailedtoReadBinaryGltfMsg << "Unknown magic number";
    return nullptr;
  }
  if (GetLE32(glb_content.data() + kVersionStart) != 1) {
    DLOG(ERROR) << kFailedtoReadBinaryGltfMsg << "Unsupported version";
    return nullptr;
  }
  if (GetLE32(glb_content.data() + kLengthStart) != glb_content.length()) {
    DLOG(ERROR) << kFailedtoReadBinaryGltfMsg << "Incorrect file size";
    return nullptr;
  }
  uint32_t content_length = GetLE32(glb_content.data() + kContentLengthStart);
  if (kContentStart + content_length > glb_content.length()) {
    DLOG(ERROR) << kFailedtoReadBinaryGltfMsg << "Invalid content length";
    return nullptr;
  }
  if (GetLE32(glb_content.data() + kContentFormatStart) != kJsonGltfFormat) {
    DLOG(ERROR) << kFailedtoReadBinaryGltfMsg << "Unsupported glTF format";
    return nullptr;
  }

  base::StringPiece gltf_content =
      glb_content.substr(kContentStart, content_length);
  int error_code;
  std::string error_msg;
  JSONStringValueDeserializer json_deserializer(gltf_content);
  std::unique_ptr<base::Value> gltf_value =
      json_deserializer.Deserialize(&error_code, &error_msg);
  if (!gltf_value) {
    DLOG(ERROR) << kFailedtoReadBinaryGltfMsg
                << "Content not a valid JSON - Error " << error_code << " - "
                << error_msg;
    return nullptr;
  }
  base::DictionaryValue* gltf_dict;
  if (!gltf_value->GetAsDictionary(&gltf_dict)) {
    DLOG(ERROR) << kFailedtoReadBinaryGltfMsg << "Content is not a JSON object";
    return nullptr;
  }

  auto glb_buffer = base::MakeUnique<gltf::Buffer>(
      glb_content.substr(kContentStart + content_length));
  buffers->push_back(std::move(glb_buffer));
  GltfParser gltf_parser;
  std::unique_ptr<gltf::Asset> gltf_asset =
      gltf_parser.Parse(*gltf_dict, buffers);
  if (!gltf_asset) {
    DLOG(ERROR) << kFailedtoReadBinaryGltfMsg << "Content is not a valid glTF";
    buffers->clear();
    return nullptr;
  }
  return gltf_asset;
}

}  // namespace vr
