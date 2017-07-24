// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/blink/web_display_item_list_impl.h"

#include <stddef.h>
#include <stdint.h>

#include <vector>

#include "cc/base/render_surface_filters.h"
#include "cc/paint/paint_op_buffer.h"
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
    : display_item_list_(base::MakeRefCounted<cc::DisplayItemList>()) {}

WebDisplayItemListImpl::WebDisplayItemListImpl(
    cc::DisplayItemList* display_list)
    : display_item_list_(display_list) {
}

WebDisplayItemListImpl::~WebDisplayItemListImpl() = default;

void WebDisplayItemListImpl::AppendDrawingItem(
    const blink::WebRect& visual_rect,
    sk_sp<const cc::PaintOpBuffer> record,
    const blink::WebRect& record_bounds) {
  display_item_list_->StartPaint();
  display_item_list_->push<cc::DrawRecordOp>(std::move(record));
  display_item_list_->EndPaintOfUnpaired(visual_rect);
}

void WebDisplayItemListImpl::AppendClipItem(
    const blink::WebRect& clip_rect,
    const blink::WebVector<SkRRect>& rounded_clip_rects) {
  bool antialias = true;
  display_item_list_->StartPaint();
  display_item_list_->push<cc::SaveOp>();
  display_item_list_->push<cc::ClipRectOp>(gfx::RectToSkRect(clip_rect),
                                           SkClipOp::kIntersect, antialias);
  for (const SkRRect& rrect : rounded_clip_rects) {
    if (rrect.isRect()) {
      display_item_list_->push<cc::ClipRectOp>(rrect.rect(),
                                               SkClipOp::kIntersect, antialias);
    } else {
      display_item_list_->push<cc::ClipRRectOp>(rrect, SkClipOp::kIntersect,
                                                antialias);
    }
  }
  display_item_list_->EndPaintOfPairedBegin();
}

void WebDisplayItemListImpl::AppendEndClipItem() {
  AppendRestore();
}

void WebDisplayItemListImpl::AppendClipPathItem(const SkPath& clip_path,
                                                bool antialias) {
  display_item_list_->StartPaint();
  display_item_list_->push<cc::SaveOp>();
  display_item_list_->push<cc::ClipPathOp>(clip_path, SkClipOp::kIntersect,
                                           antialias);
  display_item_list_->EndPaintOfPairedBegin();
}

void WebDisplayItemListImpl::AppendEndClipPathItem() {
  AppendRestore();
}

void WebDisplayItemListImpl::AppendFloatClipItem(
    const blink::WebFloatRect& clip_rect) {
  bool antialias = false;
  display_item_list_->StartPaint();
  display_item_list_->push<cc::SaveOp>();
  display_item_list_->push<cc::ClipRectOp>(gfx::RectFToSkRect(clip_rect),
                                           SkClipOp::kIntersect, antialias);
  display_item_list_->EndPaintOfPairedBegin();
}

void WebDisplayItemListImpl::AppendEndFloatClipItem() {
  AppendRestore();
}

void WebDisplayItemListImpl::AppendTransformItem(const SkMatrix44& matrix) {
  display_item_list_->StartPaint();
  display_item_list_->push<cc::SaveOp>();
  if (!matrix.isIdentity())
    display_item_list_->push<cc::ConcatOp>(static_cast<SkMatrix>(matrix));
  display_item_list_->EndPaintOfPairedBegin();
}

void WebDisplayItemListImpl::AppendEndTransformItem() {
  AppendRestore();
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
  auto alpha = static_cast<uint8_t>(gfx::ToFlooredInt(255 * opacity));

  if (xfermode == SkBlendMode::kSrcOver && !color_filter) {
    display_item_list_->StartPaint();
    display_item_list_->push<cc::SaveLayerAlphaOp>(bounds, alpha, false);
    display_item_list_->EndPaintOfPairedBegin();
    return;
  }

  cc::PaintFlags flags;
  flags.setBlendMode(xfermode);
  flags.setAlpha(alpha);
  flags.setColorFilter(sk_ref_sp(color_filter));

  display_item_list_->StartPaint();
  display_item_list_->push<cc::SaveLayerOp>(bounds, &flags);
  display_item_list_->EndPaintOfPairedBegin();
}

void WebDisplayItemListImpl::AppendEndCompositingItem() {
  AppendRestore();
}

void WebDisplayItemListImpl::AppendFilterItem(
    const cc::FilterOperations& filters,
    const blink::WebFloatRect& filter_bounds,
    const blink::WebFloatPoint& origin) {
  display_item_list_->StartPaint();

  // TODO(danakj): Skip the save+translate+restore if the origin is 0,0. This
  // should be easier to do when this code is part of the blink DisplayItem
  // which can keep related state.
  display_item_list_->push<cc::SaveOp>();
  display_item_list_->push<cc::TranslateOp>(origin.x, origin.y);

  cc::PaintFlags flags;
  flags.setImageFilter(cc::RenderSurfaceFilters::BuildImageFilter(
      filters, gfx::SizeF(filter_bounds.width, filter_bounds.height)));

  SkRect layer_bounds = gfx::RectFToSkRect(filter_bounds);
  layer_bounds.offset(-origin.x, -origin.y);
  display_item_list_->push<cc::SaveLayerOp>(&layer_bounds, &flags);
  display_item_list_->push<cc::TranslateOp>(-origin.x, -origin.y);

  display_item_list_->EndPaintOfPairedBegin(
      gfx::ToEnclosingRect(filter_bounds));
}

void WebDisplayItemListImpl::AppendEndFilterItem() {
  display_item_list_->StartPaint();
  display_item_list_->push<cc::RestoreOp>();  // For SaveLayerOp.
  display_item_list_->push<cc::RestoreOp>();  // For SaveOp.
  display_item_list_->EndPaintOfPairedEnd();
}

void WebDisplayItemListImpl::AppendScrollItem(
    const blink::WebSize& scroll_offset,
    ScrollContainerId) {
  display_item_list_->StartPaint();
  display_item_list_->push<cc::SaveOp>();
  display_item_list_->push<cc::TranslateOp>(
      static_cast<float>(-scroll_offset.width),
      static_cast<float>(-scroll_offset.height));
  display_item_list_->EndPaintOfPairedBegin();
}

void WebDisplayItemListImpl::AppendEndScrollItem() {
  AppendRestore();
}

void WebDisplayItemListImpl::AppendRestore() {
  display_item_list_->StartPaint();
  display_item_list_->push<cc::RestoreOp>();
  display_item_list_->EndPaintOfPairedEnd();
}

}  // namespace cc_blink
