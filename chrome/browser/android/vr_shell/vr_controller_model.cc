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
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkRect.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "ui/gfx/codec/png_codec.h"

namespace vr_shell {

namespace {

enum {
  ELEMENTS_BUFFER_ID = 2,
  INDICES_BUFFER_ID = 3,
};

constexpr char kPosition[] = "POSITION";
constexpr char kTexCoord[] = "TEXCOORD_0";

// TODO(acondor): Remove these hardcoded paths once VrShell resources
// are delivered through component updater.
constexpr char const kComponentName[] = "VrShell";
constexpr char const kDefaultVersion[] = "0";

constexpr char const kModelsDirectory[] = "models";
constexpr char const kModelFilename[] = "ddcontroller.glb";
constexpr char const kTexturesDirectory[] = "tex";
constexpr char const kBaseTextureFilename[] = "ddcontroller_idle.png";
constexpr char const* kTexturePatchesFilenames[] = {
    "", "ddcontroller_touchpad.png", "ddcontroller_app.png",
    "ddcontroller_system.png",
};
const gfx::Point kPatchesLocations[] = {{}, {5, 5}, {47, 165}, {47, 234}};

sk_sp<SkImage> LoadPng(const base::FilePath& path) {
  std::string data;
  SkBitmap bitmap;
  if (!base::ReadFileToString(path, &data) ||
      !gfx::PNGCodec::Decode(
          reinterpret_cast<const unsigned char*>(data.data()), data.size(),
          &bitmap) ||
      bitmap.colorType() != kRGBA_8888_SkColorType) {
    return nullptr;
  }
  return SkImage::MakeFromBitmap(bitmap);
}

}  // namespace

VrControllerModel::VrControllerModel(
    std::unique_ptr<gltf::Asset> gltf_asset,
    std::vector<std::unique_ptr<gltf::Buffer>> buffers)
    : gltf_asset_(std::move(gltf_asset)),
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

void VrControllerModel::SetBaseTexture(sk_sp<SkImage> image) {
  base_texture_ = image;
}

void VrControllerModel::SetTexturePatch(int state, sk_sp<SkImage> image) {
  DCHECK(state >= 0 && state < STATE_COUNT);
  patches_[state] = image;
}

sk_sp<SkImage> VrControllerModel::GetTexture(int state) const {
  if (!patches_[state])
    return base_texture_;
  sk_sp<SkSurface> surface = SkSurface::MakeRasterN32Premul(
      base_texture_->width(), base_texture_->height());
  SkCanvas* canvas = surface->getCanvas();
  canvas->drawImage(base_texture_, 0, 0);
  SkPaint paint;
  paint.setBlendMode(SkBlendMode::kSrc);
  canvas->drawImage(patches_[state], kPatchesLocations[state].x(),
                    kPatchesLocations[state].y(), &paint);
  return sk_sp<SkImage>(surface->makeImageSnapshot());
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
  models_path = models_path.Append(kComponentName)
                    .Append(kDefaultVersion)
                    .Append(kModelsDirectory);
  auto model_path = models_path.Append(kModelFilename);

  // No further action if model file is not present
  if (!base::PathExists(model_path)) {
    LOG(WARNING) << "Controller model files not found";
    return nullptr;
  }

  std::vector<std::unique_ptr<gltf::Buffer>> buffers;

  std::string model_data;
  base::ReadFileToString(model_path, &model_data);
  auto asset = BinaryGltfParser::Parse(base::StringPiece(model_data), &buffers);
  if (!asset) {
    LOG(ERROR) << "Failed to read controller model";
    return nullptr;
  }

  auto controller_model =
      base::MakeUnique<VrControllerModel>(std::move(asset), std::move(buffers));

  auto textures_path = models_path.Append(kTexturesDirectory);

  auto base_texture = LoadPng(textures_path.Append(kBaseTextureFilename));
  if (!base_texture) {
    LOG(ERROR) << "Failed to read controller base texture";
    return nullptr;
  }
  controller_model->SetBaseTexture(std::move(base_texture));

  for (int i = 0; i < VrControllerModel::STATE_COUNT; i++) {
    if (!kTexturePatchesFilenames[i][0])
      continue;
    auto patch_image =
        LoadPng(textures_path.Append(kTexturePatchesFilenames[i]));
    if (!patch_image) {
      LOG(ERROR) << "Failed to read controller texture patch";
      continue;
    }
    controller_model->SetTexturePatch(i, patch_image);
  }

  return controller_model;
}

}  // namespace vr_shell
