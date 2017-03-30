// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_VR_SHELL_GLTF_PARSER_H_
#define CHROME_BROWSER_ANDROID_VR_SHELL_GLTF_PARSER_H_

#include <memory>
#include <string>
#include <unordered_map>

#include "base/files/file_path.h"
#include "base/values.h"
#include "chrome/browser/android/vr_shell/gltf_asset.h"

namespace vr_shell {

// Parser for glTF 1.0 specification
// https://github.com/KhronosGroup/glTF/tree/master/specification/1.0
// Supported objects are limited to: buffers, bufferViews, accessors,
// meshes (limited), nodes (limited), scenes (limited).
// Non-supported objects include: materials, techniques, skins, shaders,
// programs, animations, cameras, images, textures, extensions.
// This parser is not intended to be used on user or third-party data,
// but only on thoroughly tested Chromium resource files.
// TODO(acondor): Implement glTF 2.0 parser. gltf::Asset is mostly version
// agnostic.
class GltfParser {
 public:
  GltfParser();
  ~GltfParser();
  // Note: If your glTF references external files, this function will perform
  // IO, and a base path must be specified.
  std::unique_ptr<gltf::Asset> Parse(
      const base::DictionaryValue& dict,
      std::vector<std::unique_ptr<gltf::Buffer>>* buffers,
      const base::FilePath& path = base::FilePath());
  // Note: This function will perform IO.
  std::unique_ptr<gltf::Asset> Parse(
      const base::FilePath& gltf_path,
      std::vector<std::unique_ptr<gltf::Buffer>>* buffers);

 private:
  bool ParseInternal(const base::DictionaryValue& dict,
                     std::vector<std::unique_ptr<gltf::Buffer>>* buffers);
  bool SetBuffers(const base::DictionaryValue& dict,
                  std::vector<std::unique_ptr<gltf::Buffer>>* buffers);
  bool SetBufferViews(const base::DictionaryValue& dict);
  bool SetAccessors(const base::DictionaryValue& dict);
  bool SetMeshes(const base::DictionaryValue& dict);
  bool SetNodes(const base::DictionaryValue& dict);
  bool SetScenes(const base::DictionaryValue& dict);
  std::unique_ptr<gltf::Mesh::Primitive> ProcessPrimitive(
      const base::DictionaryValue& dict);
  std::unique_ptr<gltf::Buffer> ProcessUri(const std::string& uri_str);
  void Clear();

  std::unique_ptr<gltf::Asset> asset_;
  base::FilePath path_;
  std::unordered_map<std::string, std::size_t> buffer_ids_;
  std::unordered_map<std::string, std::size_t> buffer_view_ids_;
  std::unordered_map<std::string, std::size_t> accessor_ids_;
  std::unordered_map<std::string, std::size_t> node_ids_;
  std::unordered_map<std::string, std::size_t> mesh_ids_;
  std::unordered_map<std::string, std::size_t> scene_ids_;

  DISALLOW_COPY_AND_ASSIGN(GltfParser);
};

}  // namespace vr_shell

#endif  // CHROME_BROWSER_ANDROID_VR_SHELL_GLTF_PARSER_H_
