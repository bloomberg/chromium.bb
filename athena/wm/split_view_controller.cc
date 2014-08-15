// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/wm/split_view_controller.h"

#include <cmath>

#include "athena/wm/public/window_list_provider.h"
#include "athena/wm/public/window_manager.h"
#include "base/bind.h"
#include "ui/aura/window.h"
#include "ui/compositor/closure_animation_observer.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/events/event_handler.h"
#include "ui/gfx/display.h"
#include "ui/gfx/screen.h"

namespace athena {

SplitViewController::SplitViewController(
    aura::Window* container,
    WindowListProvider* window_list_provider,
    WindowManager* window_manager)
    : state_(INACTIVE),
      container_(container),
      window_manager_(window_manager),
      window_list_provider_(window_list_provider),
      current_activity_window_(NULL),
      left_window_(NULL),
      right_window_(NULL),
      separator_position_(0),
      weak_factory_(this) {
  window_manager->AddObserver(this);
}

SplitViewController::~SplitViewController() {
  window_manager_->RemoveObserver(this);
}

bool SplitViewController::IsSplitViewModeActive() const {
  return state_ == ACTIVE;
}

void SplitViewController::ActivateSplitMode(aura::Window* left,
                                            aura::Window* right) {
  aura::Window::Windows windows = window_list_provider_->GetWindowList();
  aura::Window::Windows::reverse_iterator iter = windows.rbegin();
  if (state_ == ACTIVE) {
    if (!left)
      left = left_window_;
    if (!right)
      right = right_window_;
  }

  if (!left && iter != windows.rend()) {
    left = *iter;
    iter++;
    if (left == right && iter != windows.rend()) {
      left = *iter;
      iter++;
    }
  }

  if (!right && iter != windows.rend()) {
    right = *iter;
    iter++;
    if (right == left && iter != windows.rend()) {
      right = *iter;
      iter++;
    }
  }

  state_ = ACTIVE;
  left_window_ = left;
  right_window_ = right;
  container_->StackChildAtTop(right_window_);
  container_->StackChildAtTop(left_window_);
  UpdateLayout(true);
}

void SplitViewController::UpdateLayout(bool animate) {
  if  (!left_window_)
    return;
  CHECK(right_window_);
  gfx::Transform left_transform;
  gfx::Transform right_transform;
  int container_width = container_->GetBoundsInScreen().width();
  if (state_ == ACTIVE) {
    // This method should only be called once in ACTIVE state when
    // the left and rightwindows are still full screen and need to be resized.
    CHECK_EQ(left_window_->bounds().width(), container_width);
    CHECK_EQ(right_window_->bounds().width(), container_width);
    // Windows should be resized via an animation when entering the ACTIVE
    // state.
    CHECK(animate);
    // We scale the windows here, but when the animation finishes, we reset
    // the scaling and update the window bounds to the proper size - see
    // OnAnimationCompleted().
    left_transform.Scale(.5, 1);
    right_transform.Scale(.5, 1);
    right_transform.Translate(container_width, 0);
  } else {
    left_transform.Translate(separator_position_ - container_width, 0);
    right_transform.Translate(separator_position_, 0);
  }
  left_window_->Show();
  right_window_->Show();
  SetWindowTransform(left_window_, left_transform, animate);
  SetWindowTransform(right_window_, right_transform, animate);
}

void SplitViewController::SetWindowTransform(aura::Window* window,
                                             const gfx::Transform& transform,
                                             bool animate) {
  if (animate) {
    scoped_refptr<ui::LayerAnimator> animator = window->layer()->GetAnimator();
    ui::ScopedLayerAnimationSettings settings(animator);
    settings.SetPreemptionStrategy(
        ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
    settings.AddObserver(new ui::ClosureAnimationObserver(
        base::Bind(&SplitViewController::OnAnimationCompleted,
                   weak_factory_.GetWeakPtr(),
                   window)));
    window->SetTransform(transform);
  } else {
    window->SetTransform(transform);
  }
}

void SplitViewController::OnAnimationCompleted(aura::Window* window) {
  DCHECK(window == left_window_ || window == right_window_);
  if (state_ == ACTIVE) {
    gfx::Rect window_bounds = gfx::Rect(container_->bounds().size());
    int container_width = window_bounds.width();
    window_bounds.set_width(container_width / 2);
    window->SetTransform(gfx::Transform());
    if (window == left_window_) {
      left_window_->SetBounds(window_bounds);
    } else {
      window_bounds.set_x(container_width / 2);
      right_window_->SetBounds(window_bounds);
    }
  } else {
    int container_width = container_->bounds().width();
    window->SetTransform(gfx::Transform());
    if (window == left_window_) {
      if (separator_position_ == 0)
        left_window_->Hide();
      if (state_ == INACTIVE)
        left_window_ = NULL;
    } else {
      if (separator_position_ == container_width)
        right_window_->Hide();
      if (state_ == INACTIVE)
        right_window_ = NULL;
    }
  }
}

void SplitViewController::UpdateSeparatorPositionFromScrollDelta(float delta) {
  gfx::Screen* screen = gfx::Screen::GetScreenFor(container_);
  const gfx::Rect& display_bounds =
      screen->GetDisplayNearestWindow(container_).bounds();
  gfx::Rect container_bounds = container_->GetBoundsInScreen();
  separator_position_ =
      delta > 0 ? ((int)delta) + display_bounds.x() - container_bounds.x()
                : display_bounds.right() - container_bounds.x() + delta;
}

aura::Window* SplitViewController::GetCurrentActivityWindow() {
  if (!current_activity_window_) {
    aura::Window::Windows windows = window_list_provider_->GetWindowList();
    if (windows.empty())
      return NULL;
    current_activity_window_ = windows.back();
  }
  return current_activity_window_;
}

///////////////////////////////////////////////////////////////////////////////
// Begin BezelController::ScrollDelegate overrides.
void SplitViewController::ScrollBegin(BezelController::Bezel bezel,
                                      float delta) {
  if (!CanScroll())
    return;
  state_ = SCROLLING;
  aura::Window* current_window = GetCurrentActivityWindow();
  CHECK(current_window);

  aura::Window::Windows windows = window_list_provider_->GetWindowList();
  CHECK(windows.size() >= 2);
  aura::Window::Windows::const_iterator it =
      std::find(windows.begin(), windows.end(), current_window);
  CHECK(it != windows.end());

  if (delta > 0) {
    right_window_ = current_window;
    // reverse iterator points to the position before normal iterator |it|
    aura::Window::Windows::const_reverse_iterator rev_it(it);
    // circle to end if needed.
    left_window_ = rev_it == windows.rend() ? windows.back() : *(rev_it);
  } else {
    left_window_ = current_window;
    ++it;
    // circle to front if needed.
    right_window_ = it == windows.end() ? windows.front() : *it;
  }

  CHECK(left_window_);
  CHECK(right_window_);

  // TODO(oshima|mfomitchev): crbug.com/388362
  // Until we are properly hiding off-screen windows in window manager:
  // Loop through all windows and hide them
  for (it = windows.begin(); it != windows.end(); ++it) {
    if (*it != left_window_ && *it != right_window_)
      (*it)->Hide();
  }

  UpdateSeparatorPositionFromScrollDelta(delta);
  UpdateLayout(false);
}

// Max distance from the scroll end position to the middle of the screen where
// we would go into the split view mode.
const int kMaxDistanceFromMiddle = 120;
void SplitViewController::ScrollEnd() {
  if (state_ != SCROLLING)
    return;

  int container_width = container_->GetBoundsInScreen().width();
  if (std::abs(container_width / 2 - separator_position_) <=
      kMaxDistanceFromMiddle) {
    state_ = ACTIVE;
    separator_position_ = container_width / 2;
  } else if (separator_position_ < container_width / 2) {
    separator_position_ = 0;
    current_activity_window_ = right_window_;
    state_ = INACTIVE;
  } else {
    separator_position_ = container_width;
    current_activity_window_ = left_window_;
    state_ = INACTIVE;
  }
  UpdateLayout(true);
}

void SplitViewController::ScrollUpdate(float delta) {
  if (state_ != SCROLLING)
    return;
  UpdateSeparatorPositionFromScrollDelta(delta);
  UpdateLayout(false);
}

bool SplitViewController::CanScroll() {
  // TODO(mfomitchev): return false in vertical orientation, in full screen.
  bool result = (!window_manager_->IsOverviewModeActive() &&
                 !IsSplitViewModeActive() &&
                 window_list_provider_->GetWindowList().size() >= 2);
  return result;
}

///////////////////////////////////////////////////////////////////////////////
// WindowManagerObserver overrides
void SplitViewController::OnOverviewModeEnter() {
  if (state_ == ACTIVE) {
    CHECK(left_window_);
    CHECK(right_window_);
    window_list_provider_->MoveToFront(right_window_);
    window_list_provider_->MoveToFront(left_window_);
    // TODO(mfomitchev): This shouldn't be done here, but the overview mode's
    // transition animation currently looks bad if the starting transform of
    // any window is not gfx::Transform().
    right_window_->SetTransform(gfx::Transform());
  } else if (current_activity_window_) {
    window_list_provider_->MoveToFront(current_activity_window_);
  }
  state_ = INACTIVE;
  left_window_ = NULL;
  right_window_ = NULL;
  current_activity_window_ = NULL;
}

void SplitViewController::OnOverviewModeExit() {
}

}  // namespace athena
