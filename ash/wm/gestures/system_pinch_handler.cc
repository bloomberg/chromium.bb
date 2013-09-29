// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/gestures/system_pinch_handler.h"

#include "ash/launcher/launcher.h"
#include "ash/screen_ash.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "ash/wm/window_animations.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "ash/wm/workspace/snap_sizer.h"
#include "ui/aura/window.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/events/event_constants.h"
#include "ui/events/gestures/gesture_types.h"
#include "ui/gfx/rect.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

const double kPinchThresholdForMaximize = 1.5;
const double kPinchThresholdForMinimize = 0.7;

namespace ash {
namespace internal {

const int SystemPinchHandler::kSystemGesturePoints = 4;

SystemPinchHandler::SystemPinchHandler(aura::Window* target)
    : target_(target),
      phantom_(target),
      phantom_state_(PHANTOM_WINDOW_NORMAL),
      pinch_factor_(1.) {
  widget_ = views::Widget::GetWidgetForNativeWindow(target_);
}

SystemPinchHandler::~SystemPinchHandler() {
}

SystemGestureStatus SystemPinchHandler::ProcessGestureEvent(
    const ui::GestureEvent& event) {
  // The target has changed, somehow. Let's bale.
  if (!widget_ || !widget_->widget_delegate()->CanResize())
    return SYSTEM_GESTURE_END;
  wm::WindowState* window_state = wm::GetWindowState(target_);
  switch (event.type()) {
    case ui::ET_GESTURE_END: {
      if (event.details().touch_points() > kSystemGesturePoints)
        break;

      if (phantom_state_ == PHANTOM_WINDOW_MAXIMIZED) {
        if (!window_state->IsMaximizedOrFullscreen())
          window_state->Maximize();
      } else if (phantom_state_ == PHANTOM_WINDOW_MINIMIZED) {
        if (window_state->IsMaximizedOrFullscreen()) {
          window_state->Restore();
        } else {
          window_state->Minimize();

          // NOTE: Minimizing the window will cause this handler to be
          // destroyed. So do not access anything from |this| from here.
          return SYSTEM_GESTURE_END;
        }
      }
      return SYSTEM_GESTURE_END;
    }

    case ui::ET_GESTURE_PINCH_UPDATE: {
      // The PINCH_UPDATE events contain incremental scaling updates.
      pinch_factor_ *= event.details().scale();
      gfx::Rect bounds =
          GetPhantomWindowScreenBounds(target_, event.location());
      if (phantom_state_ != PHANTOM_WINDOW_NORMAL || phantom_.IsShowing())
        phantom_.Show(bounds);
      break;
    }

    case ui::ET_GESTURE_MULTIFINGER_SWIPE: {
      phantom_.Hide();
      pinch_factor_ = 1.0;
      phantom_state_ = PHANTOM_WINDOW_NORMAL;

      if (event.details().swipe_left() || event.details().swipe_right()) {
        // Snap for left/right swipes.
        ui::ScopedLayerAnimationSettings settings(
            target_->layer()->GetAnimator());
        internal::SnapSizer::SnapWindow(window_state,
            event.details().swipe_left() ? internal::SnapSizer::LEFT_EDGE :
                                           internal::SnapSizer::RIGHT_EDGE);
      } else if (event.details().swipe_up()) {
        if (!window_state->IsMaximizedOrFullscreen())
          window_state->Maximize();
      } else if (event.details().swipe_down()) {
        window_state->Minimize();
      } else {
        NOTREACHED() << "Swipe happened without a direction.";
      }
      break;
    }

    default:
      break;
  }

  return SYSTEM_GESTURE_PROCESSED;
}

gfx::Rect SystemPinchHandler::GetPhantomWindowScreenBounds(
    aura::Window* window,
    const gfx::Point& point) {
  if (pinch_factor_ > kPinchThresholdForMaximize) {
    phantom_state_ = PHANTOM_WINDOW_MAXIMIZED;
    return ScreenAsh::ConvertRectToScreen(
        target_->parent(),
        ScreenAsh::GetMaximizedWindowBoundsInParent(target_));
  }

  if (pinch_factor_ < kPinchThresholdForMinimize) {
    wm::WindowState* window_state = wm::GetWindowState(window);
    if (window_state->IsMaximizedOrFullscreen()) {
      if (window_state->HasRestoreBounds()) {
        phantom_state_ = PHANTOM_WINDOW_MINIMIZED;
        return  window_state->GetRestoreBoundsInScreen();
      }
      return window->bounds();
    }

    gfx::Rect rect = GetMinimizeAnimationTargetBoundsInScreen(target_);
    if (!rect.IsEmpty())
      rect.Inset(-8, -8);
    phantom_state_ = PHANTOM_WINDOW_MINIMIZED;
    return rect;
  }

  phantom_state_ = PHANTOM_WINDOW_NORMAL;
  return window->bounds();
}

}  // namespace internal
}  // namespace ash
