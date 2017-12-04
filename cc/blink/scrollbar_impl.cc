// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/blink/scrollbar_impl.h"

#include "base/logging.h"
#include "third_party/WebKit/public/platform/WebScrollbar.h"
#include "third_party/WebKit/public/platform/WebScrollbarThemeGeometry.h"

using blink::WebScrollbar;

namespace cc_blink {

ScrollbarImpl::ScrollbarImpl(
    std::unique_ptr<WebScrollbar> scrollbar,
    blink::WebScrollbarThemePainter painter,
    std::unique_ptr<blink::WebScrollbarThemeGeometry> geometry)
    : scrollbar_(std::move(scrollbar)),
      painter_(painter),
      geometry_(std::move(geometry)) {}

ScrollbarImpl::~ScrollbarImpl() = default;

cc::ScrollbarOrientation ScrollbarImpl::Orientation() const {
  if (scrollbar_->GetOrientation() == WebScrollbar::kHorizontal)
    return cc::HORIZONTAL;
  return cc::VERTICAL;
}

bool ScrollbarImpl::IsLeftSideVerticalScrollbar() const {
  return scrollbar_->IsLeftSideVerticalScrollbar();
}

bool ScrollbarImpl::HasThumb() const {
  return geometry_->HasThumb(scrollbar_.get());
}

bool ScrollbarImpl::IsOverlay() const {
  return scrollbar_->IsOverlay();
}

gfx::Point ScrollbarImpl::Location() const {
  return scrollbar_->Location();
}

int ScrollbarImpl::ThumbThickness() const {
  gfx::Rect thumb_rect = geometry_->ThumbRect(scrollbar_.get());
  if (scrollbar_->GetOrientation() == WebScrollbar::kHorizontal)
    return thumb_rect.height();
  return thumb_rect.width();
}

int ScrollbarImpl::ThumbLength() const {
  gfx::Rect thumb_rect = geometry_->ThumbRect(scrollbar_.get());
  if (scrollbar_->GetOrientation() == WebScrollbar::kHorizontal)
    return thumb_rect.width();
  return thumb_rect.height();
}

gfx::Rect ScrollbarImpl::TrackRect() const {
  return geometry_->TrackRect(scrollbar_.get());
}

float ScrollbarImpl::ThumbOpacity() const {
  return painter_.ThumbOpacity();
}

bool ScrollbarImpl::NeedsPaintPart(cc::ScrollbarPart part) const {
  if (part == cc::THUMB)
    return painter_.ThumbNeedsRepaint();
  return painter_.TrackNeedsRepaint();
}

bool ScrollbarImpl::UsesNinePatchThumbResource() const {
  return painter_.UsesNinePatchThumbResource();
}

gfx::Size ScrollbarImpl::NinePatchThumbCanvasSize() const {
  return geometry_->NinePatchThumbCanvasSize(scrollbar_.get());
}

gfx::Rect ScrollbarImpl::NinePatchThumbAperture() const {
  return geometry_->NinePatchThumbAperture(scrollbar_.get());
}

void ScrollbarImpl::PaintPart(cc::PaintCanvas* canvas,
                              cc::ScrollbarPart part,
                              const gfx::Rect& content_rect) {
  if (part == cc::THUMB) {
    painter_.PaintThumb(canvas, content_rect);
    return;
  }

  // The following is a simplification of ScrollbarThemeComposite::paint.
  painter_.PaintScrollbarBackground(canvas, content_rect);

  if (geometry_->HasButtons(scrollbar_.get())) {
    gfx::Rect back_button_start_paint_rect =
        geometry_->BackButtonStartRect(scrollbar_.get());
    painter_.PaintBackButtonStart(canvas, back_button_start_paint_rect);

    gfx::Rect back_button_end_paint_rect =
        geometry_->BackButtonEndRect(scrollbar_.get());
    painter_.PaintBackButtonEnd(canvas, back_button_end_paint_rect);

    gfx::Rect forward_button_start_paint_rect =
        geometry_->ForwardButtonStartRect(scrollbar_.get());
    painter_.PaintForwardButtonStart(canvas, forward_button_start_paint_rect);

    gfx::Rect forward_button_end_paint_rect =
        geometry_->ForwardButtonEndRect(scrollbar_.get());
    painter_.PaintForwardButtonEnd(canvas, forward_button_end_paint_rect);
  }

  gfx::Rect track_paint_rect = geometry_->TrackRect(scrollbar_.get());
  painter_.PaintTrackBackground(canvas, track_paint_rect);

  bool thumb_present = geometry_->HasThumb(scrollbar_.get());
  if (thumb_present) {
    painter_.PaintForwardTrackPart(canvas, track_paint_rect);
    painter_.PaintBackTrackPart(canvas, track_paint_rect);
  }

  painter_.PaintTickmarks(canvas, track_paint_rect);
}

}  // namespace cc_blink
