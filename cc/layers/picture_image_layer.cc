// Copyright 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/picture_image_layer.h"

#include <stddef.h>

#include "cc/base/math_util.h"
#include "cc/layers/picture_layer_impl.h"
#include "cc/paint/paint_op_buffer.h"
#include "cc/trees/layer_tree_host.h"
#include "cc/trees/layer_tree_settings.h"

namespace cc {

scoped_refptr<PictureImageLayer> PictureImageLayer::Create() {
  return make_scoped_refptr(new PictureImageLayer());
}

PictureImageLayer::PictureImageLayer() : PictureLayer(this) {}

PictureImageLayer::~PictureImageLayer() {
  ClearClient();
}

std::unique_ptr<LayerImpl> PictureImageLayer::CreateLayerImpl(
    LayerTreeImpl* tree_impl) {
  auto layer_impl = PictureLayerImpl::Create(tree_impl, id(), mask_type());
  layer_impl->set_is_directly_composited_image(true);
  return std::move(layer_impl);
}

bool PictureImageLayer::HasDrawableContent() const {
  return image_.sk_image() && PictureLayer::HasDrawableContent();
}

void PictureImageLayer::SetImage(PaintImage image) {
  // SetImage() currently gets called whenever there is any
  // style change that affects the layer even if that change doesn't
  // affect the actual contents of the image (e.g. a CSS animation).
  // With this check in place we avoid unecessary texture uploads.
  if (image_ == image)
    return;

  image_ = std::move(image);
  UpdateDrawsContent(HasDrawableContent());
  SetNeedsDisplay();
}

gfx::Rect PictureImageLayer::PaintableRegion() {
  return gfx::Rect(bounds());
}

scoped_refptr<DisplayItemList> PictureImageLayer::PaintContentsToDisplayList(
    ContentLayerClient::PaintingControlSetting painting_control) {
  DCHECK(image_.sk_image());
  DCHECK_GT(image_.sk_image()->width(), 0);
  DCHECK_GT(image_.sk_image()->height(), 0);
  DCHECK(layer_tree_host());

  float content_to_layer_scale_x =
      static_cast<float>(bounds().width()) / image_.sk_image()->width();
  float content_to_layer_scale_y =
      static_cast<float>(bounds().height()) / image_.sk_image()->height();
  bool has_scale = !MathUtil::IsWithinEpsilon(content_to_layer_scale_x, 1.f) ||
                   !MathUtil::IsWithinEpsilon(content_to_layer_scale_y, 1.f);

  auto display_list = base::MakeRefCounted<DisplayItemList>();

  PaintOpBuffer* buffer = display_list->StartPaint();
  if (has_scale) {
    buffer->push<SaveOp>();
    buffer->push<ScaleOp>(content_to_layer_scale_x, content_to_layer_scale_y);
  }

  // Because Android WebView resourceless software draw mode rasters directly
  // to the root canvas, this draw must use the SkBlendMode::kSrcOver so that
  // transparent images blend correctly.
  buffer->push<DrawImageOp>(image_, 0.f, 0.f, nullptr);

  if (has_scale)
    buffer->push<RestoreOp>();
  display_list->EndPaintOfUnpaired(PaintableRegion());
  display_list->Finalize();
  return display_list;
}

bool PictureImageLayer::FillsBoundsCompletely() const {
  return false;
}

size_t PictureImageLayer::GetApproximateUnsharedMemoryUsage() const {
  return 0;
}

}  // namespace cc
