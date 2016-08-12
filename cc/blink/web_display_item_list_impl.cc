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
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/safe_integer_conversions.h"
#include "ui/gfx/transform.h"

namespace cc_blink {

namespace {

scoped_refptr<cc::DisplayItemList> CreateUncachedDisplayItemListForBlink() {
  cc::DisplayItemListSettings settings;
  settings.use_cached_picture = false;
  gfx::Rect layer_rect;
  return cc::DisplayItemList::Create(layer_rect, settings);
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
  if (display_item_list_->RetainsIndividualDisplayItems()) {
    display_item_list_->CreateAndAppendDrawingItem<cc::DrawingDisplayItem>(
        visual_rect, std::move(picture));
  } else {
    cc::DrawingDisplayItem item(std::move(picture));
    display_item_list_->RasterIntoCanvas(item);
  }
}

void WebDisplayItemListImpl::appendClipItem(
    const blink::WebRect& clip_rect,
    const blink::WebVector<SkRRect>& rounded_clip_rects) {
  std::vector<SkRRect> rounded_rects;
  for (size_t i = 0; i < rounded_clip_rects.size(); ++i) {
    rounded_rects.push_back(rounded_clip_rects[i]);
  }
  bool antialias = true;
  if (display_item_list_->RetainsIndividualDisplayItems()) {
    display_item_list_->CreateAndAppendPairedBeginItem<cc::ClipDisplayItem>(
        clip_rect, rounded_rects, antialias);
  } else {
    cc::ClipDisplayItem item(clip_rect, rounded_rects, antialias);
    display_item_list_->RasterIntoCanvas(item);
  }
}

void WebDisplayItemListImpl::appendEndClipItem() {
  if (display_item_list_->RetainsIndividualDisplayItems()) {
    display_item_list_->CreateAndAppendPairedEndItem<cc::EndClipDisplayItem>();
  } else {
    display_item_list_->RasterIntoCanvas(cc::EndClipDisplayItem());
  }
}

void WebDisplayItemListImpl::appendClipPathItem(
    const SkPath& clip_path,
    SkRegion::Op clip_op,
    bool antialias) {
  if (display_item_list_->RetainsIndividualDisplayItems()) {
    display_item_list_->CreateAndAppendPairedBeginItem<cc::ClipPathDisplayItem>(
        clip_path, clip_op, antialias);
  } else {
    cc::ClipPathDisplayItem item(clip_path, clip_op, antialias);
    display_item_list_->RasterIntoCanvas(item);
  }
}

void WebDisplayItemListImpl::appendEndClipPathItem() {
  if (display_item_list_->RetainsIndividualDisplayItems()) {
    display_item_list_
        ->CreateAndAppendPairedEndItem<cc::EndClipPathDisplayItem>();
  } else {
    display_item_list_->RasterIntoCanvas(cc::EndClipPathDisplayItem());
  }
}

void WebDisplayItemListImpl::appendFloatClipItem(
    const blink::WebFloatRect& clip_rect) {
  if (display_item_list_->RetainsIndividualDisplayItems()) {
    display_item_list_
        ->CreateAndAppendPairedBeginItem<cc::FloatClipDisplayItem>(clip_rect);
  } else {
    cc::FloatClipDisplayItem item(clip_rect);
    display_item_list_->RasterIntoCanvas(item);
  }
}

void WebDisplayItemListImpl::appendEndFloatClipItem() {
  if (display_item_list_->RetainsIndividualDisplayItems()) {
    display_item_list_
        ->CreateAndAppendPairedEndItem<cc::EndFloatClipDisplayItem>();
  } else {
    display_item_list_->RasterIntoCanvas(cc::EndFloatClipDisplayItem());
  }
}

void WebDisplayItemListImpl::appendTransformItem(
    const SkMatrix44& matrix) {
  gfx::Transform transform(gfx::Transform::kSkipInitialization);
  transform.matrix() = matrix;

  if (display_item_list_->RetainsIndividualDisplayItems()) {
    display_item_list_
        ->CreateAndAppendPairedBeginItem<cc::TransformDisplayItem>(transform);
  } else {
    cc::TransformDisplayItem item(transform);
    display_item_list_->RasterIntoCanvas(item);
  }
}

void WebDisplayItemListImpl::appendEndTransformItem() {
  if (display_item_list_->RetainsIndividualDisplayItems()) {
    display_item_list_
        ->CreateAndAppendPairedEndItem<cc::EndTransformDisplayItem>();
  } else {
    display_item_list_->RasterIntoCanvas(cc::EndTransformDisplayItem());
  }
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

  const bool kLcdTextRequiresOpaqueLayer = true;
  if (display_item_list_->RetainsIndividualDisplayItems()) {
    display_item_list_
        ->CreateAndAppendPairedBeginItem<cc::CompositingDisplayItem>(
            static_cast<uint8_t>(gfx::ToFlooredInt(255 * opacity)), xfermode,
            bounds, sk_ref_sp(color_filter), kLcdTextRequiresOpaqueLayer);
  } else {
    cc::CompositingDisplayItem item(
        static_cast<uint8_t>(gfx::ToFlooredInt(255 * opacity)), xfermode,
        bounds, sk_ref_sp(color_filter), kLcdTextRequiresOpaqueLayer);
    display_item_list_->RasterIntoCanvas(item);
  }
}

void WebDisplayItemListImpl::appendEndCompositingItem() {
  if (display_item_list_->RetainsIndividualDisplayItems()) {
    display_item_list_
        ->CreateAndAppendPairedEndItem<cc::EndCompositingDisplayItem>();
  } else {
    display_item_list_->RasterIntoCanvas(cc::EndCompositingDisplayItem());
  }
}

void WebDisplayItemListImpl::appendFilterItem(
    const cc::FilterOperations& filters,
    const blink::WebFloatRect& filter_bounds) {
  if (display_item_list_->RetainsIndividualDisplayItems()) {
    display_item_list_
        ->CreateAndAppendPairedBeginItemWithVisualRect<cc::FilterDisplayItem>(
            gfx::ToEnclosingRect(filter_bounds), filters, filter_bounds);
  } else {
    cc::FilterDisplayItem item(filters, filter_bounds);
    display_item_list_->RasterIntoCanvas(item);
  }
}

void WebDisplayItemListImpl::appendEndFilterItem() {
  if (display_item_list_->RetainsIndividualDisplayItems()) {
    display_item_list_
        ->CreateAndAppendPairedEndItem<cc::EndFilterDisplayItem>();
  } else {
    display_item_list_->RasterIntoCanvas(cc::EndFilterDisplayItem());
  }
}

void WebDisplayItemListImpl::appendScrollItem(
    const blink::WebSize& scroll_offset,
    ScrollContainerId) {
  SkMatrix44 matrix(SkMatrix44::kUninitialized_Constructor);
  matrix.setTranslate(-scroll_offset.width, -scroll_offset.height, 0);
  // TODO(wkorman): Should we translate the visual rect as well?
  appendTransformItem(matrix);
}

void WebDisplayItemListImpl::appendEndScrollItem() {
  appendEndTransformItem();
}

void WebDisplayItemListImpl::setIsSuitableForGpuRasterization(bool isSuitable) {
  display_item_list_->SetIsSuitableForGpuRasterization(isSuitable);
}

WebDisplayItemListImpl::~WebDisplayItemListImpl() {
}

}  // namespace cc_blink
