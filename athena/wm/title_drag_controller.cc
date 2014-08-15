// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/wm/title_drag_controller.h"

#include "base/bind.h"
#include "ui/aura/window.h"
#include "ui/aura/window_delegate.h"
#include "ui/base/hit_test.h"
#include "ui/compositor/closure_animation_observer.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/wm/core/shadow.h"
#include "ui/wm/core/window_util.h"

namespace {

// The minimum amount to drag to confirm a window switch at the end of the
// non-fling gesture.
const int kMinDragDistanceForSwitch = 300;
// The minimum velocity to confirm a window switch for a fling (only applicable
// if the amount dragged was not sufficient, i.e. smaller than
// kMinDragDistanceForSwitch).
const int kMinDragVelocityForSwitch = 5000;

}

namespace athena {

TitleDragController::TitleDragController(aura::Window* container,
                                         TitleDragControllerDelegate* delegate)
    : container_(container),
      delegate_(delegate),
      weak_ptr_(this) {
  CHECK(container_);
  CHECK(delegate_);
  container_->AddPreTargetHandler(this);
}

TitleDragController::~TitleDragController() {
  container_->RemovePreTargetHandler(this);
}

void TitleDragController::EndTransition(aura::Window* window, bool complete) {
  ui::ScopedLayerAnimationSettings settings(window->layer()->GetAnimator());
  settings.SetPreemptionStrategy(
      ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
  settings.AddObserver(new ui::ClosureAnimationObserver(
      base::Bind(&TitleDragController::OnTransitionEnd,
                 weak_ptr_.GetWeakPtr(),
                 window,
                 complete)));
  gfx::Transform transform;
  transform.Translate(0, complete ? window->bounds().height() : 0);
  window->SetTransform(transform);
}

void TitleDragController::OnTransitionEnd(aura::Window* window, bool complete) {
  weak_ptr_.InvalidateWeakPtrs();
  if (!tracker_.Contains(window))
    window = NULL;
  shadow_.reset();
  if (window) {
    window->SetTransform(gfx::Transform());
    tracker_.Remove(window);
  }
  if (complete && window && wm::IsActiveWindow(window))
    delegate_->OnTitleDragCompleted(window);
  else
    delegate_->OnTitleDragCanceled(window);
}

void TitleDragController::OnGestureEvent(ui::GestureEvent* gesture) {
  // Do not process any gesture events if an animation is still in progress from
  // a previous drag.
  if (weak_ptr_.HasWeakPtrs())
    return;

  if (gesture->type() == ui::ET_GESTURE_TAP_DOWN) {
    // It is possible to start a gesture sequence on a second window while a
    // drag is already in progress (e.g. the user starts interacting with the
    // window that is being revealed by the title-drag). Ignore these gesture
    // sequences.
    if (!tracker_.windows().empty())
      return;
    aura::Window* window = static_cast<aura::Window*>(gesture->target());
    if (!window || !window->delegate())
      return;
    int component =
        window->delegate()->GetNonClientComponent(gesture->location());
    if (component != HTCAPTION)
      return;
    if (!delegate_->GetWindowBehind(window))
      return;
    tracker_.Add(window);
    drag_start_location_ = gesture->root_location();
    return;
  }

  // If this gesture is for a different window, then ignore.
  aura::Window* window = static_cast<aura::Window*>(gesture->target());
  if (!tracker_.Contains(window))
    return;

  if (gesture->type() == ui::ET_GESTURE_SCROLL_BEGIN) {
    delegate_->OnTitleDragStarted(window);
    shadow_.reset(new wm::Shadow());
    shadow_->Init(wm::Shadow::STYLE_ACTIVE);
    shadow_->SetContentBounds(gfx::Rect(window->bounds().size()));
    window->layer()->Add(shadow_->layer());
    gesture->SetHandled();
    return;
  }

  if (gesture->type() == ui::ET_GESTURE_SCROLL_UPDATE) {
    gfx::Vector2dF distance = gesture->root_location() - drag_start_location_;
    gfx::Transform transform;
    transform.Translate(0, std::max(0.f, distance.y()));
    window->SetTransform(transform);
    gesture->SetHandled();
    return;
  }

  if (gesture->type() == ui::ET_GESTURE_SCROLL_END) {
    gfx::Vector2dF distance = gesture->root_location() - drag_start_location_;
    EndTransition(window, distance.y() >= kMinDragDistanceForSwitch);
    gesture->SetHandled();
    return;
  }

  if (gesture->type() == ui::ET_SCROLL_FLING_START) {
    gfx::Vector2dF distance = gesture->root_location() - drag_start_location_;
    bool swipe_downwards = gesture->details().velocity_y() > 0;
    EndTransition(
        window,
        swipe_downwards &&
            (distance.y() >= kMinDragDistanceForSwitch ||
             gesture->details().velocity_y() >= kMinDragVelocityForSwitch));
    gesture->SetHandled();
  }
}

}  // namespace athena
