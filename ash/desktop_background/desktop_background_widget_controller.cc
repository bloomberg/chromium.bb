// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/desktop_background/desktop_background_widget_controller.h"

#include "ash/ash_export.h"
#include "ash/desktop_background/user_wallpaper_delegate.h"
#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ui/aura/root_window.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"

namespace ash {
namespace internal {
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

  virtual ~ShowWallpaperAnimationObserver() {
    StopObservingImplicitAnimations();
    if (desktop_widget_)
      desktop_widget_->RemoveObserver(this);
  }

 private:
  // Overridden from ui::ImplicitAnimationObserver:
  virtual void OnImplicitAnimationsScheduled() OVERRIDE {
    if (is_initial_animation_) {
      root_window_controller_->
          HandleInitialDesktopBackgroundAnimationStarted();
    }
  }

  virtual void OnImplicitAnimationsCompleted() OVERRIDE {
    root_window_controller_->OnWallpaperAnimationFinished(desktop_widget_);
    delete this;
  }

  // Overridden from views::WidgetObserver.
  virtual void OnWidgetDestroying(views::Widget* widget) OVERRIDE {
    delete this;
  }

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

DesktopBackgroundWidgetController::DesktopBackgroundWidgetController(
    ui::Layer* layer) : widget_(NULL) {
  layer_.reset(layer);
}

DesktopBackgroundWidgetController::~DesktopBackgroundWidgetController() {
  if (widget_) {
    widget_->RemoveObserver(this);
    widget_->CloseNow();
    widget_ = NULL;
  } else if (layer_)
    layer_.reset(NULL);
}

void DesktopBackgroundWidgetController::OnWidgetDestroying(
    views::Widget* widget) {
  widget_->RemoveObserver(this);
  widget_ = NULL;
}

void DesktopBackgroundWidgetController::SetBounds(gfx::Rect bounds) {
  if (widget_)
    widget_->SetBounds(bounds);
  else if (layer_)
    layer_->SetBounds(bounds);
}

bool DesktopBackgroundWidgetController::Reparent(aura::RootWindow* root_window,
                                                 int src_container,
                                                 int dest_container) {
  if (widget_) {
    views::Widget::ReparentNativeView(widget_->GetNativeView(),
        root_window->GetChildById(dest_container));
    return true;
  } else if (layer_) {
    ui::Layer* layer = layer_.get();
    root_window->GetChildById(src_container)->layer()->Remove(layer);
    root_window->GetChildById(dest_container)->layer()->Add(layer);
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
    settings.SetPreemptionStrategy(ui::LayerAnimator::ENQUEUE_NEW_ANIMATION);
    settings.AddObserver(new ShowWallpaperAnimationObserver(
        root_window_controller, widget_,
        Shell::GetInstance()->user_wallpaper_delegate()->
            ShouldShowInitialAnimation()));
    widget_->Show();
    widget_->GetNativeView()->SetName("DesktopBackgroundView");
  } else if (layer_)
    root_window_controller->OnWallpaperAnimationFinished(NULL);
}

AnimatingDesktopController::AnimatingDesktopController(
    DesktopBackgroundWidgetController* component) {
  controller_.reset(component);
}

AnimatingDesktopController::~AnimatingDesktopController() {
}

void AnimatingDesktopController::StopAnimating() {
  if (controller_) {
    ui::Layer* layer = controller_->layer() ? controller_->layer() :
        controller_->widget()->GetNativeView()->layer();
    layer->GetAnimator()->StopAnimating();
  }
}

DesktopBackgroundWidgetController* AnimatingDesktopController::GetController(
    bool pass_ownership) {
  if (pass_ownership)
    return controller_.release();
  return controller_.get();
}

}  // namespace internal
}  // namespace ash
