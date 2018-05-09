// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_FRAME_DEFAULT_FRAME_HEADER_H_
#define ASH_FRAME_DEFAULT_FRAME_HEADER_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/frame/caption_buttons/frame_caption_button.h"
#include "ash/frame/frame_header.h"
#include "base/compiler_specific.h"  // override
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/strings/string16.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/animation/animation_delegate.h"

namespace gfx {
class Rect;
class SlideAnimation;
}  // namespace gfx

namespace views {
class View;
class Widget;
}  // namespace views

namespace ash {
class FrameCaptionButton;
class FrameCaptionButtonContainerView;

// Helper class for managing the default window header.
class ASH_EXPORT DefaultFrameHeader : public FrameHeader,
                                      public gfx::AnimationDelegate {
 public:
  // DefaultFrameHeader does not take ownership of any of the parameters.
  DefaultFrameHeader(views::Widget* frame,
                     views::View* header_view,
                     FrameCaptionButtonContainerView* caption_button_container);
  ~DefaultFrameHeader() override;

  // FrameHeader overrides:
  int GetMinimumHeaderWidth() const override;
  void PaintHeader(gfx::Canvas* canvas, Mode mode) override;
  void LayoutHeader() override;
  int GetHeaderHeight() const override;
  int GetHeaderHeightForPainting() const override;
  void SetHeaderHeightForPainting(int height) override;
  void SchedulePaintForTitle() override;
  void SetPaintAsActive(bool paint_as_active) override;
  void OnShowStateChanged(ui::WindowShowState show_state) override;
  void SetLeftHeaderView(views::View* left_header_view) override;
  void SetBackButton(FrameCaptionButton* back_button) override;
  FrameCaptionButton* GetBackButton() const override;
  void SetFrameColors(SkColor active_frame_color,
                      SkColor inactive_frame_color) override;

  void SetThemeColor(SkColor theme_color);

  // Gets the color of the title text.
  SkColor GetTitleColor() const;

  // The default frame color for both active and inactive.
  static SkColor GetDefaultFrameColor();

  SkColor active_frame_color_for_testing() { return active_frame_color_; }
  SkColor inactive_frame_color_for_testing() { return inactive_frame_color_; }

 protected:
  // Updates the frame colors and ensures buttons are up to date.
  void SetFrameColorsImpl(SkColor active_frame_color,
                          SkColor inactive_frame_color);

  // Paints the title bar, primarily the title string.
  virtual void PaintTitleBar(gfx::Canvas* canvas);

  // Returns the bounds for the title.
  gfx::Rect GetAvailableTitleBounds() const;

  views::View* view() { return view_; }

 private:
  FRIEND_TEST_ALL_PREFIXES(DefaultFrameHeaderTest, BackButtonAlignment);
  FRIEND_TEST_ALL_PREFIXES(DefaultFrameHeaderTest, TitleIconAlignment);
  FRIEND_TEST_ALL_PREFIXES(DefaultFrameHeaderTest, FrameColors);

  // gfx::AnimationDelegate override:
  void AnimationProgressed(const gfx::Animation* animation) override;

  // Update all the images in the caption buttons.
  void UpdateAllButtonImages();

  // Updates the size button's images.
  void UpdateSizeButtonImages();

  // Returns the header bounds in the coordinates of |view_|. The header is
  // assumed to be positioned at the top left corner of |view_| and to have the
  // same width as |view_|.
  gfx::Rect GetLocalBounds() const;

  base::string16 GetTitle() const;

  SkColor GetCurrentFrameColor() const;

  views::Widget* frame_;
  views::View* view_;
  FrameCaptionButton* back_button_;  // May be nullptr.
  views::View* left_header_view_;    // May be nullptr.
  FrameCaptionButton::ColorMode button_color_mode_ =
      FrameCaptionButton::ColorMode::kDefault;
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

  DISALLOW_COPY_AND_ASSIGN(DefaultFrameHeader);
};

}  // namespace ash

#endif  // ASH_FRAME_DEFAULT_FRAME_HEADER_H_
