// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/gestures/two_finger_drag_handler.h"

#include "ash/wm/window_resizer.h"
#include "ash/wm/window_util.h"
#include "ash/wm/workspace/snap_sizer.h"
#include "ui/aura/client/window_types.h"
#include "ui/aura/window.h"
#include "ui/aura/window_delegate.h"
#include "ui/base/events/event.h"
#include "ui/base/events/event_constants.h"
#include "ui/base/hit_test.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"

namespace {

bool IsTopEdge(int component) {
  return component == HTTOPLEFT ||
         component == HTTOP ||
         component == HTTOPRIGHT;
}

bool IsBottomEdge(int component) {
  return component == HTBOTTOMLEFT ||
         component == HTBOTTOM ||
         component == HTBOTTOMRIGHT;
}

bool IsRightEdge(int component) {
  return component == HTTOPRIGHT ||
         component == HTRIGHT ||
         component == HTBOTTOMRIGHT;
}

bool IsLeftEdge(int component) {
  return component == HTTOPLEFT ||
         component == HTLEFT ||
         component == HTBOTTOMLEFT;
}

bool IsSomeEdge(int component) {
  switch (component) {
    case HTTOPLEFT:
    case HTTOP:
    case HTTOPRIGHT:
    case HTRIGHT:
    case HTBOTTOMRIGHT:
    case HTBOTTOM:
    case HTBOTTOMLEFT:
    case HTLEFT:
      return true;
  }
  return false;
}

// Returns whether a window-move should be allowed depending on the hit-test
// results of the two fingers.
bool WindowComponentsAllowMoving(int component1, int component2) {
  return ((IsTopEdge(component1) && IsTopEdge(component2)) ||
          (IsBottomEdge(component1) && IsBottomEdge(component2)) ||
          (IsLeftEdge(component1) && IsLeftEdge(component2)) ||
          (IsRightEdge(component1) && IsRightEdge(component2)) ||
          (!IsSomeEdge(component1) && !IsSomeEdge(component2)));
}

}  // namespace

namespace ash {
namespace internal {

TwoFingerDragHandler::TwoFingerDragHandler()
    : first_finger_hittest_(HTNOWHERE) {
}

TwoFingerDragHandler::~TwoFingerDragHandler() {
}

bool TwoFingerDragHandler::ProcessGestureEvent(aura::Window* target,
                                               const ui::GestureEvent& event) {
  if (!target->delegate())
    return false;

  if (event.type() == ui::ET_GESTURE_BEGIN &&
      event.details().touch_points() == 1) {
    first_finger_hittest_ =
        target->delegate()->GetNonClientComponent(event.location());
    return false;
  }

  if (event.type() == ui::ET_GESTURE_BEGIN &&
      event.details().touch_points() == 2) {
    if (!window_resizer_.get() && wm::IsWindowNormal(target) &&
      target->type() == aura::client::WINDOW_TYPE_NORMAL) {
      if (WindowComponentsAllowMoving(first_finger_hittest_,
          target->delegate()->GetNonClientComponent(event.location()))) {
        target->AddObserver(this);
        window_resizer_ = CreateWindowResizer(target,
            event.details().bounding_box().CenterPoint(), HTCAPTION);
        return true;
      }
    }

    return false;
  }

  if (!window_resizer_.get()) {
    // Consume all two-finger gestures on a normal window.
    return event.details().touch_points() == 2 &&
           target->type() == aura::client::WINDOW_TYPE_NORMAL &&
           wm::IsWindowNormal(target);
  }

  if (target != window_resizer_->GetTarget())
    return false;

  switch (event.type()) {
    case ui::ET_GESTURE_BEGIN:
      if (event.details().touch_points() > 2)
        Reset();
      return false;

    case ui::ET_GESTURE_SCROLL_BEGIN:
    case ui::ET_GESTURE_PINCH_BEGIN:
    case ui::ET_GESTURE_SCROLL_END:
      return true;

    case ui::ET_GESTURE_MULTIFINGER_SWIPE: {
      // For a swipe, the window either maximizes, minimizes, or snaps. In this
      // case, cancel the drag, and do the appropriate action.
      Reset();

      if (event.details().swipe_up()) {
        if (wm::CanMaximizeWindow(target))
          wm::MaximizeWindow(target);
      } else if (event.details().swipe_down() &&
                 wm::CanMinimizeWindow(target)) {
        wm::MinimizeWindow(target);
      } else if (wm::CanSnapWindow(target)) {
        ui::ScopedLayerAnimationSettings scoped_setter(
            target->layer()->GetAnimator());
        scoped_setter.SetPreemptionStrategy(
            ui::LayerAnimator::REPLACE_QUEUED_ANIMATIONS);
        internal::SnapSizer::SnapWindow(target,
            event.details().swipe_left() ? internal::SnapSizer::LEFT_EDGE :
                                           internal::SnapSizer::RIGHT_EDGE);
      }
      return true;
    }

    case ui::ET_GESTURE_PINCH_UPDATE:
    case ui::ET_GESTURE_SCROLL_UPDATE:
      window_resizer_->Drag(event.details().bounding_box().CenterPoint(),
                            event.flags());
      return true;

    case ui::ET_GESTURE_PINCH_END:
      window_resizer_->CompleteDrag(event.flags());
      Reset();
      return true;

    case ui::ET_GESTURE_END:
      if (event.details().touch_points() == 2) {
        window_resizer_->RevertDrag();
        Reset();
        return true;
      }
      break;

    default:
      break;
  }

  return false;
}

void TwoFingerDragHandler::Reset() {
  window_resizer_->GetTarget()->RemoveObserver(this);
  window_resizer_.reset();
}

void TwoFingerDragHandler::OnWindowVisibilityChanged(aura::Window* window,
                                                     bool visible) {
  Reset();
}

void TwoFingerDragHandler::OnWindowDestroying(aura::Window* window) {
  Reset();
}

}  // namespace internal
}  // namespace ash
