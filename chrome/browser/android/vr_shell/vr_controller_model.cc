// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/vr_shell/vr_controller_model.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/memory/ptr_util.h"
#include "base/path_service.h"
#include "chrome/browser/android/vr_shell/gltf_parser.h"
#include "components/component_updater/component_updater_paths.h"
#include "ui/gfx/codec/png_codec.h"

namespace vr_shell {

namespace {

enum {
  ELEMENTS_BUFFER_ID = 0,
  INDICES_BUFFER_ID = 1,
};

constexpr char kPosition[] = "POSITION";
constexpr char kTexCoord[] = "TEXCOORD_0";

}  // namespace

constexpr char const VrControllerModel::kComponentName[];
constexpr char const VrControllerModel::kDefaultVersion[];
constexpr char const VrControllerModel::kModelsDirectory[];
constexpr char const VrControllerModel::kModelFilename[];
constexpr char const VrControllerModel::kTexturesDirectory[];
constexpr char const* VrControllerModel::kTextureFilenames[];

VrControllerModel::VrControllerModel(
    std::unique_ptr<gltf::Asset> gltf_asset,
    std::vector<std::unique_ptr<gltf::Buffer>> buffers)
    : gltf_asset_(std::move(gltf_asset)),
      texture_bitmaps_(State::STATE_COUNT),
      buffers_(std::move(buffers)) {}

VrControllerModel::~VrControllerModel() = default;

const GLvoid* VrControllerModel::ElementsBuffer() const {
  const gltf::BufferView* buffer_view =
      gltf_asset_->GetBufferView(ELEMENTS_BUFFER_ID);
  DCHECK(buffer_view && buffer_view->target == GL_ARRAY_BUFFER);
  const char* buffer = Buffer();
  return buffer ? buffer + buffer_view->byte_offset : nullptr;
}

GLsizeiptr VrControllerModel::ElementsBufferSize() const {
  const gltf::BufferView* buffer_view =
      gltf_asset_->GetBufferView(ELEMENTS_BUFFER_ID);
  DCHECK(buffer_view && buffer_view->target == GL_ARRAY_BUFFER);
  return buffer_view->byte_length;
}

const GLvoid* VrControllerModel::IndicesBuffer() const {
  const gltf::BufferView* buffer_view =
      gltf_asset_->GetBufferView(INDICES_BUFFER_ID);
  DCHECK(buffer_view && buffer_view->target == GL_ELEMENT_ARRAY_BUFFER);
  const char* buffer = Buffer();
  return buffer ? buffer + buffer_view->byte_offset : nullptr;
}

GLsizeiptr VrControllerModel::IndicesBufferSize() const {
  const gltf::BufferView* buffer_view =
      gltf_asset_->GetBufferView(INDICES_BUFFER_ID);
  DCHECK(buffer_view && buffer_view->target == GL_ELEMENT_ARRAY_BUFFER);
  return buffer_view->byte_length;
}

GLenum VrControllerModel::DrawMode() const {
  const gltf::Mesh* mesh = gltf_asset_->GetMesh(0);
  DCHECK(mesh && mesh->primitives.size());
  return mesh->primitives[0]->mode;
}

const gltf::Accessor* VrControllerModel::IndicesAccessor() const {
  const gltf::Mesh* mesh = gltf_asset_->GetMesh(0);
  DCHECK(mesh && mesh->primitives.size());
  return mesh->primitives[0]->indices;
}

const gltf::Accessor* VrControllerModel::PositionAccessor() const {
  return Accessor(kPosition);
}

const gltf::Accessor* VrControllerModel::TextureCoordinateAccessor() const {
  return Accessor(kTexCoord);
}

void VrControllerModel::SetTexture(int state,
                                   std::unique_ptr<SkBitmap> bitmap) {
  DCHECK(state >= 0 && state < STATE_COUNT);
  texture_bitmaps_[state] = std::move(bitmap);
}

const SkBitmap* VrControllerModel::GetTexture(int state) const {
  DCHECK(state >= 0 && state < STATE_COUNT);
  return texture_bitmaps_[state].get();
}

const char* VrControllerModel::Buffer() const {
  if (buffers_.empty())
    return nullptr;
  return buffers_[0]->data();
}

const gltf::Accessor* VrControllerModel::Accessor(
    const std::string& key) const {
  const gltf::Mesh* mesh = gltf_asset_->GetMesh(0);
  DCHECK(mesh && mesh->primitives.size());
  auto it = mesh->primitives[0]->attributes.find(key);
  DCHECK(it != mesh->primitives[0]->attributes.begin());
  return it->second;
}

std::unique_ptr<VrControllerModel> VrControllerModel::LoadFromComponent() {
  base::FilePath models_path;
  PathService::Get(component_updater::DIR_COMPONENT_USER, &models_path);
  models_path = models_path.Append(VrControllerModel::kComponentName)
                    .Append(VrControllerModel::kDefaultVersion)
                    .Append(VrControllerModel::kModelsDirectory);
  auto model_path = models_path.Append(VrControllerModel::kModelFilename);

  // No further action if model file is not present
  if (!base::PathExists(model_path)) {
    LOG(WARNING) << "Controller model files not found";
    return nullptr;
  }

  GltfParser gltf_parser;
  std::vector<std::unique_ptr<gltf::Buffer>> buffers;
  auto asset = gltf_parser.Parse(model_path, &buffers);
  if (!asset) {
    LOG(ERROR) << "Failed to read controller model";
    return nullptr;
  }

  auto controller_model =
      base::MakeUnique<VrControllerModel>(std::move(asset), std::move(buffers));

  auto textures_path =
      models_path.Append(VrControllerModel::kTexturesDirectory);

  for (int i = 0; i < VrControllerModel::STATE_COUNT; i++) {
    auto texture_path =
        textures_path.Append(VrControllerModel::kTextureFilenames[i]);
    std::string data;
    auto bitmap = base::MakeUnique<SkBitmap>();
    if (!base::ReadFileToString(texture_path, &data) ||
        !gfx::PNGCodec::Decode(
            reinterpret_cast<const unsigned char*>(data.data()), data.size(),
            bitmap.get()) ||
        bitmap->colorType() != kRGBA_8888_SkColorType) {
      LOG(ERROR) << "Failed to read controller texture";
      return nullptr;
    }
    controller_model->SetTexture(i, std::move(bitmap));
  }

  return controller_model;
}

}  // namespace vr_shell
