// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/overview/scoped_transform_overview_window.h"

#include "ash/screen_util.h"
#include "ash/shell_window_ids.h"
#include "ash/wm/overview/scoped_window_copy.h"
#include "ash/wm/overview/window_selector_item.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/client/screen_position_client.h"
#include "ui/aura/window.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/animation/tween.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/window_animations.h"
#include "ui/wm/core/window_util.h"

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
    SetTweenType(gfx::Tween::FAST_OUT_SLOW_IN);
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

  aura::Window::Windows transient_children =
      ::wm::GetTransientChildren(window);
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
    return ::wm::GetTransientParent(window);
  return NULL;
}

}  // namespace

const int ScopedTransformOverviewWindow::kTransitionMilliseconds = 200;

ScopedTransformOverviewWindow::ScopedTransformOverviewWindow(
        aura::Window* window)
    : window_(window),
      minimized_(window->GetProperty(aura::client::kShowStateKey) ==
                 ui::SHOW_STATE_MINIMIZED),
      ignored_by_shelf_(ash::wm::GetWindowState(window)->ignored_by_shelf()),
      overview_started_(false),
      original_transform_(window->layer()->GetTargetTransform()),
      opacity_(window->layer()->GetTargetOpacity()) {
}

ScopedTransformOverviewWindow::~ScopedTransformOverviewWindow() {
  if (window_) {
    WindowSelectorAnimationSettings animation_settings(window_);
    gfx::Transform transform;
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
    window_->layer()->SetOpacity(opacity_);
  }
}

bool ScopedTransformOverviewWindow::Contains(const aura::Window* target) const {
  for (ScopedVector<ScopedWindowCopy>::const_iterator iter =
      window_copies_.begin(); iter != window_copies_.end(); ++iter) {
    if ((*iter)->GetWindow()->Contains(target))
      return true;
  }
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
    bounds.Union(ScreenUtil::ConvertRectToScreen(window->parent(),
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
  opacity_ = 1;
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
    aura::Window* root_window,
    const gfx::Transform& transform,
    bool animate) {
  DCHECK(overview_started_);

  if (root_window != window_->GetRootWindow()) {
    if (!window_copies_.empty()) {
      bool bounds_or_hierarchy_changed = false;
      aura::Window* window = window_;
      for (ScopedVector<ScopedWindowCopy>::reverse_iterator iter =
               window_copies_.rbegin();
           !bounds_or_hierarchy_changed && iter != window_copies_.rend();
           ++iter, window = GetModalTransientParent(window)) {
        if (!window) {
          bounds_or_hierarchy_changed = true;
        } else if ((*iter)->GetWindow()->GetBoundsInScreen() !=
                window->GetBoundsInScreen()) {
          bounds_or_hierarchy_changed = true;
        }
      }
      // Clearing the window copies array will force it to be recreated.
      // TODO(flackr): If only the position changed and not the size,
      // update the existing window copy's position and continue to use it.
      if (bounds_or_hierarchy_changed)
        window_copies_.clear();
    }
    if (window_copies_.empty()) {
      // TODO(flackr): Create copies of the transient children windows as well.
      // Currently they will only be visible on the window's initial display.
      CopyWindowAndTransientParents(root_window, window_);
    }
  }
  SetTransformOnWindowAndTransientChildren(transform, animate);
}

void ScopedTransformOverviewWindow::CopyWindowAndTransientParents(
    aura::Window* target_root,
    aura::Window* window) {
  aura::Window* modal_parent = GetModalTransientParent(window);
  if (modal_parent)
    CopyWindowAndTransientParents(target_root, modal_parent);
  window_copies_.push_back(new ScopedWindowCopy(target_root, window));
}

void ScopedTransformOverviewWindow::SetTransformOnWindowAndTransientChildren(
    const gfx::Transform& transform,
    bool animate) {
  gfx::Point origin(GetBoundsInScreen().origin());
  aura::Window* window = window_;
  while (::wm::GetTransientParent(window))
    window = ::wm::GetTransientParent(window);
  for (ScopedVector<ScopedWindowCopy>::const_iterator iter =
      window_copies_.begin(); iter != window_copies_.end(); ++iter) {
    SetTransformOnWindow(
        (*iter)->GetWindow(),
        TranslateTransformOrigin(ScreenUtil::ConvertRectToScreen(
            (*iter)->GetWindow()->parent(),
            (*iter)->GetWindow()->GetTargetBounds()).origin() - origin,
            transform),
        animate);
  }
  SetTransformOnWindowAndAllTransientChildren(
      window,
      TranslateTransformOrigin(ScreenUtil::ConvertRectToScreen(
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
