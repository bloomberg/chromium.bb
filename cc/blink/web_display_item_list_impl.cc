// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/blink/web_display_item_list_impl.h"

#include <vector>

#include "cc/blink/web_filter_operations_impl.h"
#include "cc/resources/clip_display_item.h"
#include "cc/resources/clip_path_display_item.h"
#include "cc/resources/compositing_display_item.h"
#include "cc/resources/drawing_display_item.h"
#include "cc/resources/filter_display_item.h"
#include "cc/resources/float_clip_display_item.h"
#include "cc/resources/transform_display_item.h"
#include "skia/ext/refptr.h"
#include "third_party/WebKit/public/platform/WebFloatRect.h"
#include "third_party/WebKit/public/platform/WebRect.h"
#include "third_party/skia/include/core/SkColorFilter.h"
#include "third_party/skia/include/core/SkPicture.h"
#include "third_party/skia/include/utils/SkMatrix44.h"
#include "ui/gfx/geometry/safe_integer_conversions.h"
#include "ui/gfx/transform.h"

namespace cc_blink {

WebDisplayItemListImpl::WebDisplayItemListImpl(
    cc::DisplayItemList* display_list)
    : display_item_list_(display_list) {
}

void WebDisplayItemListImpl::appendDrawingItem(const SkPicture* picture) {
  auto* item =
      display_item_list_->CreateAndAppendItem<cc::DrawingDisplayItem>();
  item->SetNew(skia::SharePtr(const_cast<SkPicture*>(picture)));
}

void WebDisplayItemListImpl::appendClipItem(
    const blink::WebRect& clip_rect,
    const blink::WebVector<SkRRect>& rounded_clip_rects) {
  std::vector<SkRRect> rounded_rects;
  for (size_t i = 0; i < rounded_clip_rects.size(); ++i) {
    rounded_rects.push_back(rounded_clip_rects[i]);
  }
  auto* item = display_item_list_->CreateAndAppendItem<cc::ClipDisplayItem>();
  item->SetNew(clip_rect, rounded_rects);
}

void WebDisplayItemListImpl::appendEndClipItem() {
  display_item_list_->CreateAndAppendItem<cc::EndClipDisplayItem>();
}

void WebDisplayItemListImpl::appendClipPathItem(const SkPath& clip_path,
                                                SkRegion::Op clip_op,
                                                bool antialias) {
  auto* item =
      display_item_list_->CreateAndAppendItem<cc::ClipPathDisplayItem>();
  item->SetNew(clip_path, clip_op, antialias);
}

void WebDisplayItemListImpl::appendEndClipPathItem() {
  display_item_list_->CreateAndAppendItem<cc::EndClipPathDisplayItem>();
}

void WebDisplayItemListImpl::appendFloatClipItem(
    const blink::WebFloatRect& clip_rect) {
  auto* item =
      display_item_list_->CreateAndAppendItem<cc::FloatClipDisplayItem>();
  item->SetNew(clip_rect);
}

void WebDisplayItemListImpl::appendEndFloatClipItem() {
  display_item_list_->CreateAndAppendItem<cc::EndFloatClipDisplayItem>();
}

void WebDisplayItemListImpl::appendTransformItem(const SkMatrix44& matrix) {
  gfx::Transform transform;
  transform.matrix() = matrix;
  auto* item =
      display_item_list_->CreateAndAppendItem<cc::TransformDisplayItem>();
  item->SetNew(transform);
}

void WebDisplayItemListImpl::appendEndTransformItem() {
  display_item_list_->CreateAndAppendItem<cc::EndTransformDisplayItem>();
}

void WebDisplayItemListImpl::appendCompositingItem(
    float opacity,
    SkXfermode::Mode xfermode,
    SkRect* bounds,
    SkColorFilter* color_filter) {
  DCHECK_GE(opacity, 0.f);
  DCHECK_LE(opacity, 1.f);
  // TODO(ajuma): This should really be rounding instead of flooring the alpha
  // value, but that breaks slimming paint reftests.
  auto* item =
      display_item_list_->CreateAndAppendItem<cc::CompositingDisplayItem>();
  item->SetNew(static_cast<uint8_t>(gfx::ToFlooredInt(255 * opacity)), xfermode,
               bounds, skia::SharePtr(color_filter));
}

void WebDisplayItemListImpl::appendEndCompositingItem() {
  display_item_list_->CreateAndAppendItem<cc::EndCompositingDisplayItem>();
}

void WebDisplayItemListImpl::appendFilterItem(
    const blink::WebFilterOperations& filters,
    const blink::WebFloatRect& bounds) {
  const WebFilterOperationsImpl& filters_impl =
      static_cast<const WebFilterOperationsImpl&>(filters);
  auto* item = display_item_list_->CreateAndAppendItem<cc::FilterDisplayItem>();
  item->SetNew(filters_impl.AsFilterOperations(), bounds);
}

void WebDisplayItemListImpl::appendEndFilterItem() {
  display_item_list_->CreateAndAppendItem<cc::EndFilterDisplayItem>();
}

void WebDisplayItemListImpl::appendScrollItem(
    const blink::WebSize& scrollOffset,
    ScrollContainerId) {
  SkMatrix44 matrix;
  matrix.setTranslate(-scrollOffset.width, -scrollOffset.height, 0);
  appendTransformItem(matrix);
}

void WebDisplayItemListImpl::appendEndScrollItem() {
  appendEndTransformItem();
}

WebDisplayItemListImpl::~WebDisplayItemListImpl() {
}

}  // namespace cc_blink
