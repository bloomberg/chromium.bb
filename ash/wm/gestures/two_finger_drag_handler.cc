// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/gestures/two_finger_drag_handler.h"

#include "ash/wm/window_resizer.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "ash/wm/workspace/snap_sizer.h"
#include "ui/aura/window.h"
#include "ui/aura/window_delegate.h"
#include "ui/base/hit_test.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/wm/public/window_types.h"

namespace {

enum Edge {
  TOP    = 1 << 0,
  LEFT   = 1 << 1,
  BOTTOM = 1 << 2,
  RIGHT  = 1 << 3
};

// Returns the bitfield of Edge which corresponds to |hittest_component|.
int GetEdges(int hittest_component) {
  switch (hittest_component) {
    case HTTOPLEFT:
      return TOP | LEFT;
    case HTTOP:
      return TOP;
    case HTTOPRIGHT:
      return TOP | RIGHT;
    case HTRIGHT:
      return RIGHT;
    case HTGROWBOX:
    case HTBOTTOMRIGHT:
      return BOTTOM | RIGHT;
    case HTBOTTOM:
      return BOTTOM;
    case HTBOTTOMLEFT:
      return BOTTOM | LEFT;
    case HTLEFT:
      return LEFT;
    default:
      return 0;
  }
}

// Returns whether |window| can be moved via a two finger drag given
// the hittest results of the two fingers.
bool CanStartTwoFingerDrag(aura::Window* window,
                           int hittest_component1,
                           int hittest_component2) {
  if (ash::wm::GetWindowState(window)->IsNormalShowState() &&
      window->type() == ui::wm::WINDOW_TYPE_NORMAL) {
    int edges1 = GetEdges(hittest_component1);
    int edges2 = GetEdges(hittest_component2);
    return (edges1 == edges2 || (edges1 & edges2) != 0);
  }
  return false;
}

}  // namespace

namespace ash {
namespace internal {

TwoFingerDragHandler::TwoFingerDragHandler()
    : first_finger_hittest_(HTNOWHERE),
      in_gesture_drag_(false) {
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

  wm::WindowState* window_state = wm::GetWindowState(target);

  if (event.type() == ui::ET_GESTURE_BEGIN &&
      event.details().touch_points() == 2) {
    int second_finger_hittest =
        target->delegate()->GetNonClientComponent(event.location());
    if (!in_gesture_drag_ &&
        CanStartTwoFingerDrag(
            target, first_finger_hittest_, second_finger_hittest)) {
      in_gesture_drag_ = true;
      target->AddObserver(this);
      // Only create a new WindowResizer if one doesn't already exist
      // for the target window.
      window_resizer_ = CreateWindowResizer(
          target,
          event.details().bounding_box().CenterPoint(),
          HTCAPTION,
          aura::client::WINDOW_MOVE_SOURCE_TOUCH);
      return true;
    }

    return false;
  }

  if (!in_gesture_drag_) {
    // Consume all two-finger gestures on a normal window.
    return event.details().touch_points() == 2 &&
        target->type() == ui::wm::WINDOW_TYPE_NORMAL &&
        window_state->IsNormalShowState();
  }

  // Since |in_gesture_drag_| is true a resizer was either created above or
  // it was created elsewhere and can be found in |window_state|.
  WindowResizer* any_window_resizer = window_resizer_ ?
      window_resizer_.get() : window_state->window_resizer();
  DCHECK(any_window_resizer);

  if (target != any_window_resizer->GetTarget())
    return false;

  switch (event.type()) {
    case ui::ET_GESTURE_BEGIN:
      if (event.details().touch_points() > 2) {
        if (window_resizer_)
          window_resizer_->CompleteDrag();
        Reset(target);
      }
      return false;

    case ui::ET_GESTURE_SCROLL_BEGIN:
    case ui::ET_GESTURE_PINCH_BEGIN:
    case ui::ET_GESTURE_SCROLL_END:
      return true;

    case ui::ET_GESTURE_MULTIFINGER_SWIPE: {
      // For a swipe, the window either maximizes, minimizes, or snaps. In this
      // case, complete the drag, and do the appropriate action.
      if (window_resizer_)
        window_resizer_->CompleteDrag();
      Reset(target);
      if (event.details().swipe_up()) {
        if (window_state->CanMaximize())
          window_state->Maximize();
      } else if (event.details().swipe_down() && window_state->CanMinimize()) {
        window_state->Minimize();
      } else if (window_state->CanSnap()) {
        internal::SnapSizer::SnapWindow(window_state,
            event.details().swipe_left() ? internal::SnapSizer::LEFT_EDGE :
                                           internal::SnapSizer::RIGHT_EDGE);
      }
      return true;
    }

    case ui::ET_GESTURE_PINCH_UPDATE:
    case ui::ET_GESTURE_SCROLL_UPDATE:
      any_window_resizer->Drag(event.details().bounding_box().CenterPoint(),
                               event.flags());
      return true;

    case ui::ET_GESTURE_PINCH_END:
      if (window_resizer_)
        window_resizer_->CompleteDrag();
      Reset(target);
      return true;

    case ui::ET_GESTURE_END:
      if (event.details().touch_points() == 2) {
        if (window_resizer_)
          window_resizer_->RevertDrag();
        Reset(target);
        return true;
      }
      break;

    default:
      break;
  }

  return false;
}

void TwoFingerDragHandler::Reset(aura::Window* window) {
  window->RemoveObserver(this);
  window_resizer_.reset();
  in_gesture_drag_ = false;
}

void TwoFingerDragHandler::OnWindowVisibilityChanged(aura::Window* window,
                                                     bool visible) {
  Reset(window);
}

void TwoFingerDragHandler::OnWindowDestroying(aura::Window* window) {
  Reset(window);
}

}  // namespace internal
}  // namespace ash
