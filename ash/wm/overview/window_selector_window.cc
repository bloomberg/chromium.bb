// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/overview/window_selector_window.h"

#include "ash/shell.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/client/screen_position_client.h"
#include "ui/aura/root_window.h"
#include "ui/aura/window.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/display.h"
#include "ui/gfx/interpolated_transform.h"
#include "ui/gfx/transform_util.h"
#include "ui/views/corewm/shadow_types.h"
#include "ui/views/corewm/window_animations.h"
#include "ui/views/corewm/window_util.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

const int kOverviewWindowTransitionMilliseconds = 100;

// Creates a copy of |window| with |recreated_layer| in the |target_root|.
views::Widget* CreateCopyOfWindow(aura::RootWindow* target_root,
                                  aura::Window* src_window,
                                  ui::Layer* recreated_layer) {
  views::Widget* widget = new views::Widget;
  views::Widget::InitParams params(views::Widget::InitParams::TYPE_POPUP);
  params.opacity = views::Widget::InitParams::TRANSLUCENT_WINDOW;
  params.parent = src_window->parent();
  params.can_activate = false;
  params.keep_on_top = true;
  widget->set_focus_on_creation(false);
  widget->Init(params);
  widget->SetVisibilityChangedAnimationsEnabled(false);
  std::string name = src_window->name() + " (Copy)";
  widget->GetNativeWindow()->SetName(name);
  views::corewm::SetShadowType(widget->GetNativeWindow(),
                               views::corewm::SHADOW_TYPE_RECTANGULAR);

  // Set the bounds in the target root window.
  gfx::Display target_display =
      Shell::GetScreen()->GetDisplayNearestWindow(target_root);
  aura::client::ScreenPositionClient* screen_position_client =
      aura::client::GetScreenPositionClient(src_window->GetRootWindow());
  if (screen_position_client && target_display.is_valid()) {
    screen_position_client->SetBounds(widget->GetNativeWindow(),
        src_window->GetBoundsInScreen(), target_display);
  } else {
    widget->SetBounds(src_window->GetBoundsInScreen());
  }
  widget->StackAbove(src_window);

  // Move the |recreated_layer| to the newly created window.
  recreated_layer->set_delegate(src_window->layer()->delegate());
  gfx::Rect layer_bounds = recreated_layer->bounds();
  layer_bounds.set_origin(gfx::Point(0, 0));
  recreated_layer->SetBounds(layer_bounds);
  recreated_layer->SetVisible(false);
  recreated_layer->parent()->Remove(recreated_layer);

  aura::Window* window = widget->GetNativeWindow();
  recreated_layer->SetVisible(true);
  window->layer()->Add(recreated_layer);
  window->layer()->StackAtTop(recreated_layer);
  window->layer()->SetOpacity(1);
  window->Show();
  return widget;
}

// An observer which closes the widget and deletes the layer after an
// animation finishes.
class CleanupWidgetAfterAnimationObserver : public ui::LayerAnimationObserver {
 public:
  CleanupWidgetAfterAnimationObserver(views::Widget* widget, ui::Layer* layer);

  virtual void OnLayerAnimationEnded(
      ui::LayerAnimationSequence* sequence) OVERRIDE;
  virtual void OnLayerAnimationAborted(
      ui::LayerAnimationSequence* sequence) OVERRIDE;
  virtual void OnLayerAnimationScheduled(
      ui::LayerAnimationSequence* sequence) OVERRIDE;

 protected:
  virtual ~CleanupWidgetAfterAnimationObserver();

 private:
  views::Widget* widget_;
  ui::Layer* layer_;

  DISALLOW_COPY_AND_ASSIGN(CleanupWidgetAfterAnimationObserver);
};

CleanupWidgetAfterAnimationObserver::CleanupWidgetAfterAnimationObserver(
        views::Widget* widget,
        ui::Layer* layer)
    : widget_(widget),
      layer_(layer) {
  widget_->GetNativeWindow()->layer()->GetAnimator()->AddObserver(this);
}

void CleanupWidgetAfterAnimationObserver::OnLayerAnimationEnded(
    ui::LayerAnimationSequence* sequence) {
  delete this;
}

void CleanupWidgetAfterAnimationObserver::OnLayerAnimationAborted(
    ui::LayerAnimationSequence* sequence) {
  delete this;
}

void CleanupWidgetAfterAnimationObserver::OnLayerAnimationScheduled(
    ui::LayerAnimationSequence* sequence) {
}

