// Copyright 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/picture_image_layer.h"

#include "cc/picture_image_layer_impl.h"
#include "third_party/skia/include/core/SkCanvas.h"

namespace cc {

scoped_refptr<PictureImageLayer> PictureImageLayer::create()
{
  return make_scoped_refptr(new PictureImageLayer());
}

PictureImageLayer::PictureImageLayer()
    : PictureLayer(this)
{
}

PictureImageLayer::~PictureImageLayer()
{
  clearClient();
}

scoped_ptr<LayerImpl> PictureImageLayer::createLayerImpl(
    LayerTreeImpl* treeImpl) {
  return PictureImageLayerImpl::create(treeImpl, id()).PassAs<LayerImpl>();
}

bool PictureImageLayer::drawsContent() const {
  return !bitmap_.isNull() && PictureLayer::drawsContent();
}

void PictureImageLayer::setBitmap(const SkBitmap& bitmap)
{
  // setBitmap() currently gets called whenever there is any
  // style change that affects the layer even if that change doesn't
  // affect the actual contents of the image (e.g. a CSS animation).
  // With this check in place we avoid unecessary texture uploads.
  if (bitmap.pixelRef() && bitmap.pixelRef() == bitmap_.pixelRef())
      return;

  bitmap_ = bitmap;
  setNeedsDisplay();
}

void PictureImageLayer::paintContents(
    SkCanvas* canvas,
    const gfx::Rect& clip,
    gfx::RectF& opaque) {
  if (!bitmap_.width() || !bitmap_.height())
    return;

  SkScalar content_to_layer_scale_x = SkFloatToScalar(
      static_cast<float>(bounds().width()) / bitmap_.width());
  SkScalar content_to_layer_scale_y = SkFloatToScalar(
      static_cast<float>(bounds().height()) / bitmap_.height());
  canvas->scale(content_to_layer_scale_x, content_to_layer_scale_y);

  canvas->drawBitmap(bitmap_, 0, 0);
}

}  // namespace cc
