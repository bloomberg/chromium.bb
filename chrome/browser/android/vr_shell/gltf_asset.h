// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_VR_SHELL_GLTF_ASSET_H_
#define CHROME_BROWSER_ANDROID_VR_SHELL_GLTF_ASSET_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "ui/gl/gl_bindings.h"

namespace vr_shell {

namespace gltf {

enum Type {
  UNKNOWN = 0,
  SCALAR,
  VEC2,
  VEC3,
  VEC4,
  MAT2,
  MAT3,
  MAT4,
};

Type GetType(const std::string& type);

// A Buffer is data stored as binary blob in little-endian format.
using Buffer = std::string;

// The following structures match the objects defined in glTF 1.0 standard.
// https://github.com/KhronosGroup/glTF/tree/master/specification/1.0#properties-reference

// A BufferView is subset of data in a buffer.
struct BufferView {
  const Buffer* buffer;
  int byte_length = 0;
  int byte_offset;
  int target = GL_ARRAY_BUFFER;
};

// An Accessor is a typed view into a BufferView.
struct Accessor {
  const BufferView* buffer_view;
  int byte_offset;
  // TODO(acondor): byte_stride is on BufferView in glTF 2.0.
  int byte_stride = 0;
  int component_type;
  int count;
  Type type;
};

// A Mesh is a set of primitives to be rendered.
struct Mesh {
  // A Primitive describes a geometry to be rendered.
  struct Primitive {
    std::map<std::string, const Accessor*> attributes;
    const Accessor* indices;
    int mode;
    Primitive();
    ~Primitive();
  };

  std::vector<std::unique_ptr<Primitive>> primitives;
  Mesh();
  ~Mesh();
};

// A Node in the node hierarchy.
struct Node {
  std::vector<const Node*> children;
  // TODO(acondor): There is only one mesh per node in glTF 2.0.
  std::vector<const Mesh*> meshes;
  Node();
  ~Node();
};

// The root nodes of a scene.
struct Scene {
  std::vector<const Node*> nodes;
  Scene();
  ~Scene();
};

class Asset {
 public:
  Asset();
  virtual ~Asset();

  std::size_t AddBuffer(std::unique_ptr<Buffer> buffer);
  std::size_t AddBufferView(std::unique_ptr<BufferView> buffer_view);
  std::size_t AddAccessor(std::unique_ptr<Accessor> accessor);
  std::size_t AddMesh(std::unique_ptr<Mesh> mesh);
  std::size_t AddNode(std::unique_ptr<Node> node);
  std::size_t AddScene(std::unique_ptr<Scene> scene);
  const Buffer* GetBuffer(std::size_t id) const;
  const BufferView* GetBufferView(std::size_t id) const;
  const Accessor* GetAccessor(std::size_t id) const;
  const Mesh* GetMesh(std::size_t id) const;
  const Node* GetNode(std::size_t id) const;
  const Scene* GetScene(std::size_t id) const;

  const Scene* GetMainScene() const { return scene_; }

  void SetMainScene(const Scene* scene) { scene_ = scene; }

 private:
  std::vector<std::unique_ptr<Buffer>> buffers_;
  std::vector<std::unique_ptr<BufferView>> buffer_views_;
  std::vector<std::unique_ptr<Accessor>> accessors_;
  std::vector<std::unique_ptr<Mesh>> meshes_;
  std::vector<std::unique_ptr<Node>> nodes_;
  std::vector<std::unique_ptr<Scene>> scenes_;
  const Scene* scene_;
};

}  // namespace gltf

}  // namespace vr_shell

#endif  // CHROME_BROWSER_ANDROID_VR_SHELL_GLTF_ASSET_H_
