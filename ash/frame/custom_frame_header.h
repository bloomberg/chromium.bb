// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_FRAME_CUSTOM_FRAME_HEADER_H_
#define ASH_FRAME_CUSTOM_FRAME_HEADER_H_

#include "ash/ash_export.h"
#include "ash/frame/frame_header.h"
#include "base/callback.h"
#include "base/macros.h"
#include "ui/gfx/animation/animation_delegate.h"
#include "ui/gfx/animation/slide_animation.h"

namespace gfx {
class ImageSkia;
class Rect;
}  // namespace gfx

namespace views {
class View;
class Widget;
}  // namespace views

namespace ash {

class FrameCaptionButton;
class FrameCaptionButtonContainerView;

// Helper class for drawing a custom frame (such as for a themed Chrome Browser
// frame).
class ASH_EXPORT CustomFrameHeader : public FrameHeader,
                                     public gfx::AnimationDelegate {
 public:
  class AppearanceProvider {
   public:
    virtual ~AppearanceProvider() = default;
    virtual SkColor GetFrameHeaderColor(bool active) = 0;
    virtual gfx::ImageSkia GetFrameHeaderImage(bool active) = 0;
    virtual gfx::ImageSkia GetFrameHeaderOverlayImage(bool active) = 0;
    virtual bool IsTabletMode() = 0;
  };

  CustomFrameHeader();
  ~CustomFrameHeader() override;

  // BrowserFrameHeaderAsh does not take ownership of any of the parameters.
  // |view| is the view into which |this| will paint. |back_button| can be
  // nullptr, and the frame will not have a back button.
  void Init(views::View* view,
            AppearanceProvider* appearance_provider,
            bool incognito,
            FrameCaptionButtonContainerView* caption_button_container);

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

 private:
  // gfx::AnimationDelegate override:
  void AnimationProgressed(const gfx::Animation* animation) override;

  // Does the actual work of layouting header.
  void LayoutHeaderInternal();

  // Paints the frame image for the |active| state based on the current value of
  // the activation animation.
  void PaintFrameImages(gfx::Canvas* canvas, bool active);

  // Paints the title bar, primarily the title string.
  void PaintTitleBar(gfx::Canvas* canvas);

  // Updates the size and icons used for the minimize, restore, and close
  // buttons.
  void UpdateCaptionButtons();

  // Returns bounds of the region in |view_| which is painted with the header
  // images. The region is assumed to start at the top left corner of |view_|
  // and to have the same width as |view_|.
  gfx::Rect GetPaintedBounds() const;

  // Returns the bounds for the title.
  gfx::Rect GetTitleBounds() const;

  views::Widget* GetWidget();

  // The view that owns |this|, into which the header is painted.
  views::View* view_ = nullptr;

  AppearanceProvider* appearance_provider_ = nullptr;

  // Whether the header is for an incognito browser window.
  bool is_incognito_ = false;

  views::View* window_icon_ = nullptr;
  FrameCaptionButton* back_button_ = nullptr;
  FrameCaptionButtonContainerView* caption_button_container_ = nullptr;
  int painted_height_ = 0;

  // Whether the header is painted for the first time.
  bool initial_paint_ = true;

  // Whether the header should be painted as active.
  Mode mode_ = MODE_INACTIVE;

  gfx::SlideAnimation activation_animation_{this};

  DISALLOW_COPY_AND_ASSIGN(CustomFrameHeader);
};

}  // namespace ash

#endif  // ASH_FRAME_CUSTOM_FRAME_HEADER_H_
