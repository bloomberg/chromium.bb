// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/desktop_background/desktop_background_view.h"

#include <limits>

#include "ash/ash_export.h"
#include "ash/desktop_background/desktop_background_controller.h"
#include "ash/desktop_background/desktop_background_widget_controller.h"
#include "ash/desktop_background/user_wallpaper_delegate.h"
#include "ash/display/display_manager.h"
#include "ash/root_window_controller.h"
#include "ash/session_state_delegate.h"
#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "ash/wm/property_util.h"
#include "ash/wm/window_animations.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/aura/root_window.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/image.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace internal {
namespace {

// For our scaling ratios we need to round positive numbers.
int RoundPositive(double x) {
  return static_cast<int>(floor(x + 0.5));
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// DesktopBackgroundView, public:

DesktopBackgroundView::DesktopBackgroundView() {
  set_context_menu_controller(this);
}

DesktopBackgroundView::~DesktopBackgroundView() {
}

////////////////////////////////////////////////////////////////////////////////
// DesktopBackgroundView, views::View overrides:

void DesktopBackgroundView::OnPaint(gfx::Canvas* canvas) {
  // Scale the image while maintaining the aspect ratio, cropping as
  // necessary to fill the background. Ideally the image should be larger
  // than the largest display supported, if not we will center it rather than
  // streching to avoid upsampling artifacts (Note that we could tile too, but
  // decided not to do this at the moment).
  DesktopBackgroundController* controller =
      Shell::GetInstance()->desktop_background_controller();
  gfx::ImageSkia wallpaper = controller->GetWallpaper();
  WallpaperLayout wallpaper_layout = controller->GetWallpaperLayout();

  gfx::NativeView native_view = GetWidget()->GetNativeView();
  gfx::Display display = gfx::Screen::GetScreenFor(native_view)->
      GetDisplayNearestWindow(native_view);

  DisplayManager* display_manager = Shell::GetInstance()->display_manager();
  DisplayInfo display_info = display_manager->GetDisplayInfo(display.id());
  float scaling = display_info.ui_scale();
  if (scaling <= 1.0f)
    scaling = 1.0f;
  // Allow scaling up to the UI scaling.
  // TODO(oshima): Create separate layer that fits to the image and then
  // scale to avoid artifacts and be more efficient when clipped.
  gfx::Rect wallpaper_rect(
      0, 0, wallpaper.width() * scaling, wallpaper.height() * scaling);

  if (wallpaper_layout == WALLPAPER_LAYOUT_CENTER_CROPPED &&
      wallpaper_rect.width() >= width() &&
      wallpaper_rect.height() >= height()) {
    // The dimension with the smallest ratio must be cropped, the other one
    // is preserved. Both are set in gfx::Size cropped_size.
    double horizontal_ratio = static_cast<double>(width()) /
        static_cast<double>(wallpaper.width());
    double vertical_ratio = static_cast<double>(height()) /
        static_cast<double>(wallpaper.height());

    gfx::Size cropped_size;
    if (vertical_ratio > horizontal_ratio) {
      cropped_size = gfx::Size(
          RoundPositive(static_cast<double>(width()) / vertical_ratio),
          wallpaper.height());
    } else {
      cropped_size = gfx::Size(wallpaper.width(),
          RoundPositive(static_cast<double>(height()) / horizontal_ratio));
    }

    gfx::Rect wallpaper_cropped_rect(
        0, 0, wallpaper.width(), wallpaper.height());
    wallpaper_cropped_rect.ClampToCenteredSize(cropped_size);
    canvas->DrawImageInt(wallpaper,
        wallpaper_cropped_rect.x(), wallpaper_cropped_rect.y(),
        wallpaper_cropped_rect.width(), wallpaper_cropped_rect.height(),
        0, 0, width(), height(),
        true);
  } else if (wallpaper_layout == WALLPAPER_LAYOUT_TILE) {
    canvas->TileImageInt(wallpaper, 0, 0, width(), height());
  } else if (wallpaper_layout == WALLPAPER_LAYOUT_STRETCH) {
    // This is generally not recommended as it may show artifacts.
    canvas->DrawImageInt(wallpaper, 0, 0, wallpaper.width(),
        wallpaper.height(), 0, 0, width(), height(), true);
  } else {
    // Fill with black to make sure that the entire area is opaque.
    canvas->FillRect(GetLocalBounds(), SK_ColorBLACK);
    // All other are simply centered, and not scaled (but may be clipped).
    if (wallpaper.width() && wallpaper.height()) {
      canvas->DrawImageInt(
          wallpaper,
          0, 0, wallpaper.width(), wallpaper.height(),
          (width() - wallpaper_rect.width()) / 2,
          (height() - wallpaper_rect.height()) / 2,
          wallpaper_rect.width(),
          wallpaper_rect.height(),
          true);
    }
  }
}

bool DesktopBackgroundView::OnMousePressed(const ui::MouseEvent& event) {
  return true;
}

void DesktopBackgroundView::ShowContextMenuForView(
    views::View* source,
    const gfx::Point& point,
    ui::MenuSourceType source_type) {
  Shell::GetInstance()->ShowContextMenu(point, source_type);
}

views::Widget* CreateDesktopBackground(aura::RootWindow* root_window,
                                       int container_id) {
  DesktopBackgroundController* controller =
      Shell::GetInstance()->desktop_background_controller();
  UserWallpaperDelegate* wallpaper_delegate =
      Shell::GetInstance()->user_wallpaper_delegate();

  views::Widget* desktop_widget = new views::Widget;
  views::Widget::InitParams params(
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  if (controller->GetWallpaper().isNull())
    params.opacity = views::Widget::InitParams::TRANSLUCENT_WINDOW;
  params.parent = root_window->GetChildById(container_id);
  desktop_widget->Init(params);
  desktop_widget->SetContentsView(new DesktopBackgroundView());
  int animation_type = wallpaper_delegate->GetAnimationType();
  views::corewm::SetWindowVisibilityAnimationType(
      desktop_widget->GetNativeView(), animation_type);

  RootWindowController* root_window_controller =
      GetRootWindowController(root_window);

  // Enable wallpaper transition for the following cases:
  // 1. Initial(OOBE) wallpaper animation.
  // 2. Wallpaper fades in from a non empty background.
  // 3. From an empty background, chrome transit to a logged in user session.
  // 4. From an empty background, guest user logged in.
  if (wallpaper_delegate->ShouldShowInitialAnimation() ||
      root_window_controller->animating_wallpaper_controller() ||
      Shell::GetInstance()->session_state_delegate()->NumberOfLoggedInUsers()) {
    views::corewm::SetWindowVisibilityAnimationTransition(
        desktop_widget->GetNativeView(), views::corewm::ANIMATE_SHOW);
  } else {
    // Disable animation if transition to login screen from an empty background.
    views::corewm::SetWindowVisibilityAnimationTransition(
        desktop_widget->GetNativeView(), views::corewm::ANIMATE_NONE);
  }

  desktop_widget->SetBounds(params.parent->bounds());
  return desktop_widget;
}

}  // namespace internal
}  // namespace ash
