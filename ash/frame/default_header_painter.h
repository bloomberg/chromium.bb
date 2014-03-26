// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_FRAME_DEFAULT_HEADER_PAINTER_H_
#define ASH_FRAME_DEFAULT_HEADER_PAINTER_H_

#include "ash/ash_export.h"
#include "ash/frame/header_painter.h"
#include "base/basictypes.h"
#include "base/compiler_specific.h"  // OVERRIDE
#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "ui/gfx/animation/animation_delegate.h"

namespace gfx {
class ImageSkia;
class Rect;
class SlideAnimation;
}
namespace views {
class View;
class Widget;
}

namespace ash {
class FrameCaptionButtonContainerView;

// Helper class for painting the default window header.
class ASH_EXPORT DefaultHeaderPainter : public HeaderPainter,
                                        public gfx::AnimationDelegate {
 public:
  DefaultHeaderPainter();
  virtual ~DefaultHeaderPainter();

  // DefaultHeaderPainter does not take ownership of any of the parameters.
  void Init(views::Widget* frame,
            views::View* header_view,
            views::View* window_icon,
            FrameCaptionButtonContainerView* caption_button_container);

  // HeaderPainter overrides:
  virtual int GetMinimumHeaderWidth() const OVERRIDE;
  virtual void PaintHeader(gfx::Canvas* canvas, Mode mode) OVERRIDE;
  virtual void LayoutHeader() OVERRIDE;
  virtual int GetHeaderHeightForPainting() const OVERRIDE;
  virtual void SetHeaderHeightForPainting(int height) OVERRIDE;
  virtual void SchedulePaintForTitle() OVERRIDE;

  // Sets the window icon for the header. Passing NULL removes the window icon.
  void UpdateWindowIcon(views::View* window_icon, int icon_size);

 private:
  FRIEND_TEST_ALL_PREFIXES(DefaultHeaderPainterTest, TitleIconAlignment);

  // gfx::AnimationDelegate override:
  virtual void AnimationProgressed(const gfx::Animation* animation) OVERRIDE;

  // Paints highlight around the edge of the header for inactive restored
  // windows.
  void PaintHighlightForInactiveRestoredWindow(gfx::Canvas* canvas);

  // Paints the title bar, primarily the title string.
  void PaintTitleBar(gfx::Canvas* canvas);

  // Paints the header/content separator.
  void PaintHeaderContentSeparator(gfx::Canvas* canvas);

  // Returns the header bounds in the coordinates of |view_|. The header is
  // assumed to be positioned at the top left corner of |view_| and to have the
  // same width as |view_|.
  gfx::Rect GetLocalBounds() const;

  // Returns the bounds for the title.
  gfx::Rect GetTitleBounds() const;

  // Returns the frame image to use when |frame_| is active.
  gfx::ImageSkia* GetActiveFrameImage() const;

  // Returns the frame image to use when |frame_| is inactive.
  gfx::ImageSkia* GetInactiveFrameImage() const;

  views::Widget* frame_;
  views::View* view_;
  views::View* window_icon_;  // May be NULL.
  int window_icon_size_;
  FrameCaptionButtonContainerView* caption_button_container_;

  // The height of the header including the header/content separator.
  int height_;

  // Whether the header should be painted as active.
  Mode mode_;

  // Whether the header is painted for the first time.
  bool initial_paint_;

  scoped_ptr<gfx::SlideAnimation> activation_animation_;

  DISALLOW_COPY_AND_ASSIGN(DefaultHeaderPainter);
};

}  // namespace ash

#endif  // ASH_FRAME_DEFAULT_HEADER_PAINTER_H_
