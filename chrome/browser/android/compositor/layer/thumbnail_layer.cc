// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/compositor/layer/thumbnail_layer.h"

#include "cc/layers/ui_resource_layer.h"
#include "chrome/browser/android/thumbnail/thumbnail.h"
#include "content/public/browser/android/compositor.h"
#include "ui/gfx/geometry/size_conversions.h"

namespace chrome {
namespace android {

// static
scoped_refptr<ThumbnailLayer> ThumbnailLayer::Create() {
  return make_scoped_refptr(new ThumbnailLayer());
}

void ThumbnailLayer::SetThumbnail(Thumbnail* thumbnail) {
  layer_->SetUIResourceId(thumbnail->ui_resource_id());
  UpdateSizes(thumbnail->scaled_content_size(), thumbnail->scaled_data_size());
}

void ThumbnailLayer::Clip(const gfx::Rect& clipping) {
  last_clipping_ = clipping;
  gfx::Size clipped_content = gfx::Size(content_size_.width() - clipping.x(),
                                        content_size_.height() - clipping.y());
  clipped_content.SetToMin(clipping.size());
  layer_->SetBounds(clipped_content);

  layer_->SetUV(
      gfx::PointF(clipping.x() / resource_size_.width(),
                  clipping.y() / resource_size_.height()),
      gfx::PointF(
          (clipping.x() + clipped_content.width()) / resource_size_.width(),
          (clipping.y() + clipped_content.height()) / resource_size_.height()));
}

void ThumbnailLayer::AddSelfToParentOrReplaceAt(scoped_refptr<cc::Layer> parent,
                                                size_t index) {
  if (index >= parent->children().size())
    parent->AddChild(layer_);
  else if (parent->child_at(index)->id() != layer_->id())
    parent->ReplaceChild(parent->child_at(index), layer_);
}

scoped_refptr<cc::Layer> ThumbnailLayer::layer() {
  return layer_;
}

ThumbnailLayer::ThumbnailLayer()
    : layer_(
          cc::UIResourceLayer::Create(content::Compositor::LayerSettings())) {
  layer_->SetIsDrawable(true);
}

ThumbnailLayer::~ThumbnailLayer() {
}

void ThumbnailLayer::UpdateSizes(const gfx::SizeF& content_size,
                                 const gfx::SizeF& resource_size) {
  if (content_size != content_size_ || resource_size != resource_size_) {
    content_size_ = content_size;
    resource_size_ = resource_size;
    Clip(last_clipping_);
  }
}

}  // namespace android
}  // namespace chrome
