// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/blink/web_display_item_list_impl.h"

#include <stddef.h>
#include <stdint.h>

#include <vector>

#include "cc/playback/clip_display_item.h"
#include "cc/playback/clip_path_display_item.h"
#include "cc/playback/compositing_display_item.h"
#include "cc/playback/display_item_list_settings.h"
#include "cc/playback/drawing_display_item.h"
#include "cc/playback/filter_display_item.h"
#include "cc/playback/float_clip_display_item.h"
#include "cc/playback/transform_display_item.h"
#include "third_party/WebKit/public/platform/WebFloatRect.h"
#include "third_party/WebKit/public/platform/WebRect.h"
#include "third_party/skia/include/core/SkColorFilter.h"
#include "third_party/skia/include/core/SkMatrix44.h"
#include "third_party/skia/include/core/SkPicture.h"
#include "ui/gfx/geometry/safe_integer_conversions.h"
#include "ui/gfx/transform.h"

namespace cc_blink {

namespace {

scoped_refptr<cc::DisplayItemList> CreateUncachedDisplayItemListForBlink() {
  cc::DisplayItemListSettings settings;
  settings.use_cached_picture = false;
  return cc::DisplayItemList::Create(settings);
}

}  // namespace

WebDisplayItemListImpl::WebDisplayItemListImpl()
    : display_item_list_(CreateUncachedDisplayItemListForBlink()) {
}

WebDisplayItemListImpl::WebDisplayItemListImpl(
    cc::DisplayItemList* display_list)
    : display_item_list_(display_list) {
}

void WebDisplayItemListImpl::appendDrawingItem(
    const blink::WebRect& visual_rect,
    sk_sp<const SkPicture> picture) {
  display_item_list_->CreateAndAppendItem<cc::DrawingDisplayItem>(
      visual_rect, std::move(picture));
}

void WebDisplayItemListImpl::appendClipItem(
    const blink::WebRect& visual_rect,
    const blink::WebRect& clip_rect,
    const blink::WebVector<SkRRect>& rounded_clip_rects) {
  std::vector<SkRRect> rounded_rects;
  for (size_t i = 0; i < rounded_clip_rects.size(); ++i) {
    rounded_rects.push_back(rounded_clip_rects[i]);
  }
  bool antialias = true;
  display_item_list_->CreateAndAppendItem<cc::ClipDisplayItem>(
      visual_rect, clip_rect, rounded_rects, antialias);
}

void WebDisplayItemListImpl::appendEndClipItem(
    const blink::WebRect& visual_rect) {
  display_item_list_->CreateAndAppendItem<cc::EndClipDisplayItem>(visual_rect);
}

void WebDisplayItemListImpl::appendClipPathItem(
    const blink::WebRect& visual_rect,
    const SkPath& clip_path,
    SkRegion::Op clip_op,
    bool antialias) {
  display_item_list_->CreateAndAppendItem<cc::ClipPathDisplayItem>(
      visual_rect, clip_path, clip_op, antialias);
}

void WebDisplayItemListImpl::appendEndClipPathItem(
    const blink::WebRect& visual_rect) {
  display_item_list_->CreateAndAppendItem<cc::EndClipPathDisplayItem>(
      visual_rect);
}

void WebDisplayItemListImpl::appendFloatClipItem(
    const blink::WebRect& visual_rect,
    const blink::WebFloatRect& clip_rect) {
  display_item_list_->CreateAndAppendItem<cc::FloatClipDisplayItem>(visual_rect,
                                                                    clip_rect);
}

void WebDisplayItemListImpl::appendEndFloatClipItem(
    const blink::WebRect& visual_rect) {
  display_item_list_->CreateAndAppendItem<cc::EndFloatClipDisplayItem>(
      visual_rect);
}

void WebDisplayItemListImpl::appendTransformItem(
    const blink::WebRect& visual_rect,
    const SkMatrix44& matrix) {
  gfx::Transform transform(gfx::Transform::kSkipInitialization);
  transform.matrix() = matrix;

  display_item_list_->CreateAndAppendItem<cc::TransformDisplayItem>(visual_rect,
                                                                    transform);
}

void WebDisplayItemListImpl::appendEndTransformItem(
    const blink::WebRect& visual_rect) {
  display_item_list_->CreateAndAppendItem<cc::EndTransformDisplayItem>(
      visual_rect);
}

void WebDisplayItemListImpl::appendCompositingItem(
    const blink::WebRect& visual_rect,
    float opacity,
    SkXfermode::Mode xfermode,
    SkRect* bounds,
    SkColorFilter* color_filter) {
  DCHECK_GE(opacity, 0.f);
  DCHECK_LE(opacity, 1.f);
  // TODO(ajuma): This should really be rounding instead of flooring the alpha
  // value, but that breaks slimming paint reftests.

  const bool kLcdTextRequiresOpaqueLayer = true;
  display_item_list_->CreateAndAppendItem<cc::CompositingDisplayItem>(
      visual_rect, static_cast<uint8_t>(gfx::ToFlooredInt(255 * opacity)),
      xfermode, bounds, sk_ref_sp(color_filter), kLcdTextRequiresOpaqueLayer);
}

void WebDisplayItemListImpl::appendEndCompositingItem(
    const blink::WebRect& visual_rect) {
  display_item_list_->CreateAndAppendItem<cc::EndCompositingDisplayItem>(
      visual_rect);
}

void WebDisplayItemListImpl::appendFilterItem(
    const blink::WebRect& visual_rect,
    const cc::FilterOperations& filters,
    const blink::WebFloatRect& bounds) {
  display_item_list_->CreateAndAppendItem<cc::FilterDisplayItem>(
      visual_rect, filters, bounds);
}

void WebDisplayItemListImpl::appendEndFilterItem(
    const blink::WebRect& visual_rect) {
  display_item_list_->CreateAndAppendItem<cc::EndFilterDisplayItem>(
      visual_rect);
}

void WebDisplayItemListImpl::appendScrollItem(
    const blink::WebRect& visual_rect,
    const blink::WebSize& scroll_offset,
    ScrollContainerId) {
  SkMatrix44 matrix(SkMatrix44::kUninitialized_Constructor);
  matrix.setTranslate(-scroll_offset.width, -scroll_offset.height, 0);
  // TODO(wkorman): http://crbug.com/633636 Should we translate the visual rect
  // as well? Create a test case and investigate.
  appendTransformItem(visual_rect, matrix);
}

void WebDisplayItemListImpl::appendEndScrollItem(
    const blink::WebRect& visual_rect) {
  appendEndTransformItem(visual_rect);
}

void WebDisplayItemListImpl::setIsSuitableForGpuRasterization(bool isSuitable) {
  display_item_list_->SetIsSuitableForGpuRasterization(isSuitable);
}

WebDisplayItemListImpl::~WebDisplayItemListImpl() {
}

}  // namespace cc_blink
