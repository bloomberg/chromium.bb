// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/overview/scoped_transform_overview_window.h"

#include "ash/screen_ash.h"
#include "ash/shell.h"
#include "ash/wm/overview/scoped_window_copy.h"
#include "ash/wm/window_state.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/root_window.h"
#include "ui/aura/window.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/views/corewm/window_animations.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

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

void SetTransformOnWindow(aura::Window* window,
                          const gfx::Transform& transform,
                          bool animate) {
  if (animate) {
    WindowSelectorAnimationSettings animation_settings(window);
    window->SetTransform(transform);
  } else {
    window->SetTransform(transform);
  }
}

gfx::Transform TranslateTransformOrigin(const gfx::Vector2d& new_origin,
                                        const gfx::Transform& transform) {
  gfx::Transform result;
  result.Translate(-new_origin.x(), -new_origin.y());
  result.PreconcatTransform(transform);
  result.Translate(new_origin.x(), new_origin.y());
  return result;
}

void SetTransformOnWindowAndAllTransientChildren(
    aura::Window* window,
    const gfx::Transform& transform,
    bool animate) {
  SetTransformOnWindow(window, transform, animate);

  aura::Window::Windows transient_children = window->transient_children();
  for (aura::Window::Windows::iterator iter = transient_children.begin();
       iter != transient_children.end(); ++iter) {
    aura::Window* transient_child = *iter;
    gfx::Rect window_bounds = window->bounds();
    gfx::Rect child_bounds = transient_child->bounds();
    gfx::Transform transient_window_transform(
        TranslateTransformOrigin(child_bounds.origin() - window_bounds.origin(),
                                 transform));
    SetTransformOnWindow(transient_child, transient_window_transform, animate);
  }
}

aura::Window* GetModalTransientParent(aura::Window* window) {
  if (window->GetProperty(aura::client::kModalKey) == ui::MODAL_TYPE_WINDOW)
    return window->transient_parent();
  return NULL;
}

}  // namespace

const int ScopedTransformOverviewWindow::kTransitionMilliseconds = 100;

ScopedTransformOverviewWindow::ScopedTransformOverviewWindow(
        aura::Window* window)
    : window_(window),
      minimized_(window->GetProperty(aura::client::kShowStateKey) ==
                 ui::SHOW_STATE_MINIMIZED),
      ignored_by_shelf_(ash::wm::GetWindowState(window)->ignored_by_shelf()),
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
    SetTransformOnWindowAndTransientChildren(original_transform_, true);
    if (minimized_ && window_->GetProperty(aura::client::kShowStateKey) !=
        ui::SHOW_STATE_MINIMIZED) {
      // Setting opacity 0 and visible false ensures that the property change
      // to SHOW_STATE_MINIMIZED will not animate the window from its original
      // bounds to the minimized position.
      // Hiding the window needs to be done before the target opacity is 0,
      // otherwise the layer's visibility will not be updated
      // (See VisibilityController::UpdateLayerVisibility).
      window_->Hide();
      window_->layer()->SetOpacity(0);
      window_->SetProperty(aura::client::kShowStateKey,
                           ui::SHOW_STATE_MINIMIZED);
    }
    ash::wm::GetWindowState(window_)->set_ignored_by_shelf(ignored_by_shelf_);
  }
}

bool ScopedTransformOverviewWindow::Contains(const aura::Window* target) const {
  if (window_copy_ && window_copy_->GetWindow()->Contains(target))
    return true;
  aura::Window* window = window_;
  while (window) {
    if (window->Contains(target))
      return true;
    window = GetModalTransientParent(window);
  }
  return false;
}

gfx::Rect ScopedTransformOverviewWindow::GetBoundsInScreen() const {
  gfx::Rect bounds;
  aura::Window* window = window_;
  while (window) {
    bounds.Union(ScreenAsh::ConvertRectToScreen(window->parent(),
                                                window->GetTargetBounds()));
    window = GetModalTransientParent(window);
  }
  return bounds;
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

gfx::Rect ScopedTransformOverviewWindow::ShrinkRectToFitPreservingAspectRatio(
    const gfx::Rect& rect,
    const gfx::Rect& bounds) {
  DCHECK(!rect.IsEmpty());
  DCHECK(!bounds.IsEmpty());
  float scale = std::min(1.0f,
      std::min(static_cast<float>(bounds.width()) / rect.width(),
               static_cast<float>(bounds.height()) / rect.height()));
  return gfx::Rect(bounds.x() + 0.5 * (bounds.width() - scale * rect.width()),
                   bounds.y() + 0.5 * (bounds.height() - scale * rect.height()),
                   rect.width() * scale,
                   rect.height() * scale);
}

gfx::Transform ScopedTransformOverviewWindow::GetTransformForRect(
    const gfx::Rect& src_rect,
    const gfx::Rect& dst_rect) {
  DCHECK(!src_rect.IsEmpty());
  DCHECK(!dst_rect.IsEmpty());
  gfx::Transform transform;
  transform.Translate(dst_rect.x() - src_rect.x(),
                      dst_rect.y() - src_rect.y());
  transform.Scale(static_cast<float>(dst_rect.width()) / src_rect.width(),
                  static_cast<float>(dst_rect.height()) / src_rect.height());
  return transform;
}

void ScopedTransformOverviewWindow::SetTransform(
    aura::RootWindow* root_window,
    const gfx::Transform& transform,
    bool animate) {
  DCHECK(overview_started_);

  // If the window bounds have changed and a copy of the window is being
  // shown on another display, forcibly recreate the copy.
  if (window_copy_ && window_copy_->GetWindow()->GetBoundsInScreen() !=
      window_->GetBoundsInScreen()) {
    DCHECK_NE(window_->GetRootWindow(), root_window);
    // TODO(flackr): If only the position changed and not the size, update the
    // existing window_copy_'s position and continue to use it.
    window_copy_.reset();
  }

  if (root_window != window_->GetRootWindow() && !window_copy_) {
    // TODO(flackr): Create copies of the transient children and transient
    // parent windows as well. Currently they will only be visible on the
    // window's initial display.
    window_copy_.reset(new ScopedWindowCopy(root_window, window_));
  }
  SetTransformOnWindowAndTransientChildren(transform, animate);
}

void ScopedTransformOverviewWindow::SetTransformOnWindowAndTransientChildren(
    const gfx::Transform& transform,
    bool animate) {
  gfx::Point origin(GetBoundsInScreen().origin());
  aura::Window* window = window_;
  while (window->transient_parent())
    window = window->transient_parent();
  if (window_copy_) {
    SetTransformOnWindow(
        window_copy_->GetWindow(),
        TranslateTransformOrigin(ScreenAsh::ConvertRectToScreen(
            window_->parent(), window_->GetTargetBounds()).origin() - origin,
            transform),
        animate);
  }
  SetTransformOnWindowAndAllTransientChildren(
      window,
      TranslateTransformOrigin(ScreenAsh::ConvertRectToScreen(
          window->parent(), window->GetTargetBounds()).origin() - origin,
          transform),
      animate);
}

void ScopedTransformOverviewWindow::PrepareForOverview() {
  DCHECK(!overview_started_);
  overview_started_ = true;
  ash::wm::GetWindowState(window_)->set_ignored_by_shelf(true);
  RestoreWindow();
}

}  // namespace ash
