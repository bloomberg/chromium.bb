// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/overview/scoped_transform_overview_window.h"

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
        ScopedTransformOverviewWindow::kTransitionMilliseconds));
  }

  virtual ~WindowSelectorAnimationSettings() {
  }
};

}  // namespace

const int ScopedTransformOverviewWindow::kTransitionMilliseconds = 100;

ScopedTransformOverviewWindow::ScopedTransformOverviewWindow(
        aura::Window* window)
    : window_(window),
      window_copy_(NULL),
      layer_(NULL),
      minimized_(window->GetProperty(aura::client::kShowStateKey) ==
                 ui::SHOW_STATE_MINIMIZED),
      overview_started_(false),
      original_transform_(window->layer()->GetTargetTransform()) {
}

ScopedTransformOverviewWindow::~ScopedTransformOverviewWindow() {
  if (window_) {
    WindowSelectorAnimationSettings animation_settings(window_);
    gfx::Transform transform;
    // If the initial window wasn't destroyed and we have copied the window
    // layer, the copy needs to be animated out.
    // CleanupWidgetAfterAnimationObserver will destroy the widget and
    // layer after the animation is complete.
    if (window_copy_)
      new CleanupWidgetAfterAnimationObserver(window_copy_, layer_);
    AnimateTransformOnWindowAndTransientChildren(original_transform_);
    window_copy_ = NULL;
    layer_ = NULL;
    if (minimized_ && window_->GetProperty(aura::client::kShowStateKey) !=
        ui::SHOW_STATE_MINIMIZED) {
      // Setting opacity 0 and visible false ensures that the property change
      // to SHOW_STATE_MINIMIZED will not animate the window from its original
      // bounds to the minimized position.
      window_->layer()->SetOpacity(0);
      window_->Hide();
      window_->SetProperty(aura::client::kShowStateKey,
                           ui::SHOW_STATE_MINIMIZED);
    }
  } else if (window_copy_) {
    // If this class still owns a copy of the window, clean up the copy. This
    // will be the case if the window was destroyed.
    window_copy_->Close();
    if (layer_)
      views::corewm::DeepDeleteLayers(layer_);
    window_copy_ = NULL;
    layer_ = NULL;
  }
}

bool ScopedTransformOverviewWindow::Contains(const aura::Window* window) const {
  if (window_copy_ && window_copy_->GetNativeWindow()->Contains(window))
    return true;
  return window_->Contains(window);
}

void ScopedTransformOverviewWindow::RestoreWindow() {
  if (minimized_ && window_->GetProperty(aura::client::kShowStateKey) ==
      ui::SHOW_STATE_MINIMIZED) {
    window_->Show();
  }
}

void ScopedTransformOverviewWindow::RestoreWindowOnExit() {
  minimized_ = false;
  original_transform_ = gfx::Transform();
}

void ScopedTransformOverviewWindow::OnWindowDestroyed() {
  window_ = NULL;
}

gfx::Transform ScopedTransformOverviewWindow::
    GetTransformForRectPreservingAspectRatio(const gfx::Rect& rect,
                                             const gfx::Rect& bounds) {
  DCHECK(!rect.IsEmpty());
  DCHECK(!bounds.IsEmpty());
  float scale = std::min(1.0f,
      std::min(static_cast<float>(bounds.width()) / rect.width(),
               static_cast<float>(bounds.height()) / rect.height()));
  gfx::Transform transform;
  gfx::Vector2d offset(
      0.5 * (bounds.width() - scale * rect.width()),
      0.5 * (bounds.height() - scale * rect.height()));
  transform.Translate(bounds.x() - rect.x() + offset.x(),
                      bounds.y() - rect.y() + offset.y());
  transform.Scale(scale, scale);
  return transform;
}

void ScopedTransformOverviewWindow::SetTransform(
    aura::RootWindow* root_window,
    const gfx::Transform& transform) {
  // If this is the first transform, perform one-time window modifications
  // necessary for overview mode.
  if (!overview_started_) {
    OnOverviewStarted();
    overview_started_ = true;
  }

  if (root_window != window_->GetRootWindow() && !window_copy_) {
    DCHECK(!layer_);
    layer_ = views::corewm::RecreateWindowLayers(window_, true);
    window_copy_ = CreateCopyOfWindow(root_window, window_, layer_);
  }
  AnimateTransformOnWindowAndTransientChildren(transform);
}

void ScopedTransformOverviewWindow::
    AnimateTransformOnWindowAndTransientChildren(
        const gfx::Transform& transform) {
  WindowSelectorAnimationSettings animation_settings(window_);
  window_->SetTransform(transform);

  if (window_copy_) {
    WindowSelectorAnimationSettings animation_settings(
        window_copy_->GetNativeWindow());
    window_copy_->GetNativeWindow()->SetTransform(transform);
  }

  // TODO(flackr): Create copies of the transient children windows as well.
  // Currently they will only be visible on the window's initial display.
  aura::Window::Windows transient_children = window_->transient_children();
  for (aura::Window::Windows::iterator iter = transient_children.begin();
       iter != transient_children.end(); ++iter) {
    aura::Window* transient_child = *iter;
    WindowSelectorAnimationSettings animation_settings(transient_child);
    gfx::Transform transient_window_transform;
    gfx::Rect window_bounds = window_->bounds();
    gfx::Rect child_bounds = transient_child->bounds();
    transient_window_transform.Translate(window_bounds.x() - child_bounds.x(),
                                         window_bounds.y() - child_bounds.y());
    transient_window_transform.PreconcatTransform(transform);
    transient_window_transform.Translate(child_bounds.x() - window_bounds.x(),
                                         child_bounds.y() - window_bounds.y());
    transient_child->SetTransform(transient_window_transform);
  }
}

void ScopedTransformOverviewWindow::OnOverviewStarted() {
  RestoreWindow();
}

}  // namespace ash
