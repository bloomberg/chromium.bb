// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/blink/web_display_item_list_impl.h"

#include <vector>

#include "cc/blink/web_blend_mode.h"
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
#include "third_party/skia/include/core/SkImageFilter.h"
#include "third_party/skia/include/core/SkPicture.h"
#include "third_party/skia/include/utils/SkMatrix44.h"
#include "ui/gfx/transform.h"

namespace cc_blink {

WebDisplayItemListImpl::WebDisplayItemListImpl()
    : display_item_list_(cc::DisplayItemList::Create()) {
}

scoped_refptr<cc::DisplayItemList> WebDisplayItemListImpl::ToDisplayItemList() {
  return display_item_list_;
}

void WebDisplayItemListImpl::appendDrawingItem(const SkPicture* picture) {
  display_item_list_->AppendItem(cc::DrawingDisplayItem::Create(
      skia::SharePtr(const_cast<SkPicture*>(picture))));
}

void WebDisplayItemListImpl::appendClipItem(
    const blink::WebRect& clip_rect,
    const blink::WebVector<SkRRect>& rounded_clip_rects) {
  std::vector<SkRRect> rounded_rects;
  for (size_t i = 0; i < rounded_clip_rects.size(); ++i) {
    rounded_rects.push_back(rounded_clip_rects[i]);
  }
  display_item_list_->AppendItem(
      cc::ClipDisplayItem::Create(clip_rect, rounded_rects));
}

void WebDisplayItemListImpl::appendEndClipItem() {
  display_item_list_->AppendItem(cc::EndClipDisplayItem::Create());
}

void WebDisplayItemListImpl::appendClipPathItem(const SkPath& clip_path,
                                                SkRegion::Op clip_op,
                                                bool antialias) {
  display_item_list_->AppendItem(
      cc::ClipPathDisplayItem::Create(clip_path, clip_op, antialias));
}

void WebDisplayItemListImpl::appendEndClipPathItem() {
  display_item_list_->AppendItem(cc::EndClipPathDisplayItem::Create());
}

void WebDisplayItemListImpl::appendFloatClipItem(
    const blink::WebFloatRect& clip_rect) {
  display_item_list_->AppendItem(cc::FloatClipDisplayItem::Create(clip_rect));
}

void WebDisplayItemListImpl::appendEndFloatClipItem() {
  display_item_list_->AppendItem(cc::EndFloatClipDisplayItem::Create());
}

void WebDisplayItemListImpl::appendTransformItem(const SkMatrix44& matrix) {
  gfx::Transform transform;
  transform.matrix() = matrix;
  display_item_list_->AppendItem(cc::TransformDisplayItem::Create(transform));
}

void WebDisplayItemListImpl::appendEndTransformItem() {
  display_item_list_->AppendItem(cc::EndTransformDisplayItem::Create());
}

// TODO(pdr): Remove this once the blink-side callers have been removed.
void WebDisplayItemListImpl::appendCompositingItem(
    float opacity,
    SkXfermode::Mode xfermode,
    SkColorFilter* color_filter) {
  appendCompositingItem(opacity, xfermode, nullptr, color_filter);
}

void WebDisplayItemListImpl::appendCompositingItem(
    float opacity,
    SkXfermode::Mode xfermode,
    SkRect* bounds,
    SkColorFilter* color_filter) {
  display_item_list_->AppendItem(cc::CompositingDisplayItem::Create(
      opacity, xfermode, bounds, skia::SharePtr(color_filter)));
}

void WebDisplayItemListImpl::appendEndCompositingItem() {
  display_item_list_->AppendItem(cc::EndCompositingDisplayItem::Create());
}

void WebDisplayItemListImpl::appendFilterItem(
    const blink::WebFilterOperations& filters,
    const blink::WebFloatRect& bounds) {
  const WebFilterOperationsImpl& filters_impl =
      static_cast<const WebFilterOperationsImpl&>(filters);
  display_item_list_->AppendItem(
      cc::FilterDisplayItem::Create(filters_impl.AsFilterOperations(), bounds));
}

void WebDisplayItemListImpl::appendEndFilterItem() {
  display_item_list_->AppendItem(cc::EndFilterDisplayItem::Create());
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
