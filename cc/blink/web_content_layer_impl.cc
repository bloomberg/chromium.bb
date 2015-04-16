// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/blink/web_content_layer_impl.h"

#include "cc/blink/web_display_item_list_impl.h"
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

static blink::WebContentLayerClient::PaintingControlSetting
PaintingControlToWeb(
    cc::ContentLayerClient::PaintingControlSetting painting_control) {
  switch (painting_control) {
    case cc::ContentLayerClient::PAINTING_BEHAVIOR_NORMAL:
      return blink::WebContentLayerClient::PaintDefaultBehavior;
    case cc::ContentLayerClient::DISPLAY_LIST_CONSTRUCTION_DISABLED:
      return blink::WebContentLayerClient::DisplayListConstructionDisabled;
    case cc::ContentLayerClient::DISPLAY_LIST_CACHING_DISABLED:
      return blink::WebContentLayerClient::DisplayListCachingDisabled;
  }
  NOTREACHED();
  return blink::WebContentLayerClient::PaintDefaultBehavior;
}

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
    cc::ContentLayerClient::PaintingControlSetting painting_control) {
  if (!client_)
    return;

  client_->paintContents(canvas, clip, PaintingControlToWeb(painting_control));
}

scoped_refptr<cc::DisplayItemList>
WebContentLayerImpl::PaintContentsToDisplayList(
    const gfx::Rect& clip,
    cc::ContentLayerClient::PaintingControlSetting painting_control) {
  if (!client_)
    return cc::DisplayItemList::Create();

  WebDisplayItemListImpl list;
  client_->paintContents(&list, clip, PaintingControlToWeb(painting_control));
  return list.ToDisplayItemList();
}

bool WebContentLayerImpl::FillsBoundsCompletely() const {
  return false;
}

}  // namespace cc_blink
