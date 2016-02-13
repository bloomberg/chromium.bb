// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_HEADER_PAINTER_ASH_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_HEADER_PAINTER_ASH_H_

#include "ash/frame/header_painter.h"
#include "base/compiler_specific.h"  // override
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/animation/animation_delegate.h"

class BrowserView;

namespace ash {
class FrameCaptionButtonContainerView;
}

namespace gfx {
class ImageSkia;
class Rect;
class SlideAnimation;
}
namespace views {
class View;
class Widget;
}

// Helper class for painting the browser window header.
class BrowserHeaderPainterAsh : public ash::HeaderPainter,
                                public gfx::AnimationDelegate {
 public:
  BrowserHeaderPainterAsh();
  ~BrowserHeaderPainterAsh() override;

  // BrowserHeaderPainterAsh does not take ownership of any of the parameters.
  void Init(
    views::Widget* frame,
    BrowserView* browser_view,
    views::View* header_view,
    views::View* window_icon,
    ash::FrameCaptionButtonContainerView* caption_button_container);

  // ash::HeaderPainter overrides:
  int GetMinimumHeaderWidth() const override;
  void PaintHeader(gfx::Canvas* canvas, Mode mode) override;
  void LayoutHeader() override;
  int GetHeaderHeight() const override;
  int GetHeaderHeightForPainting() const override;
  void SetHeaderHeightForPainting(int height) override;
  void SchedulePaintForTitle() override;

 private:
  // gfx::AnimationDelegate override:
  void AnimationProgressed(const gfx::Animation* animation) override;

  // Paints highlight around the edge of the header for restored windows.
  void PaintHighlightForRestoredWindow(gfx::Canvas* canvas);

  // Paints the title bar, primarily the title string.
  void PaintTitleBar(gfx::Canvas* canvas);

  // Gets the base image to paint for the frame. This may be null, in which case
  // GetFrameColor() should be used instead.
  gfx::ImageSkia GetFrameImage(Mode mode) const;

  // Gets the overlay image to paint on top of the base image. This may be null,
  // in which case nothing will be drawn.
  gfx::ImageSkia GetFrameOverlayImage(Mode mode) const;

  // Gets the color to use to paint the frame (when GetFrameImage() returns
  // a null image).
  SkColor GetFrameColor(Mode mode) const;

  // Updates the size and icons used for the minimize, restore, and close
  // buttons.
  void UpdateCaptionButtons();

  // Returns bounds of the region in |view_| which is painted with the header
  // images. The region is assumed to start at the top left corner of |view_|
  // and to have the same width as |view_|.
  gfx::Rect GetPaintedBounds() const;

  // Returns the bounds for the title.
  gfx::Rect GetTitleBounds() const;

  views::Widget* frame_;

  // Whether the header is for a tabbed browser window.
  bool is_tabbed_;

  // Whether the header is for an incognito browser window.
  bool is_incognito_;

  // The header view.
  views::View* view_;

  views::View* window_icon_;
  ash::FrameCaptionButtonContainerView* caption_button_container_;
  int painted_height_;

  // Whether the header is painted for the first time.
  bool initial_paint_;

  // Whether the header should be painted as active.
  Mode mode_;

  scoped_ptr<gfx::SlideAnimation> activation_animation_;

  DISALLOW_COPY_AND_ASSIGN(BrowserHeaderPainterAsh);
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_HEADER_PAINTER_ASH_H_
