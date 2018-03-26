// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_FRAME_HEADER_VIEW_H_
#define ASH_FRAME_HEADER_VIEW_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/public/cpp/immersive/immersive_fullscreen_controller_delegate.h"
#include "ash/public/interfaces/window_style.mojom.h"
#include "ash/wm/tablet_mode/tablet_mode_observer.h"
#include "base/macros.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/views/view.h"

namespace gfx {
class ImageSkia;
}

namespace views {
class ImageView;
class Widget;
}

namespace ash {

class CaptionButtonModel;
class DefaultFrameHeader;
class FrameCaptionButton;
class FrameCaptionButtonContainerView;
enum class FrameBackButtonState;

// View which paints the frame header (title, caption buttons...). It slides off
// and on screen in immersive fullscreen.
class ASH_EXPORT HeaderView : public views::View,
                              public ImmersiveFullscreenControllerDelegate,
                              public TabletModeObserver {
 public:
  // |target_widget| is the widget that the caption buttons act on.
  // |target_widget| is not necessarily the same as the widget the header is
  // placed in. For example, in immersive fullscreen this view may be painted in
  // a widget that slides in and out on top of the main app or browser window.
  // However, clicking a caption button should act on the target widget.
  explicit HeaderView(
      views::Widget* target_widget,
      mojom::WindowStyle window_style = mojom::WindowStyle::DEFAULT,
      std::unique_ptr<CaptionButtonModel> model = nullptr);
  ~HeaderView() override;

  void set_is_immersive_delegate(bool value) { is_immersive_delegate_ = value; }

  bool should_paint() { return should_paint_; }

  // Schedules a repaint for the entire title.
  void SchedulePaintForTitle();

  // Tells the window controls to reset themselves to the normal state.
  void ResetWindowControls();

  // Returns the amount of the view's pixels which should be on screen.
  int GetPreferredOnScreenHeight();

  // Returns the view's preferred height.
  int GetPreferredHeight();

  // Returns the view's minimum width.
  int GetMinimumWidth() const;

  // Sets the avatar icon to be displayed on the frame header.
  void SetAvatarIcon(const gfx::ImageSkia& avatar);

  void UpdateCaptionButtons();

  void SetFrameColors(SkColor active_frame_color, SkColor inactive_frame_color);
  SkColor GetActiveFrameColor() const;
  SkColor GetInactiveFrameColor() const;

  // views::View:
  void Layout() override;
  void OnPaint(gfx::Canvas* canvas) override;
  void ChildPreferredSizeChanged(views::View* child) override;

  // TabletModeObserver:
  void OnTabletModeStarted() override;
  void OnTabletModeEnded() override;

  FrameCaptionButtonContainerView* caption_button_container() {
    return caption_button_container_;
  }

  views::View* avatar_icon() const;

  void SetShouldPaintHeader(bool paint);

  FrameCaptionButton* GetBackButton();

 private:
  // ImmersiveFullscreenControllerDelegate:
  void OnImmersiveRevealStarted() override;
  void OnImmersiveRevealEnded() override;
  void OnImmersiveFullscreenExited() override;
  void SetVisibleFraction(double visible_fraction) override;
  std::vector<gfx::Rect> GetVisibleBoundsInScreen() const override;

  // The widget that the caption buttons act on.
  views::Widget* target_widget_;

  // Helper for painting the header.
  std::unique_ptr<DefaultFrameHeader> frame_header_;

  views::ImageView* avatar_icon_;

  // View which contains the window caption buttons.
  FrameCaptionButtonContainerView* caption_button_container_;

  // The fraction of the header's height which is visible while in fullscreen.
  // This value is meaningless when not in fullscreen.
  double fullscreen_visible_fraction_;

  // Has this instance been set as the ImmersiveFullscreenControllerDelegate?
  bool is_immersive_delegate_ = true;

  bool did_layout_ = false;

  // False to skip painting. Used for overview mode to hide the header.
  bool should_paint_;

  DISALLOW_COPY_AND_ASSIGN(HeaderView);
};

}  // namespace ash

#endif  // ASH_FRAME_HEADER_VIEW_H_
