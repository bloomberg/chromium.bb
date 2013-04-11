// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/desktop_background/desktop_background_view.h"

#include <limits>

#include "ash/ash_export.h"
#include "ash/desktop_background/desktop_background_controller.h"
#include "ash/desktop_background/desktop_background_widget_controller.h"
#include "ash/desktop_background/user_wallpaper_delegate.h"
#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "ash/shell_window_ids.h"
#include "ash/wm/property_util.h"
#include "ash/wm/window_animations.h"
#include "base/message_loop.h"
#include "base/utf_string_conversions.h"
#include "ui/aura/root_window.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/image.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"

namespace ash {
namespace internal {
namespace {

class ShowWallpaperAnimationObserver : public ui::ImplicitAnimationObserver,
                                       public views::WidgetObserver {
 public:
  ShowWallpaperAnimationObserver(aura::RootWindow* root_window,
                                 views::Widget* desktop_widget,
                                 bool is_initial_animation)
      : root_window_(root_window),
        desktop_widget_(desktop_widget),
        is_initial_animation_(is_initial_animation) {
    DCHECK(desktop_widget_);
    desktop_widget_->AddObserver(this);
  }

  virtual ~ShowWallpaperAnimationObserver() {
    StopObservingImplicitAnimations();
    if (desktop_widget_)
      desktop_widget_->RemoveObserver(this);
  }

 private:
  // Overridden from ui::ImplicitAnimationObserver:
  virtual void OnImplicitAnimationsScheduled() OVERRIDE {
    if (is_initial_animation_) {
      GetRootWindowController(root_window_)->
          HandleInitialDesktopBackgroundAnimationStarted();
    }
  }

  virtual void OnImplicitAnimationsCompleted() OVERRIDE {
    DCHECK(desktop_widget_);
    GetRootWindowController(root_window_)->HandleDesktopBackgroundVisible();
    ash::Shell::GetInstance()->user_wallpaper_delegate()->
        OnWallpaperAnimationFinished();
    // Only removes old component when wallpaper animation finished. If we
    // remove the old one before the new wallpaper is done fading in there will
    // be a white flash during the animation.
    if (root_window_->GetProperty(kAnimatingDesktopController)) {
      DesktopBackgroundWidgetController* controller =
          root_window_->GetProperty(kAnimatingDesktopController)->
              GetController(true);
      // |desktop_widget_| should be the same animating widget we try to move
      // to |kDesktopController|. Otherwise, we may close |desktop_widget_|
      // before move it to |kDesktopController|.
      DCHECK_EQ(controller->widget(), desktop_widget_);
      // Release the old controller and close its background widget.
      root_window_->SetProperty(kDesktopController, controller);
    }
    delete this;
  }

  // Overridden from views::WidgetObserver.
  virtual void OnWidgetDestroying(views::Widget* widget) OVERRIDE {
    delete this;
  }

  aura::RootWindow* root_window_;
  views::Widget* desktop_widget_;

  // Is this object observing the initial brightness/grayscale animation?
  const bool is_initial_animation_;

  DISALLOW_COPY_AND_ASSIGN(ShowWallpaperAnimationObserver);
};

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
      ash::Shell::GetInstance()->desktop_background_controller();
  gfx::ImageSkia wallpaper = controller->GetWallpaper();
  WallpaperLayout wallpaper_layout = controller->GetWallpaperLayout();

  gfx::Rect wallpaper_rect(0, 0, wallpaper.width(), wallpaper.height());
  if (wallpaper_layout == WALLPAPER_LAYOUT_CENTER_CROPPED &&
      wallpaper.width() > width() && wallpaper.height() > height()) {
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

    gfx::Rect wallpaper_cropped_rect = wallpaper_rect;
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
    // All other are simply centered, and not scaled (but may be clipped).
     canvas->DrawImageInt(wallpaper, (width() - wallpaper.width()) / 2,
         (height() - wallpaper.height()) / 2);
  }
}

bool DesktopBackgroundView::OnMousePressed(const ui::MouseEvent& event) {
  return true;
}

void DesktopBackgroundView::ShowContextMenuForView(views::View* source,
                                                   const gfx::Point& point) {
  Shell::GetInstance()->ShowContextMenu(point);
}

views::Widget* CreateDesktopBackground(aura::RootWindow* root_window,
                                       int container_id) {
  DesktopBackgroundController* controller =
      ash::Shell::GetInstance()->desktop_background_controller();
  ash::UserWallpaperDelegate* wallpaper_delegate =
    ash::Shell::GetInstance()->user_wallpaper_delegate();

  views::Widget* desktop_widget = new views::Widget;
  views::Widget::InitParams params(
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  if (controller->GetWallpaper().isNull())
    params.transparent = true;
  params.parent = root_window->GetChildById(container_id);
  desktop_widget->Init(params);
  desktop_widget->SetContentsView(new DesktopBackgroundView());
  int animation_type = wallpaper_delegate->GetAnimationType();
  views::corewm::SetWindowVisibilityAnimationType(
      desktop_widget->GetNativeView(), animation_type);

  // Enable wallpaper transition for the following cases:
  // 1. Initial(OOBE) wallpaper animation.
  // 2. Wallpaper fades in from a non empty background.
  // 3. From an empty background, chrome transit to a logged in user session.
  // 4. From an empty background, guest user logged in.
  if (wallpaper_delegate->ShouldShowInitialAnimation() ||
      root_window->GetProperty(kAnimatingDesktopController) ||
      Shell::GetInstance()->delegate()->IsGuestSession() ||
      Shell::GetInstance()->delegate()->IsUserLoggedIn()) {
    views::corewm::SetWindowVisibilityAnimationTransition(
        desktop_widget->GetNativeView(), views::corewm::ANIMATE_SHOW);
  } else {
    // Disable animation if transition to login screen from an empty background.
    views::corewm::SetWindowVisibilityAnimationTransition(
        desktop_widget->GetNativeView(), views::corewm::ANIMATE_NONE);
  }

  desktop_widget->SetBounds(params.parent->bounds());
  ui::ScopedLayerAnimationSettings settings(
      desktop_widget->GetNativeView()->layer()->GetAnimator());
  settings.SetPreemptionStrategy(ui::LayerAnimator::ENQUEUE_NEW_ANIMATION);
  settings.AddObserver(new ShowWallpaperAnimationObserver(
      root_window, desktop_widget,
      wallpaper_delegate->ShouldShowInitialAnimation()));
  desktop_widget->Show();
  desktop_widget->GetNativeView()->SetName("DesktopBackgroundView");
  return desktop_widget;
}

}  // namespace internal
}  // namespace ash
