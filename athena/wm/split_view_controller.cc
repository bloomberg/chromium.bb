// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/wm/split_view_controller.h"

#include <cmath>

#include "athena/screen/public/screen_manager.h"
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
#include "ui/wm/core/window_util.h"

namespace athena {

namespace {

// Returns a target transform which is suitable for animating a windows's
// bounds.
gfx::Transform GetTargetTransformForBoundsAnimation(const gfx::Rect& from,
                                                    const gfx::Rect& to) {
  gfx::Transform transform;
  transform.Translate(to.x() - from.x(), to.y() - from.y());
  transform.Scale(to.width() / static_cast<float>(from.width()),
                  to.height() / static_cast<float>(from.height()));
  return transform;
}

bool IsLandscapeOrientation(gfx::Display::Rotation rotation) {
  return rotation == gfx::Display::ROTATE_0 ||
         rotation == gfx::Display::ROTATE_180;
}

}  // namespace

SplitViewController::SplitViewController(
    aura::Window* container,
    WindowListProvider* window_list_provider)
    : state_(INACTIVE),
      container_(container),
      window_list_provider_(window_list_provider),
      left_window_(NULL),
      right_window_(NULL),
      separator_position_(0),
      weak_factory_(this) {
}

SplitViewController::~SplitViewController() {
}

bool SplitViewController::IsSplitViewModeActive() const {
  return state_ == ACTIVE;
}

void SplitViewController::ActivateSplitMode(aura::Window* left,
                                            aura::Window* right) {
  aura::Window::Windows windows = window_list_provider_->GetWindowList();
  aura::Window::Windows::reverse_iterator iter = windows.rbegin();
  if (state_ == ACTIVE) {
    if (left_window_ == right)
      left_window_ = left;
    if (right_window_ == left)
      right_window_ = right;

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

  SetState(ACTIVE);
  if (right_window_ != right) {
    right_window_ = right;
    // Since the |window_list_provider_| controls directly the order of windows,
    // it needs to change the window order accordingly.
    window_list_provider_->MoveToFront(right_window_);
  }
  if (left_window_ != left) {
    left_window_ = left;
    // Since the |window_list_provider_| controls directly the order of windows,
    // it needs to change the window order accordingly.
    window_list_provider_->MoveToFront(left_window_);
  }
  UpdateLayout(true);
}

void SplitViewController::ReplaceWindow(aura::Window* window,
                                        aura::Window* replace_with) {
  CHECK(IsSplitViewModeActive());
  CHECK(replace_with);
  CHECK(window == left_window_ || window == right_window_);
  CHECK(replace_with != left_window_ && replace_with != right_window_);
#if !defined(NDEBUG)
  aura::Window::Windows windows = window_list_provider_->GetWindowList();
  DCHECK(std::find(windows.begin(), windows.end(), replace_with) !=
         windows.end());
#endif

  if (window == left_window_)
    left_window_ = replace_with;
  else
    right_window_ = replace_with;
  wm::ActivateWindow(replace_with);
  UpdateLayout(false);
  window->SetTransform(gfx::Transform());
  window->Hide();
}

void SplitViewController::DeactivateSplitMode() {
  CHECK_EQ(ACTIVE, state_);
  SetState(INACTIVE);
  UpdateLayout(false);
  left_window_ = right_window_ = NULL;
}

gfx::Rect SplitViewController::GetLeftTargetBounds() {
  gfx::Rect work_area =
      gfx::Screen::GetNativeScreen()->GetPrimaryDisplay().work_area();
  return gfx::Rect(0, 0, container_->bounds().width() / 2, work_area.height());
}

gfx::Rect SplitViewController::GetRightTargetBounds() {
  gfx::Rect work_area =
      gfx::Screen::GetNativeScreen()->GetPrimaryDisplay().work_area();
  int container_width = container_->bounds().width();
  return gfx::Rect(
      container_width / 2, 0, container_width / 2, work_area.height());
}

void SplitViewController::SetState(SplitViewController::State state) {
  if (state_ == state)
    return;

  state_ = state;
  ScreenManager::Get()->SetRotationLocked(state_ != INACTIVE);
}

void SplitViewController::UpdateLayout(bool animate) {
  CHECK(left_window_);
  CHECK(right_window_);

  // Splitview can be activated from SplitViewController::ActivateSplitMode or
  // SplitViewController::ScrollEnd. Additionally we don't want to rotate the
  // screen while engaging splitview (i.e. state_ == SCROLLING).
  if (state_ == INACTIVE && !animate) {
    if (!wm::IsActiveWindow(left_window_))
      left_window_->Hide();
    if (!wm::IsActiveWindow(right_window_))
      right_window_->Hide();
    SetWindowTransforms(gfx::Transform(), gfx::Transform(), false);
    return;
  }

  left_window_->Show();
  right_window_->Show();
  if (state_ == ACTIVE) {
    if (animate) {
      gfx::Transform left_transform = GetTargetTransformForBoundsAnimation(
          left_window_->bounds(), GetLeftTargetBounds());
      gfx::Transform right_transform = GetTargetTransformForBoundsAnimation(
          right_window_->bounds(), GetRightTargetBounds());
      SetWindowTransforms(left_transform, right_transform, true);
    } else {
      left_window_->SetBounds(GetLeftTargetBounds());
      right_window_->SetBounds(GetRightTargetBounds());
      SetWindowTransforms(gfx::Transform(), gfx::Transform(), false);
    }
  } else {
    gfx::Transform left_transform;
    left_transform.Translate(separator_position_ - container_->bounds().width(),
                             0);
    gfx::Transform right_transform;
    right_transform.Translate(separator_position_, 0);
    SetWindowTransforms(left_transform, right_transform, animate);
  }
  // Note: |left_window_| and |right_window_| may be NULL if calling
  // SetWindowTransforms():
  // - caused the in-progress animation to abort.
  // - started a zero duration animation.
}

void SplitViewController::SetWindowTransforms(
    const gfx::Transform& left_transform,
    const gfx::Transform& right_transform,
    bool animate) {
  if (animate) {
    ui::ScopedLayerAnimationSettings left_settings(
        left_window_->layer()->GetAnimator());
    left_settings.SetPreemptionStrategy(
        ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
    left_window_->SetTransform(left_transform);

    ui::ScopedLayerAnimationSettings right_settings(
        right_window_->layer()->GetAnimator());
    right_settings.SetPreemptionStrategy(
        ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
    right_settings.AddObserver(new ui::ClosureAnimationObserver(
        base::Bind(&SplitViewController::OnAnimationCompleted,
                   weak_factory_.GetWeakPtr())));
    right_window_->SetTransform(right_transform);
  } else {
    left_window_->SetTransform(left_transform);
    right_window_->SetTransform(right_transform);
  }
}

void SplitViewController::OnAnimationCompleted() {
  // Animation can be cancelled when deactivated.
  if (left_window_ == NULL)
    return;
  UpdateLayout(false);

  if (state_ == INACTIVE) {
    left_window_ = NULL;
    right_window_ = NULL;
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

///////////////////////////////////////////////////////////////////////////////
// BezelController::ScrollDelegate:

void SplitViewController::ScrollBegin(BezelController::Bezel bezel,
                                      float delta) {
  if (!CanScroll())
    return;
  SetState(SCROLLING);

  aura::Window::Windows windows = window_list_provider_->GetWindowList();
  CHECK(windows.size() >= 2);
  aura::Window::Windows::const_reverse_iterator iter = windows.rbegin();
  aura::Window* current_window = *(iter);
  CHECK(wm::IsActiveWindow(current_window));

  if (delta > 0) {
    right_window_ = current_window;
    left_window_ = *(iter + 1);
  } else {
    left_window_ = current_window;
    right_window_ = *(iter + 1);
  }

  CHECK(left_window_);
  CHECK(right_window_);

  UpdateSeparatorPositionFromScrollDelta(delta);
  UpdateLayout(false);
}

void SplitViewController::ScrollEnd() {
  if (state_ != SCROLLING)
    return;

  // Max distance from the scroll end position to the middle of the screen where
  // we would go into the split view mode.
  const int kMaxDistanceFromMiddle = 120;
  int container_width = container_->GetBoundsInScreen().width();
  if (std::abs(container_width / 2 - separator_position_) <=
      kMaxDistanceFromMiddle) {
    SetState(ACTIVE);
    separator_position_ = container_width / 2;
  } else if (separator_position_ < container_width / 2) {
    separator_position_ = 0;
    SetState(INACTIVE);
    wm::ActivateWindow(right_window_);
  } else {
    separator_position_ = container_width;
    SetState(INACTIVE);
    wm::ActivateWindow(left_window_);
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
  // TODO(mfomitchev): return false in full screen.
  bool result = (!IsSplitViewModeActive() &&
                 window_list_provider_->GetWindowList().size() >= 2 &&
                 IsLandscapeOrientation(gfx::Screen::GetNativeScreen()->
                     GetDisplayNearestWindow(container_).rotation()));
  return result;
}

}  // namespace athena
