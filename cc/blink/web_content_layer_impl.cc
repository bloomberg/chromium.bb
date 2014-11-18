// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/blink/web_content_layer_impl.h"

#include "cc/layers/content_layer.h"
#include "cc/layers/picture_layer.h"
#include "third_party/WebKit/public/platform/WebContentLayerClient.h"
#include "third_party/WebKit/public/platform/WebFloatPoint.h"
#include "third_party/WebKit/public/platform/WebFloatRect.h"
#include "third_party/WebKit/public/platform/WebRect.h"
#include "third_party/WebKit/public/platform/WebSize.h"
#include "third_party/skia/include/utils/SkMatrix44.h"

using cc::ContentLayer;
using cc::PictureLayer;

namespace cc_blink {

WebContentLayerImpl::WebContentLayerImpl(blink::WebContentLayerClient* client)
    : client_(client) {
  if (WebLayerImpl::UsingPictureLayer())
    layer_ = make_scoped_ptr(new WebLayerImpl(PictureLayer::Create(this)));
  else
    layer_ = make_scoped_ptr(new WebLayerImpl(ContentLayer::Create(this)));
  layer_->layer()->SetIsDrawable(true);
}

WebContentLayerImpl::~WebContentLayerImpl() {
  if (WebLayerImpl::UsingPictureLayer())
    static_cast<PictureLayer*>(layer_->layer())->ClearClient();
  else
    static_cast<ContentLayer*>(layer_->layer())->ClearClient();
}

blink::WebLayer* WebContentLayerImpl::layer() {
  return layer_.get();
}

void WebContentLayerImpl::setDoubleSided(bool double_sided) {
  layer_->layer()->SetDoubleSided(double_sided);
}

void WebContentLayerImpl::setDrawCheckerboardForMissingTiles(bool enable) {
  layer_->layer()->SetDrawCheckerboardForMissingTiles(enable);
}

void WebContentLayerImpl::PaintContents(
    SkCanvas* canvas,
    const gfx::Rect& clip,
    ContentLayerClient::GraphicsContextStatus graphics_context_status) {
  if (!client_)
    return;

  // TODO(danakj): Stop passing this to blink it should always use LCD when it
  // wants to. crbug.com/430617
  bool can_use_lcd_text = true;
  client_->paintContents(
      canvas, clip, can_use_lcd_text,
      graphics_context_status == ContentLayerClient::GRAPHICS_CONTEXT_ENABLED
          ? blink::WebContentLayerClient::GraphicsContextEnabled
          : blink::WebContentLayerClient::GraphicsContextDisabled);
}

bool WebContentLayerImpl::FillsBoundsCompletely() const {
  return false;
}

}  // namespace cc_blink