CleanupWidgetAfterAnimationObserver::~CleanupWidgetAfterAnimationObserver() {
  widget_->GetNativeWindow()->layer()->GetAnimator()->RemoveObserver(this);
  widget_->Close();
  widget_ = NULL;
  if (layer_) {
    views::corewm::DeepDeleteLayers(layer_);
    layer_ = NULL;
  }
}

// The animation settings used for window selector animations.
class WindowSelectorAnimationSettings
    : public ui::ScopedLayerAnimationSettings {
 public:
  WindowSelectorAnimationSettings(aura::Window* window) :
      ui::ScopedLayerAnimationSettings(window->layer()->GetAnimator()) {
    SetPreemptionStrategy(
        ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
    SetTransitionDuration(base::TimeDelta::FromMilliseconds(
        kOverviewWindowTransitionMilliseconds));
  }

  virtual ~WindowSelectorAnimationSettings() {
  }
};

}  // namespace

WindowSelectorWindow::WindowSelectorWindow(aura::Window* window)
    : window_(window),
      window_copy_(NULL),
      layer_(NULL),
      minimized_(window->GetProperty(aura::client::kShowStateKey) ==
                 ui::SHOW_STATE_MINIMIZED),
      original_transform_(window->layer()->GetTargetTransform()) {
}

WindowSelectorWindow::~WindowSelectorWindow() {
  if (window_) {
    WindowSelectorAnimationSettings animation_settings(window_);
    gfx::Transform transform;
    window_->SetTransform(original_transform_);
    if (minimized_ && window_->GetProperty(aura::client::kShowStateKey) !=
        ui::SHOW_STATE_MINIMIZED) {
      // Setting opacity 0 and visible false ensures that the property change
      // to SHOW_STATE_MINIMIZED will not animate the window from its original
      // bounds to the minimized position.
      window_->layer()->SetOpacity(0);
      window_->layer()->SetVisible(false);
      window_->SetProperty(aura::client::kShowStateKey,
                           ui::SHOW_STATE_MINIMIZED);
    }
  }
  // If a copy of the window was created, clean it up.
  if (window_copy_) {
    if (window_) {
      // If the initial window wasn't destroyed, the copy needs to be animated
      // out. CleanupWidgetAfterAnimationObserver will destroy the widget and
      // layer after the animation is complete.
      new CleanupWidgetAfterAnimationObserver(window_copy_, layer_);
      WindowSelectorAnimationSettings animation_settings(
          window_copy_->GetNativeWindow());
      window_copy_->GetNativeWindow()->SetTransform(original_transform_);
    } else {
      window_copy_->Close();
      if (layer_)
        views::corewm::DeepDeleteLayers(layer_);
    }
    window_copy_ = NULL;
    layer_ = NULL;
  }
}

bool WindowSelectorWindow::Contains(const aura::Window* window) const {
  if (window_copy_ && window_copy_->GetNativeWindow()->Contains(window))
    return true;
  return window_->Contains(window);
}

void WindowSelectorWindow::RestoreWindowOnExit() {
  minimized_ = false;
  original_transform_ = gfx::Transform();
}

void WindowSelectorWindow::OnWindowDestroyed() {
  window_ = NULL;
}

void WindowSelectorWindow::TransformToFitBounds(
    aura::RootWindow* root_window,
    const gfx::Rect& target_bounds) {
  if (minimized_ && window_->GetProperty(aura::client::kShowStateKey) ==
      ui::SHOW_STATE_MINIMIZED) {
    window_->Show();
  }
  fit_bounds_ = target_bounds;
  const gfx::Rect bounds = window_->GetBoundsInScreen();
  float scale = std::min(1.0f,
      std::min(static_cast<float>(target_bounds.width()) / bounds.width(),
               static_cast<float>(target_bounds.height()) / bounds.height()));
  gfx::Transform transform;
  gfx::Vector2d offset(
      0.5 * (target_bounds.width() - scale * bounds.width()),
      0.5 * (target_bounds.height() - scale * bounds.height()));
  transform.Translate(target_bounds.x() - bounds.x() + offset.x(),
                      target_bounds.y() - bounds.y() + offset.y());
  transform.Scale(scale, scale);
  if (root_window != window_->GetRootWindow()) {
    if (!window_copy_) {
      DCHECK(!layer_);
      layer_ = views::corewm::RecreateWindowLayers(window_, true);
      window_copy_ = CreateCopyOfWindow(root_window, window_, layer_);
    }
    WindowSelectorAnimationSettings animation_settings(
        window_copy_->GetNativeWindow());
    window_copy_->GetNativeWindow()->SetTransform(transform);
  }
  WindowSelectorAnimationSettings animation_settings(window_);
  window_->SetTransform(transform);
}

}  // namespace ash
