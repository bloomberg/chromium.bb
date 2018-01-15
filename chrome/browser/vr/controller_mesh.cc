// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/controller_mesh.h"

#include "base/trace_event/trace_event.h"
#include "chrome/browser/vr/gltf_parser.h"
#include "chrome/grit/vr_shell_resources.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkRect.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "third_party/skia/include/core/SkSwizzle.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/codec/png_codec.h"

namespace vr {

namespace {

enum {
  ELEMENTS_BUFFER_ID = 0,
  INDICES_BUFFER_ID = 1,
};

constexpr char kPosition[] = "POSITION";
constexpr char kTexCoord[] = "TEXCOORD_0";

const int kTexturePatchesResources[] = {
    -1, IDR_VR_SHELL_DDCONTROLLER_TOUCHPAD_PATCH,
    IDR_VR_SHELL_DDCONTROLLER_APP_PATCH, IDR_VR_SHELL_DDCONTROLLER_SYSTEM_PATCH,
};
const gfx::Point kPatchesLocations[] = {{}, {5, 5}, {47, 165}, {47, 234}};

sk_sp<SkImage> LoadPng(int resource_id) {
  base::StringPiece data =
      ui::ResourceBundle::GetSharedInstance().GetRawDataResource(resource_id);

  SkBitmap bitmap;
  bool decoded =
      gfx::PNGCodec::Decode(reinterpret_cast<const unsigned char*>(data.data()),
                            data.size(), &bitmap);
  DCHECK(decoded);
  if (bitmap.colorType() == kBGRA_8888_SkColorType) {
    SkSwapRB(bitmap.getAddr32(0, 0), bitmap.getAddr32(0, 0),
             bitmap.width() * bitmap.height());
  } else {
    DCHECK(bitmap.colorType() == kRGBA_8888_SkColorType);
  }
  return SkImage::MakeFromBitmap(bitmap);
}

}  // namespace

ControllerMesh::ControllerMesh(
    std::unique_ptr<vr::gltf::Asset> gltf_asset,
    std::vector<std::unique_ptr<vr::gltf::Buffer>> buffers)
    : gltf_asset_(std::move(gltf_asset)), buffers_(std::move(buffers)) {}

ControllerMesh::~ControllerMesh() = default;

const GLvoid* ControllerMesh::ElementsBuffer() const {
  const vr::gltf::BufferView* buffer_view =
      gltf_asset_->GetBufferView(ELEMENTS_BUFFER_ID);
  DCHECK(buffer_view && buffer_view->target == GL_ARRAY_BUFFER);
  const char* buffer = Buffer();
  return buffer ? buffer + buffer_view->byte_offset : nullptr;
}

GLsizeiptr ControllerMesh::ElementsBufferSize() const {
  const vr::gltf::BufferView* buffer_view =
      gltf_asset_->GetBufferView(ELEMENTS_BUFFER_ID);
  DCHECK(buffer_view && buffer_view->target == GL_ARRAY_BUFFER);
  return buffer_view->byte_length;
}

const GLvoid* ControllerMesh::IndicesBuffer() const {
  const vr::gltf::BufferView* buffer_view =
      gltf_asset_->GetBufferView(INDICES_BUFFER_ID);
  DCHECK(buffer_view && buffer_view->target == GL_ELEMENT_ARRAY_BUFFER);
  const char* buffer = Buffer();
  return buffer ? buffer + buffer_view->byte_offset : nullptr;
}

GLsizeiptr ControllerMesh::IndicesBufferSize() const {
  const vr::gltf::BufferView* buffer_view =
      gltf_asset_->GetBufferView(INDICES_BUFFER_ID);
  DCHECK(buffer_view && buffer_view->target == GL_ELEMENT_ARRAY_BUFFER);
  return buffer_view->byte_length;
}

GLenum ControllerMesh::DrawMode() const {
  const vr::gltf::Mesh* mesh = gltf_asset_->GetMesh(0);
  DCHECK(mesh && mesh->primitives.size());
  return mesh->primitives[0]->mode;
}

const vr::gltf::Accessor* ControllerMesh::IndicesAccessor() const {
  const vr::gltf::Mesh* mesh = gltf_asset_->GetMesh(0);
  DCHECK(mesh && mesh->primitives.size());
  return mesh->primitives[0]->indices;
}

const vr::gltf::Accessor* ControllerMesh::PositionAccessor() const {
  return Accessor(kPosition);
}

const vr::gltf::Accessor* ControllerMesh::TextureCoordinateAccessor() const {
  return Accessor(kTexCoord);
}

void ControllerMesh::SetBaseTexture(sk_sp<SkImage> image) {
  base_texture_ = image;
}

void ControllerMesh::SetTexture(int state, sk_sp<SkImage> patch) {
  DCHECK(state >= 0 && state < STATE_COUNT);
  if (!patch) {
    textures_[state] = base_texture_;
    return;
  }
  sk_sp<SkSurface> surface = SkSurface::MakeRasterN32Premul(
      base_texture_->width(), base_texture_->height());
  SkCanvas* canvas = surface->getCanvas();
  canvas->drawImage(base_texture_, 0, 0);
  SkPaint paint;
  paint.setBlendMode(SkBlendMode::kSrc);
  canvas->drawImage(patch, kPatchesLocations[state].x(),
                    kPatchesLocations[state].y(), &paint);
  textures_[state] = sk_sp<SkImage>(surface->makeImageSnapshot());
}

sk_sp<SkImage> ControllerMesh::GetTexture(int state) const {
  DCHECK(state >= 0 && state < STATE_COUNT);
  return textures_[state];
}

const char* ControllerMesh::Buffer() const {
  if (buffers_.empty())
    return nullptr;
  return buffers_[0]->data();
}

const vr::gltf::Accessor* ControllerMesh::Accessor(
    const std::string& key) const {
  const vr::gltf::Mesh* mesh = gltf_asset_->GetMesh(0);
  DCHECK(mesh && mesh->primitives.size());
  auto it = mesh->primitives[0]->attributes.find(key);
  DCHECK(it != mesh->primitives[0]->attributes.begin());
  return it->second;
}

std::unique_ptr<ControllerMesh> ControllerMesh::LoadFromResources() {
  TRACE_EVENT0("gpu", "ControllerMesh::LoadFromResources");
  std::vector<std::unique_ptr<vr::gltf::Buffer>> buffers;
  auto model_data = ui::ResourceBundle::GetSharedInstance().GetRawDataResource(
      IDR_VR_SHELL_DDCONTROLLER_MODEL);
  std::unique_ptr<vr::gltf::Asset> asset =
      vr::BinaryGltfParser::Parse(model_data, &buffers);
  DCHECK(asset);

  auto controller_model =
      std::make_unique<ControllerMesh>(std::move(asset), std::move(buffers));
  sk_sp<SkImage> base_texture = LoadPng(IDR_VR_SHELL_DDCONTROLLER_IDLE_TEXTURE);
  controller_model->SetBaseTexture(std::move(base_texture));

  for (int i = 0; i < ControllerMesh::STATE_COUNT; i++) {
    if (kTexturePatchesResources[i] == -1) {
      controller_model->SetTexture(i, nullptr);
    } else {
      auto patch_image = LoadPng(kTexturePatchesResources[i]);
      controller_model->SetTexture(i, patch_image);
    }
  }

  return controller_model;
}

}  // namespace vr
