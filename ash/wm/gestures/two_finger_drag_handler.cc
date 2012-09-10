// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/gestures/two_finger_drag_handler.h"

#include "ash/wm/default_window_resizer.h"
#include "ash/wm/window_util.h"
#include "ash/wm/workspace/snap_sizer.h"
#include "ui/aura/client/window_types.h"
#include "ui/aura/window.h"
#include "ui/base/events/event.h"
#include "ui/base/events/event_constants.h"
#include "ui/base/hit_test.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"

namespace ash {
namespace internal {

TwoFingerDragHandler::TwoFingerDragHandler() {
}

TwoFingerDragHandler::~TwoFingerDragHandler() {
}

bool TwoFingerDragHandler::ProcessGestureEvent(aura::Window* target,
                                               const ui::GestureEvent& event) {
  if (event.type() == ui::ET_GESTURE_BEGIN &&
      event.details().touch_points() == 2) {
    if (wm::IsWindowNormal(target) &&
      target->type() == aura::client::WINDOW_TYPE_NORMAL) {
      target->AddObserver(this);
      window_resizer_.reset(DefaultWindowResizer::Create(target,
          event.details().bounding_box().CenterPoint(), HTCAPTION));
      return true;
    }

    return false;
  }

  if (!window_resizer_.get())
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
      aura::Window* target = window_resizer_->target_window();
      Reset();

      if (event.details().swipe_up()) {
        wm::MaximizeWindow(target);
      } else if (event.details().swipe_down()) {
        wm::MinimizeWindow(target);
      } else {
        internal::SnapSizer sizer(target,
            gfx::Point(),
            event.details().swipe_left() ? internal::SnapSizer::LEFT_EDGE :
                                           internal::SnapSizer::RIGHT_EDGE);

        ui::ScopedLayerAnimationSettings scoped_setter(
            target->layer()->GetAnimator());
        scoped_setter.SetPreemptionStrategy(
            ui::LayerAnimator::REPLACE_QUEUED_ANIMATIONS);
        target->SetBounds(sizer.target_bounds());
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
  window_resizer_->target_window()->RemoveObserver(this);
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
