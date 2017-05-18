// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/blink/web_display_item_list_impl.h"

#include <stddef.h>
#include <stdint.h>

#include <vector>

#include "cc/paint/clip_display_item.h"
#include "cc/paint/clip_path_display_item.h"
#include "cc/paint/compositing_display_item.h"
#include "cc/paint/drawing_display_item.h"
#include "cc/paint/filter_display_item.h"
#include "cc/paint/float_clip_display_item.h"
#include "cc/paint/paint_record.h"
#include "cc/paint/transform_display_item.h"
#include "third_party/WebKit/public/platform/WebFloatRect.h"
#include "third_party/WebKit/public/platform/WebRect.h"
#include "third_party/skia/include/core/SkColorFilter.h"
#include "third_party/skia/include/core/SkMatrix44.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/safe_integer_conversions.h"
#include "ui/gfx/skia_util.h"
#include "ui/gfx/transform.h"

namespace cc_blink {

namespace {

}  // namespace

WebDisplayItemListImpl::WebDisplayItemListImpl()
    : display_item_list_(new cc::DisplayItemList) {}

WebDisplayItemListImpl::WebDisplayItemListImpl(
    cc::DisplayItemList* display_list)
    : display_item_list_(display_list) {
}

void WebDisplayItemListImpl::AppendDrawingItem(
    const blink::WebRect& visual_rect,
    sk_sp<const cc::PaintRecord> record,
    const blink::WebRect& record_bounds) {
  display_item_list_->CreateAndAppendDrawingItem<cc::DrawingDisplayItem>(
      visual_rect, std::move(record), gfx::RectToSkRect(record_bounds));
}

void WebDisplayItemListImpl::AppendClipItem(
    const blink::WebRect& clip_rect,
    const blink::WebVector<SkRRect>& rounded_clip_rects) {
  std::vector<SkRRect> rounded_rects;
  for (size_t i = 0; i < rounded_clip_rects.size(); ++i) {
    rounded_rects.push_back(rounded_clip_rects[i]);
  }
  bool antialias = true;
  display_item_list_->CreateAndAppendPairedBeginItem<cc::ClipDisplayItem>(
      clip_rect, rounded_rects, antialias);
}

void WebDisplayItemListImpl::AppendEndClipItem() {
  display_item_list_->CreateAndAppendPairedEndItem<cc::EndClipDisplayItem>();
}

void WebDisplayItemListImpl::AppendClipPathItem(const SkPath& clip_path,
                                                bool antialias) {
  display_item_list_->CreateAndAppendPairedBeginItem<cc::ClipPathDisplayItem>(
      clip_path, antialias);
}

void WebDisplayItemListImpl::AppendEndClipPathItem() {
  display_item_list_
      ->CreateAndAppendPairedEndItem<cc::EndClipPathDisplayItem>();
}

void WebDisplayItemListImpl::AppendFloatClipItem(
    const blink::WebFloatRect& clip_rect) {
  display_item_list_->CreateAndAppendPairedBeginItem<cc::FloatClipDisplayItem>(
      clip_rect);
}

void WebDisplayItemListImpl::AppendEndFloatClipItem() {
  display_item_list_
      ->CreateAndAppendPairedEndItem<cc::EndFloatClipDisplayItem>();
}

void WebDisplayItemListImpl::AppendTransformItem(const SkMatrix44& matrix) {
  gfx::Transform transform(gfx::Transform::kSkipInitialization);
  transform.matrix() = matrix;

  display_item_list_->CreateAndAppendPairedBeginItem<cc::TransformDisplayItem>(
      transform);
}

void WebDisplayItemListImpl::AppendEndTransformItem() {
  display_item_list_
      ->CreateAndAppendPairedEndItem<cc::EndTransformDisplayItem>();
}

void WebDisplayItemListImpl::AppendCompositingItem(
    float opacity,
    SkBlendMode xfermode,
    SkRect* bounds,
    SkColorFilter* color_filter) {
  DCHECK_GE(opacity, 0.f);
  DCHECK_LE(opacity, 1.f);
  // TODO(ajuma): This should really be rounding instead of flooring the alpha
  // value, but that breaks slimming paint reftests.

  const bool kLcdTextRequiresOpaqueLayer = true;
  display_item_list_
      ->CreateAndAppendPairedBeginItem<cc::CompositingDisplayItem>(
          static_cast<uint8_t>(gfx::ToFlooredInt(255 * opacity)), xfermode,
          bounds, sk_ref_sp(color_filter), kLcdTextRequiresOpaqueLayer);
}

void WebDisplayItemListImpl::AppendEndCompositingItem() {
  display_item_list_
      ->CreateAndAppendPairedEndItem<cc::EndCompositingDisplayItem>();
}

void WebDisplayItemListImpl::AppendFilterItem(
    const cc::FilterOperations& filters,
    const blink::WebFloatRect& filter_bounds,
    const blink::WebFloatPoint& origin) {
  display_item_list_
      ->CreateAndAppendPairedBeginItemWithVisualRect<cc::FilterDisplayItem>(
          gfx::ToEnclosingRect(filter_bounds), filters, filter_bounds, origin);
}

void WebDisplayItemListImpl::AppendEndFilterItem() {
  display_item_list_->CreateAndAppendPairedEndItem<cc::EndFilterDisplayItem>();
}

void WebDisplayItemListImpl::AppendScrollItem(
    const blink::WebSize& scroll_offset,
    ScrollContainerId) {
  SkMatrix44 matrix(SkMatrix44::kUninitialized_Constructor);
  matrix.setTranslate(-scroll_offset.width, -scroll_offset.height, 0);
  // TODO(wkorman): http://crbug.com/633636 Should we translate the visual rect
  // as well? Create a test case and investigate.
  AppendTransformItem(matrix);
}

void WebDisplayItemListImpl::AppendEndScrollItem() {
  AppendEndTransformItem();
}

void WebDisplayItemListImpl::SetIsSuitableForGpuRasterization(bool isSuitable) {
  display_item_list_->SetIsSuitableForGpuRasterization(isSuitable);
}

WebDisplayItemListImpl::~WebDisplayItemListImpl() {
}

}  // namespace cc_blink
