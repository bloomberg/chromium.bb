// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/desktop_background/desktop_background_widget_controller.h"

#include "ash/ash_export.h"
#include "ash/desktop_background/user_wallpaper_delegate.h"
#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"

namespace ash {
namespace {

class ShowWallpaperAnimationObserver : public ui::ImplicitAnimationObserver,
                                       public views::WidgetObserver {
 public:
  ShowWallpaperAnimationObserver(RootWindowController* root_window_controller,
                                 views::Widget* desktop_widget,
                                 bool is_initial_animation)
      : root_window_controller_(root_window_controller),
        desktop_widget_(desktop_widget),
        is_initial_animation_(is_initial_animation) {
    DCHECK(desktop_widget_);
    desktop_widget_->AddObserver(this);
  }

  ~ShowWallpaperAnimationObserver() override {
    StopObservingImplicitAnimations();
    if (desktop_widget_)
      desktop_widget_->RemoveObserver(this);
  }

 private:
  // Overridden from ui::ImplicitAnimationObserver:
  void OnImplicitAnimationsScheduled() override {
    if (is_initial_animation_) {
      root_window_controller_->
          HandleInitialDesktopBackgroundAnimationStarted();
    }
  }

  void OnImplicitAnimationsCompleted() override {
    root_window_controller_->OnWallpaperAnimationFinished(desktop_widget_);
    delete this;
  }

  // Overridden from views::WidgetObserver.
  void OnWidgetDestroying(views::Widget* widget) override { delete this; }

  RootWindowController* root_window_controller_;
  views::Widget* desktop_widget_;

  // Is this object observing the initial brightness/grayscale animation?
  const bool is_initial_animation_;

  DISALLOW_COPY_AND_ASSIGN(ShowWallpaperAnimationObserver);
};

}  // namespace

DesktopBackgroundWidgetController::DesktopBackgroundWidgetController(
    views::Widget* widget) : widget_(widget) {
  DCHECK(widget_);
  widget_->AddObserver(this);
}

DesktopBackgroundWidgetController::~DesktopBackgroundWidgetController() {
  if (widget_) {
    widget_->RemoveObserver(this);
    widget_->CloseNow();
    widget_ = NULL;
  }
}

void DesktopBackgroundWidgetController::OnWidgetDestroying(
    views::Widget* widget) {
  widget_->RemoveObserver(this);
  widget_ = NULL;
}

void DesktopBackgroundWidgetController::SetBounds(gfx::Rect bounds) {
  if (widget_)
    widget_->SetBounds(bounds);
}

bool DesktopBackgroundWidgetController::Reparent(aura::Window* root_window,
                                                 int src_container,
                                                 int dest_container) {
  if (widget_) {
    views::Widget::ReparentNativeView(widget_->GetNativeView(),
        root_window->GetChildById(dest_container));
    return true;
  }
  // Nothing to reparent.
  return false;
}

void DesktopBackgroundWidgetController::StartAnimating(
    RootWindowController* root_window_controller) {
  if (widget_) {
    ui::ScopedLayerAnimationSettings settings(
        widget_->GetNativeView()->layer()->GetAnimator());
    settings.AddObserver(new ShowWallpaperAnimationObserver(
        root_window_controller, widget_,
        Shell::GetInstance()->user_wallpaper_delegate()->
            ShouldShowInitialAnimation()));
    // When |widget_| shows, AnimateShowWindowCommon() is called to do the
    // animation. Sets transition duration to 0 to avoid animating to the
    // show animation's initial values.
    settings.SetTransitionDuration(base::TimeDelta());
    widget_->Show();
  }
}

AnimatingDesktopController::AnimatingDesktopController(
    DesktopBackgroundWidgetController* component) {
  controller_.reset(component);
}

AnimatingDesktopController::~AnimatingDesktopController() {
}

void AnimatingDesktopController::StopAnimating() {
  if (controller_) {
    ui::Layer* layer = controller_->widget()->GetNativeView()->layer();
    layer->GetAnimator()->StopAnimating();
  }
}

DesktopBackgroundWidgetController* AnimatingDesktopController::GetController(
    bool pass_ownership) {
  if (pass_ownership)
    return controller_.release();
  return controller_.get();
}

}  // namespace ash
