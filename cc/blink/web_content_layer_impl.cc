// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/blink/web_content_layer_impl.h"

#include <stddef.h>

#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "cc/base/switches.h"
#include "cc/blink/web_display_item_list_impl.h"
#include "cc/layers/picture_layer.h"
#include "third_party/WebKit/public/platform/WebContentLayerClient.h"
#include "third_party/WebKit/public/platform/WebFloatPoint.h"
#include "third_party/WebKit/public/platform/WebFloatRect.h"
#include "third_party/WebKit/public/platform/WebRect.h"
#include "third_party/WebKit/public/platform/WebSize.h"
#include "third_party/skia/include/core/SkMatrix44.h"

using cc::PictureLayer;

namespace cc_blink {

static blink::WebContentLayerClient::PaintingControlSetting
PaintingControlToWeb(
    cc::ContentLayerClient::PaintingControlSetting painting_control) {
  switch (painting_control) {
    case cc::ContentLayerClient::PAINTING_BEHAVIOR_NORMAL:
      return blink::WebContentLayerClient::kPaintDefaultBehavior;
    case cc::ContentLayerClient::PAINTING_BEHAVIOR_NORMAL_FOR_TEST:
      return blink::WebContentLayerClient::kPaintDefaultBehaviorForTest;
    case cc::ContentLayerClient::DISPLAY_LIST_CONSTRUCTION_DISABLED:
      return blink::WebContentLayerClient::kDisplayListConstructionDisabled;
    case cc::ContentLayerClient::DISPLAY_LIST_CACHING_DISABLED:
      return blink::WebContentLayerClient::kDisplayListCachingDisabled;
    case cc::ContentLayerClient::DISPLAY_LIST_PAINTING_DISABLED:
      return blink::WebContentLayerClient::kDisplayListPaintingDisabled;
    case cc::ContentLayerClient::SUBSEQUENCE_CACHING_DISABLED:
      return blink::WebContentLayerClient::kSubsequenceCachingDisabled;
    case cc::ContentLayerClient::PARTIAL_INVALIDATION:
      return blink::WebContentLayerClient::kPartialInvalidation;
  }
  NOTREACHED();
  return blink::WebContentLayerClient::kPaintDefaultBehavior;
}

WebContentLayerImpl::WebContentLayerImpl(blink::WebContentLayerClient* client)
    : client_(client) {
  layer_ = std::make_unique<WebLayerImpl>(PictureLayer::Create(this));
  layer_->layer()->SetIsDrawable(true);
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
    cc::ContentLayerClient::PaintingControlSetting painting_control) {
  auto display_list = base::MakeRefCounted<cc::DisplayItemList>();
  if (client_) {
    WebDisplayItemListImpl list(display_list.get());
    client_->PaintContents(&list, PaintingControlToWeb(painting_control));
  }
  display_list->Finalize();
  return display_list;
}

bool WebContentLayerImpl::FillsBoundsCompletely() const {
  return false;
}

size_t WebContentLayerImpl::GetApproximateUnsharedMemoryUsage() const {
  return client_->ApproximateUnsharedMemoryUsage();
}

}  // namespace cc_blink
