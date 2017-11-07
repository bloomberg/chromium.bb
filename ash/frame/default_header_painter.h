// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_FRAME_DEFAULT_HEADER_PAINTER_H_
#define ASH_FRAME_DEFAULT_HEADER_PAINTER_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/frame/header_painter.h"
#include "ash/public/interfaces/window_style.mojom.h"
#include "base/compiler_specific.h"  // override
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/animation/animation_delegate.h"

namespace gfx {
class Rect;
class SlideAnimation;
}
namespace views {
class View;
class Widget;
}

namespace ash {
class FrameCaptionButton;
class FrameCaptionButtonContainerView;

// Helper class for painting the default window header.
class ASH_EXPORT DefaultHeaderPainter : public HeaderPainter,
                                        public gfx::AnimationDelegate {
 public:
  explicit DefaultHeaderPainter(
      mojom::WindowStyle window_style = mojom::WindowStyle::DEFAULT);
  ~DefaultHeaderPainter() override;

  // DefaultHeaderPainter does not take ownership of any of the parameters.
  void Init(views::Widget* frame,
            views::View* header_view,
            FrameCaptionButtonContainerView* caption_button_container,
            FrameCaptionButton* back_button);

  // HeaderPainter overrides:
  int GetMinimumHeaderWidth() const override;
  void PaintHeader(gfx::Canvas* canvas, Mode mode) override;
  void LayoutHeader() override;
  int GetHeaderHeight() const override;
  int GetHeaderHeightForPainting() const override;
  void SetHeaderHeightForPainting(int height) override;
  void SchedulePaintForTitle() override;
  void SetPaintAsActive(bool paint_as_active) override;

  // Sets the left header view for the header. Passing |nullptr| removes the
  // view.
  void UpdateLeftHeaderView(views::View* left_header_view);

  // Sets the back button for the header. Passing |nullptr| removes the view.
  void UpdateBackButton(FrameCaptionButton* button);

  // Sets the active and inactive frame colors. Note the inactive frame color
  // will have some transparency added when the frame is drawn.
  void SetFrameColors(SkColor active_frame_color, SkColor inactive_frame_color);
  SkColor GetActiveFrameColor() const;
  SkColor GetInactiveFrameColor() const;

  // Gets the color of the title text.
  SkColor GetTitleColor() const;

  // Whether light caption images should be used. This is the case when the
  // background of the frame is dark.
  bool ShouldUseLightImages() const;

 private:
  FRIEND_TEST_ALL_PREFIXES(DefaultHeaderPainterTest, BackButtonAlignment);
  FRIEND_TEST_ALL_PREFIXES(DefaultHeaderPainterTest, TitleIconAlignment);
  FRIEND_TEST_ALL_PREFIXES(DefaultHeaderPainterTest, LightIcons);

  // gfx::AnimationDelegate override:
  void AnimationProgressed(const gfx::Animation* animation) override;

  // Paints highlight around the edge of the header for inactive restored
  // windows.
  void PaintHighlightForInactiveRestoredWindow(gfx::Canvas* canvas);

  // Paints the title bar, primarily the title string.
  void PaintTitleBar(gfx::Canvas* canvas);

  // Paints the header/content separator.
  void PaintHeaderContentSeparator(gfx::Canvas* canvas);

  // Update all the images in the caption buttons.
  void UpdateAllButtonImages();

  // Updates the size button's images.
  void UpdateSizeButtonImages();

  // Returns the header bounds in the coordinates of |view_|. The header is
  // assumed to be positioned at the top left corner of |view_| and to have the
  // same width as |view_|.
  gfx::Rect GetLocalBounds() const;

  // Returns the bounds for the title.
  gfx::Rect GetTitleBounds() const;

  // Returns whether the frame uses custom frame coloring.
  bool UsesCustomFrameColors() const;

  const mojom::WindowStyle window_style_;
  views::Widget* frame_;
  views::View* view_;
  FrameCaptionButton* back_button_;  // May be nullptr.
  views::View* left_header_view_;    // May be nullptr.
  SkColor active_frame_color_;
  SkColor inactive_frame_color_;
  FrameCaptionButtonContainerView* caption_button_container_;

  // The height of the header to paint.
  int painted_height_;

  // Whether the header should be painted as active.
  Mode mode_;

  // Whether the header is painted for the first time.
  bool initial_paint_;

  std::unique_ptr<gfx::SlideAnimation> activation_animation_;

  DISALLOW_COPY_AND_ASSIGN(DefaultHeaderPainter);
};

}  // namespace ash

#endif  // ASH_FRAME_DEFAULT_HEADER_PAINTER_H_
