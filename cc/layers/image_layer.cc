// Copyright 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/image_layer.h"

#include "base/compiler_specific.h"
#include "cc/resources/image_layer_updater.h"
#include "cc/resources/layer_updater.h"
#include "cc/resources/prioritized_resource.h"
#include "cc/resources/resource_update_queue.h"
#include "cc/trees/layer_tree_host.h"

namespace cc {

scoped_refptr<ImageLayer> ImageLayer::Create() {
  return make_scoped_refptr(new ImageLayer());
}

ImageLayer::ImageLayer() : TiledLayer() {}

ImageLayer::~ImageLayer() {}

void ImageLayer::SetBitmap(const SkBitmap& bitmap) {
  // SetBitmap() currently gets called whenever there is any
  // style change that affects the layer even if that change doesn't
  // affect the actual contents of the image (e.g. a CSS animation).
  // With this check in place we avoid unecessary texture uploads.
  if (bitmap.pixelRef() && bitmap.pixelRef() == bitmap_.pixelRef())
    return;

  bitmap_ = bitmap;
  SetNeedsDisplay();
}

void ImageLayer::SetTexturePriorities(const PriorityCalculator& priority_calc) {
  // Update the tile data before creating all the layer's tiles.
  UpdateTileSizeAndTilingOption();

  TiledLayer::SetTexturePriorities(priority_calc);
}

void ImageLayer::Update(ResourceUpdateQueue* queue,
                        const OcclusionTracker* occlusion) {
  CreateUpdaterIfNeeded();
  if (needs_display_) {
    updater_->set_bitmap(bitmap_);
    UpdateTileSizeAndTilingOption();
    InvalidateContentRect(gfx::Rect(content_bounds()));
    needs_display_ = false;
  }
  TiledLayer::Update(queue, occlusion);
}

void ImageLayer::CreateUpdaterIfNeeded() {
  if (updater_)
    return;

  updater_ = ImageLayerUpdater::Create();
  GLenum texture_format =
      layer_tree_host()->GetRendererCapabilities().best_texture_format;
  SetTextureFormat(texture_format);
}

LayerUpdater* ImageLayer::Updater() const {
  return updater_.get();
}

void ImageLayer::CalculateContentsScale(float ideal_contents_scale,
                                        bool animating_transform_to_screen,
                                        float* contents_scale_x,
                                        float* contents_scale_y,
                                        gfx::Size* content_bounds) {
  *contents_scale_x = ImageContentsScaleX();
  *contents_scale_y = ImageContentsScaleY();
  *content_bounds = gfx::Size(bitmap_.width(), bitmap_.height());
}

bool ImageLayer::DrawsContent() const {
  return !bitmap_.isNull() && TiledLayer::DrawsContent();
}

float ImageLayer::ImageContentsScaleX() const {
  if (bounds().IsEmpty() || bitmap_.width() == 0)
    return 1;
  return static_cast<float>(bitmap_.width()) / bounds().width();
}

float ImageLayer::ImageContentsScaleY() const {
  if (bounds().IsEmpty() || bitmap_.height() == 0)
    return 1;
  return static_cast<float>(bitmap_.height()) / bounds().height();
}

}  // namespace cc
