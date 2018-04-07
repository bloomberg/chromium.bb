// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_BLINK_SCROLLBAR_IMPL_H_
#define CC_BLINK_SCROLLBAR_IMPL_H_

#include <memory>

#include "base/macros.h"
#include "cc/input/scrollbar.h"
#include "cc/paint/paint_canvas.h"
#include "third_party/blink/public/platform/web_scrollbar_theme_painter.h"

namespace blink {
class WebScrollbar;
class WebScrollbarThemeGeometry;
}

namespace cc_blink {

class ScrollbarImpl : public cc::Scrollbar {
 public:
  ScrollbarImpl(std::unique_ptr<blink::WebScrollbar> scrollbar,
                blink::WebScrollbarThemePainter painter,
                std::unique_ptr<blink::WebScrollbarThemeGeometry> geometry);
  ~ScrollbarImpl() override;

  // cc::Scrollbar implementation.
  cc::ScrollbarOrientation Orientation() const override;
  bool IsLeftSideVerticalScrollbar() const override;
  bool HasThumb() const override;
  bool IsOverlay() const override;
  gfx::Point Location() const override;
  int ThumbThickness() const override;
  int ThumbLength() const override;
  gfx::Rect TrackRect() const override;
  float ThumbOpacity() const override;
  bool NeedsPaintPart(cc::ScrollbarPart part) const override;
  bool HasTickmarks() const override;
  void PaintPart(cc::PaintCanvas* canvas,
                 cc::ScrollbarPart part,
                 const gfx::Rect& content_rect) override;

  bool UsesNinePatchThumbResource() const override;
  gfx::Size NinePatchThumbCanvasSize() const override;
  gfx::Rect NinePatchThumbAperture() const override;

 private:
  std::unique_ptr<blink::WebScrollbar> scrollbar_;
  blink::WebScrollbarThemePainter painter_;
  std::unique_ptr<blink::WebScrollbarThemeGeometry> geometry_;

  DISALLOW_COPY_AND_ASSIGN(ScrollbarImpl);
};

}  // namespace cc_blink

#endif  // CC_BLINK_SCROLLBAR_IMPL_H_
