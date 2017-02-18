// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/wallpaper/wallpaper_widget_controller.h"

#include "ash/ash_export.h"
#include "ash/common/wallpaper/wallpaper_delegate.h"
#include "ash/common/wm_lookup.h"
#include "ash/common/wm_shell.h"
#include "ash/common/wm_window.h"
#include "ash/root_window_controller.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace {

class ShowWallpaperAnimationObserver : public ui::ImplicitAnimationObserver,
                                       public views::WidgetObserver {
 public:
  ShowWallpaperAnimationObserver(RootWindowController* root_window_controller,
                                 views::Widget* wallpaper_widget,
                                 bool is_initial_animation)
      : root_window_controller_(root_window_controller),
        wallpaper_widget_(wallpaper_widget),
        is_initial_animation_(is_initial_animation) {
    DCHECK(wallpaper_widget_);
    wallpaper_widget_->AddObserver(this);
  }

  ~ShowWallpaperAnimationObserver() override {
    StopObservingImplicitAnimations();
    if (wallpaper_widget_)
      wallpaper_widget_->RemoveObserver(this);
  }

 private:
  // Overridden from ui::ImplicitAnimationObserver:
  void OnImplicitAnimationsScheduled() override {
    if (is_initial_animation_)
      root_window_controller_->OnInitialWallpaperAnimationStarted();
  }

  void OnImplicitAnimationsCompleted() override {
    root_window_controller_->OnWallpaperAnimationFinished(wallpaper_widget_);
    delete this;
  }

  // Overridden from views::WidgetObserver.
  void OnWidgetDestroying(views::Widget* widget) override { delete this; }

  RootWindowController* root_window_controller_;
  views::Widget* wallpaper_widget_;

  // Is this object observing the initial brightness/grayscale animation?
  const bool is_initial_animation_;

  DISALLOW_COPY_AND_ASSIGN(ShowWallpaperAnimationObserver);
};

}  // namespace

WallpaperWidgetController::WallpaperWidgetController(views::Widget* widget)
    : widget_(widget),
      widget_parent_(WmLookup::Get()->GetWindowForWidget(widget)->GetParent()) {
  DCHECK(widget_);
  widget_->AddObserver(this);
  widget_parent_->aura_window()->AddObserver(this);
}

WallpaperWidgetController::~WallpaperWidgetController() {
  if (widget_) {
    views::Widget* widget = widget_;
    RemoveObservers();
    widget->CloseNow();
  }
}

void WallpaperWidgetController::OnWidgetDestroying(views::Widget* widget) {
  RemoveObservers();
}

void WallpaperWidgetController::SetBounds(const gfx::Rect& bounds) {
  if (widget_)
    widget_->SetBounds(bounds);
}

bool WallpaperWidgetController::Reparent(WmWindow* root_window, int container) {
  if (widget_) {
    widget_parent_->aura_window()->RemoveObserver(this);
    WmWindow* window = WmLookup::Get()->GetWindowForWidget(widget_);
    root_window->GetChildByShellWindowId(container)->AddChild(window);
    widget_parent_ = WmLookup::Get()->GetWindowForWidget(widget_)->GetParent();
    widget_parent_->aura_window()->AddObserver(this);
    return true;
  }
  // Nothing to reparent.
  return false;
}

void WallpaperWidgetController::RemoveObservers() {
  widget_parent_->aura_window()->RemoveObserver(this);
  widget_->RemoveObserver(this);
  widget_ = nullptr;
}

void WallpaperWidgetController::OnWindowBoundsChanged(
    aura::Window* window,
    const gfx::Rect& old_bounds,
    const gfx::Rect& new_bounds) {
  SetBounds(new_bounds);
}

void WallpaperWidgetController::StartAnimating(
    RootWindowController* root_window_controller) {
  if (widget_) {
    ui::ScopedLayerAnimationSettings settings(
        widget_->GetLayer()->GetAnimator());
    settings.AddObserver(new ShowWallpaperAnimationObserver(
        root_window_controller, widget_,
        WmShell::Get()->wallpaper_delegate()->ShouldShowInitialAnimation()));
    // When |widget_| shows, AnimateShowWindowCommon() is called to do the
    // animation. Sets transition duration to 0 to avoid animating to the
    // show animation's initial values.
    settings.SetTransitionDuration(base::TimeDelta());
    widget_->Show();
  }
}

AnimatingWallpaperWidgetController::AnimatingWallpaperWidgetController(
    WallpaperWidgetController* controller)
    : controller_(controller) {}

AnimatingWallpaperWidgetController::~AnimatingWallpaperWidgetController() {}

void AnimatingWallpaperWidgetController::StopAnimating() {
  if (controller_)
    controller_->widget()->GetLayer()->GetAnimator()->StopAnimating();
}

WallpaperWidgetController* AnimatingWallpaperWidgetController::GetController(
    bool pass_ownership) {
  if (pass_ownership)
    return controller_.release();
  return controller_.get();
}

}  // namespace ash
