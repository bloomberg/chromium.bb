// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_FRAME_HEADER_ASH_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_FRAME_HEADER_ASH_H_

#include "ash/frame/frame_header.h"
#include "base/callback.h"
#include "base/macros.h"
#include "ui/gfx/animation/animation_delegate.h"
#include "ui/gfx/animation/slide_animation.h"

namespace ash {
class FrameCaptionButton;
class FrameCaptionButtonContainerView;
}  // namespace ash

namespace gfx {
class ImageSkia;
class Rect;
}  // namespace gfx
namespace views {
class View;
class Widget;
}  // namespace views

// Helper class for managing the browser window header.
class BrowserFrameHeaderAsh : public ash::FrameHeader,
                              public gfx::AnimationDelegate {
 public:
  class AppearanceProvider {
   public:
    virtual SkColor GetFrameHeaderColor(bool active) = 0;
    virtual gfx::ImageSkia GetFrameHeaderImage(bool active) = 0;
    virtual gfx::ImageSkia GetFrameHeaderOverlayImage(bool active) = 0;
    virtual bool IsTabletMode() = 0;
  };

  BrowserFrameHeaderAsh();
  ~BrowserFrameHeaderAsh() override;

  // BrowserFrameHeaderAsh does not take ownership of any of the parameters.
  // |view| is the view into which |this| will paint. |back_button| can be
  // nullptr, and the frame will not have a back button.
  void Init(views::View* view,
            AppearanceProvider* appearance_provider,
            bool incognito,
            views::View* window_icon,
            ash::FrameCaptionButtonContainerView* caption_button_container,
            ash::FrameCaptionButton* back_button);

  // ash::FrameHeader overrides:
  int GetMinimumHeaderWidth() const override;
  void PaintHeader(gfx::Canvas* canvas, Mode mode) override;
  void LayoutHeader() override;
  int GetHeaderHeight() const override;
  int GetHeaderHeightForPainting() const override;
  void SetHeaderHeightForPainting(int height) override;
  void SchedulePaintForTitle() override;
  void SetPaintAsActive(bool paint_as_active) override;
  void OnShowStateChanged(ui::WindowShowState show_state) override;

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
  ash::FrameCaptionButton* back_button_ = nullptr;
  ash::FrameCaptionButtonContainerView* caption_button_container_ = nullptr;
  int painted_height_ = 0;

  // Whether the header is painted for the first time.
  bool initial_paint_ = true;

  // Whether the header should be painted as active.
  Mode mode_ = MODE_INACTIVE;

  gfx::SlideAnimation activation_animation_{this};

  DISALLOW_COPY_AND_ASSIGN(BrowserFrameHeaderAsh);
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_FRAME_HEADER_ASH_H_
