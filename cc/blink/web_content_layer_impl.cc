// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/blink/web_content_layer_impl.h"

#include <stddef.h>

#include "base/command_line.h"
#include "cc/base/switches.h"
#include "cc/layers/content_layer_client.h"
#include "cc/layers/picture_layer.h"
#include "third_party/blink/public/platform/web_float_point.h"
#include "third_party/blink/public/platform/web_float_rect.h"
#include "third_party/blink/public/platform/web_rect.h"
#include "third_party/blink/public/platform/web_size.h"
#include "third_party/skia/include/core/SkMatrix44.h"

using cc::PictureLayer;

namespace cc_blink {

WebContentLayerImpl::WebContentLayerImpl(cc::ContentLayerClient* client)
    : client_(client) {
  layer_ = std::make_unique<WebLayerImpl>(PictureLayer::Create(this));
}

WebContentLayerImpl::~WebContentLayerImpl() {
  static_cast<PictureLayer*>(layer_->layer())->ClearClient();
}

blink::WebLayer* WebContentLayerImpl::Layer() {
  return layer_.get();
}

void WebContentLayerImpl::SetTransformedRasterizationAllowed(bool allowed) {
  static_cast<PictureLayer*>(layer_->layer())
      ->SetTransformedRasterizationAllowed(allowed);
}

bool WebContentLayerImpl::TransformedRasterizationAllowed() const {
  return static_cast<PictureLayer*>(layer_->layer())
      ->transformed_rasterization_allowed();
}

gfx::Rect WebContentLayerImpl::PaintableRegion() {
  return client_->PaintableRegion();
}

scoped_refptr<cc::DisplayItemList>
WebContentLayerImpl::PaintContentsToDisplayList(
    PaintingControlSetting painting_control) {
  return client_->PaintContentsToDisplayList(painting_control);
}

bool WebContentLayerImpl::FillsBoundsCompletely() const {
  return client_->FillsBoundsCompletely();
}

size_t WebContentLayerImpl::GetApproximateUnsharedMemoryUsage() const {
  return client_->GetApproximateUnsharedMemoryUsage();
}

}  // namespace cc_blink
