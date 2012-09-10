// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/gestures/system_pinch_handler.h"

#include "ash/launcher/launcher.h"
#include "ash/screen_ash.h"
#include "ash/shell.h"
#include "ash/wm/property_util.h"
#include "ash/wm/window_util.h"
#include "ash/wm/workspace/snap_sizer.h"
#include "ui/aura/window.h"
#include "ui/base/events/event_constants.h"
#include "ui/base/gestures/gesture_types.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
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

  switch (event.type()) {
    case ui::ET_GESTURE_END: {
      if (event.details().touch_points() > kSystemGesturePoints)
        break;

      if (phantom_state_ == PHANTOM_WINDOW_MAXIMIZED) {
        if (!wm::IsWindowMaximized(target_) &&
            !wm::IsWindowFullscreen(target_))
          wm::MaximizeWindow(target_);
      } else if (phantom_state_ == PHANTOM_WINDOW_MINIMIZED) {
        if (wm::IsWindowMaximized(target_) ||
            wm::IsWindowFullscreen(target_)) {
          wm::RestoreWindow(target_);
        } else {
          wm::MinimizeWindow(target_);

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
        phantom_.Show(bounds, NULL);
      break;
    }

    case ui::ET_GESTURE_MULTIFINGER_SWIPE: {
      phantom_.Hide();
      pinch_factor_ = 1.0;
      phantom_state_ = PHANTOM_WINDOW_NORMAL;

      if (event.details().swipe_left() || event.details().swipe_right()) {
        // Snap for left/right swipes. In case the window is
        // maximized/fullscreen, then restore the window first so that tiling
        // works correctly.
        if (wm::IsWindowMaximized(target_) ||
            wm::IsWindowFullscreen(target_))
          wm::RestoreWindow(target_);

        ui::ScopedLayerAnimationSettings settings(
            target_->layer()->GetAnimator());
        SnapSizer sizer(target_,
            gfx::Point(),
            event.details().swipe_left() ? internal::SnapSizer::LEFT_EDGE :
                                           internal::SnapSizer::RIGHT_EDGE);
        target_->SetBounds(sizer.GetSnapBounds(target_->bounds()));
      } else if (event.details().swipe_up()) {
        if (!wm::IsWindowMaximized(target_) &&
            !wm::IsWindowFullscreen(target_))
          wm::MaximizeWindow(target_);
      } else if (event.details().swipe_down()) {
        wm::MinimizeWindow(target_);
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
    if (wm::IsWindowMaximized(window) || wm::IsWindowFullscreen(window)) {
      const gfx::Rect* restore = GetRestoreBoundsInScreen(window);
      if (restore) {
        phantom_state_ = PHANTOM_WINDOW_MINIMIZED;
        return *restore;
      }
      return window->bounds();
    }

    Launcher* launcher = Shell::GetInstance()->launcher();
    gfx::Rect rect = launcher->GetScreenBoundsOfItemIconForWindow(target_);
    if (rect.IsEmpty())
      rect = launcher->widget()->GetWindowBoundsInScreen();
    else
      rect.Inset(-8, -8);
    phantom_state_ = PHANTOM_WINDOW_MINIMIZED;
    return rect;
  }

  phantom_state_ = PHANTOM_WINDOW_NORMAL;
  return window->bounds();
}

}  // namespace internal
}  // namespace ash
