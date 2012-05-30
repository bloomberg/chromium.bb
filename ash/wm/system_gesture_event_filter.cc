// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/system_gesture_event_filter.h"

#include "ash/accelerators/accelerator_controller.h"
#include "ash/shell.h"
#include "ash/system/brightness/brightness_control_delegate.h"
#include "ash/volume_control_delegate.h"
#include "ash/wm/property_util.h"
#include "ash/wm/window_resizer.h"
#include "ash/wm/window_util.h"
#include "ash/wm/workspace/phantom_window_controller.h"
#include "ash/wm/workspace/snap_sizer.h"
#include "ui/aura/event.h"
#include "ui/aura/root_window.h"
#include "ui/gfx/screen.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

namespace {

const int kSystemPinchPoints = 4;

enum SystemGestureStatus {
  SYSTEM_GESTURE_PROCESSED,  // The system gesture has been processed.
  SYSTEM_GESTURE_IGNORED,    // The system gesture was ignored.
  SYSTEM_GESTURE_END,        // Marks the end of the sytem gesture.
};

aura::Window* GetTargetForSystemGestureEvent(aura::Window* target) {
  aura::Window* system_target = target;
  if (!system_target || system_target == ash::Shell::GetPrimaryRootWindow())
    system_target = ash::wm::GetActiveWindow();
  if (system_target)
    system_target = system_target->GetToplevelWindow();
  return system_target;
}

}

namespace ash {
namespace internal {

class SystemPinchHandler {
 public:
  explicit SystemPinchHandler(aura::Window* target)
      : target_(target),
        phantom_(target),
        pinch_factor_(1.) {
    widget_ = views::Widget::GetWidgetForNativeWindow(target_);
  }

  ~SystemPinchHandler() {
  }

  SystemGestureStatus ProcessGestureEvent(const aura::GestureEvent& event) {
    // The target has changed, somehow. Let's bale.
    if (!widget_ || !widget_->widget_delegate()->CanResize())
      return SYSTEM_GESTURE_END;

    switch (event.type()) {
      case ui::ET_GESTURE_TAP_UP: {
        if (event.delta_x() > kSystemPinchPoints)
          break;

        gfx::Rect bounds = phantom_.IsShowing() ?  phantom_.bounds() :
                                                   target_->bounds();
        int grid = Shell::GetInstance()->GetGridSize();
        bounds.set_x(WindowResizer::AlignToGridRoundUp(bounds.x(), grid));
        bounds.set_y(WindowResizer::AlignToGridRoundUp(bounds.y(), grid));
        if (wm::IsWindowFullscreen(target_) || wm::IsWindowMaximized(target_)) {
          SetRestoreBounds(target_, bounds);
          wm::RestoreWindow(target_);
        } else {
          target_->SetBounds(bounds);
        }
        return SYSTEM_GESTURE_END;
      }

      case ui::ET_GESTURE_SCROLL_UPDATE: {
        if (wm::IsWindowFullscreen(target_) || wm::IsWindowMaximized(target_)) {
          if (!phantom_.IsShowing())
            break;
        } else {
          gfx::Rect bounds = target_->bounds();
          bounds.set_x(static_cast<int>(bounds.x() + event.delta_x()));
          bounds.set_y(static_cast<int>(bounds.y() + event.delta_y()));
          target_->SetBounds(bounds);
        }

        if (phantom_.IsShowing()) {
          gfx::Rect bounds = phantom_.bounds();
          bounds.set_x(static_cast<int>(bounds.x() + event.delta_x()));
          bounds.set_y(static_cast<int>(bounds.y() + event.delta_y()));
          phantom_.SetBounds(bounds);
        }
        break;
      }

      case ui::ET_GESTURE_PINCH_UPDATE: {
        // The PINCH_UPDATE events contain incremental scaling updates.
        pinch_factor_ *= event.delta_x();
        gfx::Rect bounds = target_->bounds();

        if (wm::IsWindowFullscreen(target_) || wm::IsWindowMaximized(target_)) {
          // For a fullscreen/maximized window, if you start pinching in, and
          // you pinch enough, then it shows the phantom window with the
          // restore-bounds. The subsequent pinch updates then work on the
          // restore bounds instead of the fullscreen/maximized bounds.
          const gfx::Rect* restore = NULL;
          if (phantom_.IsShowing()) {
            restore = GetRestoreBounds(target_);
          } else if (pinch_factor_ < 0.8) {
            restore = GetRestoreBounds(target_);
            // Reset the pinch factor.
            pinch_factor_ = 1.0;
          }

          if (restore)
            bounds = *restore;
          else
            break;
        }

        gfx::Rect new_bounds = bounds.Scale(pinch_factor_);
        new_bounds.set_x(bounds.x() + (event.x() - event.x() * pinch_factor_));
        new_bounds.set_y(bounds.y() + (event.y() - event.y() * pinch_factor_));
        phantom_.Show(new_bounds);
        break;
      }

      default:
        break;
    }

    return SYSTEM_GESTURE_PROCESSED;
  }

 private:
  aura::Window* target_;
  views::Widget* widget_;

  PhantomWindowController phantom_;
  double pinch_factor_;

  DISALLOW_COPY_AND_ASSIGN(SystemPinchHandler);
};

SystemGestureEventFilter::SystemGestureEventFilter()
    : aura::EventFilter(),
      overlap_percent_(5),
      start_location_(BEZEL_START_UNSET),
      orientation_(SCROLL_ORIENTATION_UNSET),
      is_scrubbing_(false){
}

SystemGestureEventFilter::~SystemGestureEventFilter() {
}

bool SystemGestureEventFilter::PreHandleKeyEvent(aura::Window* target,
                                                 aura::KeyEvent* event) {
  return false;
}

bool SystemGestureEventFilter::PreHandleMouseEvent(aura::Window* target,
                                                   aura::MouseEvent* event) {
  return false;
}

ui::TouchStatus SystemGestureEventFilter::PreHandleTouchEvent(
    aura::Window* target,
    aura::TouchEvent* event) {
  return ui::TOUCH_STATUS_UNKNOWN;
}

ui::GestureStatus SystemGestureEventFilter::PreHandleGestureEvent(
    aura::Window* target, aura::GestureEvent* event) {
  if (!target || target == Shell::GetPrimaryRootWindow()) {
    switch (event->type()) {
      case ui::ET_GESTURE_SCROLL_BEGIN: {
          gfx::Rect screen =
              gfx::Screen::GetPrimaryMonitor().bounds();
          int overlap_area = screen.width() * overlap_percent_ / 100;
          orientation_ = SCROLL_ORIENTATION_UNSET;

          if (event->x() <= screen.x() + overlap_area) {
            start_location_ = BEZEL_START_LEFT;
          } else if (event->x() >= screen.right() - overlap_area) {
            start_location_ = BEZEL_START_RIGHT;
          } else if (event->y() >= screen.bottom()) {
            start_location_ = BEZEL_START_BOTTOM;
          }
        }
        break;
      case ui::ET_GESTURE_SCROLL_UPDATE:
        if (start_location_ == BEZEL_START_UNSET)
          break;
        if (orientation_ == SCROLL_ORIENTATION_UNSET) {
          if (!event->delta_x() && !event->delta_y())
            break;
          // For left and right the scroll angle needs to be much steeper to
          // be accepted for a 'device configuration' gesture.
          if (start_location_ == BEZEL_START_LEFT ||
              start_location_ == BEZEL_START_RIGHT) {
            orientation_ = abs(event->delta_y()) > abs(event->delta_x()) * 3 ?
                SCROLL_ORIENTATION_VERTICAL : SCROLL_ORIENTATION_HORIZONTAL;
          } else {
            orientation_ = abs(event->delta_y()) > abs(event->delta_x()) ?
                SCROLL_ORIENTATION_VERTICAL : SCROLL_ORIENTATION_HORIZONTAL;
          }
        }
        if (orientation_ == SCROLL_ORIENTATION_HORIZONTAL) {
          if (HandleApplicationControl(event))
            start_location_ = BEZEL_START_UNSET;
        } else {
          if (start_location_ == BEZEL_START_BOTTOM) {
            if (HandleLauncherControl(event))
              start_location_ = BEZEL_START_UNSET;
          } else {
            if (HandleDeviceControl(event))
              start_location_ = BEZEL_START_UNSET;
          }
        }
        break;
      case ui::ET_GESTURE_SCROLL_END:
        start_location_ = BEZEL_START_UNSET;
        break;
      default:
        break;
    }
    return ui::GESTURE_STATUS_CONSUMED;
  }

  aura::Window* system_target = GetTargetForSystemGestureEvent(target);
  if (!system_target)
    return ui::GESTURE_STATUS_UNKNOWN;

  WindowPinchHandlerMap::iterator find = pinch_handlers_.find(system_target);
  if (find != pinch_handlers_.end()) {
    SystemGestureStatus status =
        (*find).second->ProcessGestureEvent(*event);
    if (status == SYSTEM_GESTURE_END)
      ClearGestureHandlerForWindow(system_target);
    return ui::GESTURE_STATUS_CONSUMED;
  } else {
    if (event->type() == ui::ET_GESTURE_TAP_DOWN &&
        event->delta_x() >= kSystemPinchPoints) {
      pinch_handlers_[system_target] = new SystemPinchHandler(system_target);
      system_target->AddObserver(this);
      return ui::GESTURE_STATUS_CONSUMED;
    }
  }

  return ui::GESTURE_STATUS_UNKNOWN;
}

void SystemGestureEventFilter::OnWindowVisibilityChanged(aura::Window* window,
                                                         bool visible) {
  if (!visible)
    ClearGestureHandlerForWindow(window);
}

void SystemGestureEventFilter::OnWindowDestroying(aura::Window* window) {
  ClearGestureHandlerForWindow(window);
}

void SystemGestureEventFilter::ClearGestureHandlerForWindow(
    aura::Window* window) {
  WindowPinchHandlerMap::iterator find = pinch_handlers_.find(window);
  DCHECK(find != pinch_handlers_.end());
  delete (*find).second;
  pinch_handlers_.erase(find);
  window->RemoveObserver(this);
}

bool SystemGestureEventFilter::HandleDeviceControl(aura::GestureEvent* event) {
  gfx::Rect screen = gfx::Screen::GetPrimaryMonitor().bounds();
  double percent = 100.0 * (event->y() - screen.y()) / screen.height();
  if (percent > 100.0)
    percent = 100.0;
  if (percent < 0.0)
    percent = 0.0;
  ash::AcceleratorController* accelerator =
      ash::Shell::GetInstance()->accelerator_controller();
  if (start_location_ == BEZEL_START_LEFT) {
    ash::BrightnessControlDelegate* delegate =
        accelerator->brightness_control_delegate();
    if (delegate)
      delegate->SetBrightnessPercent(100.0 - percent, true);
  } else if (start_location_ == BEZEL_START_RIGHT) {
    ash::VolumeControlDelegate* delegate =
        accelerator->volume_control_delegate();
    if (delegate)
      delegate->SetVolumePercent(100.0 - percent);
  } else {
    return true;
  }
  // More notifications can be send.
  return false;
}

bool SystemGestureEventFilter::HandleLauncherControl(
    aura::GestureEvent* event) {
  ash::AcceleratorController* accelerator =
      ash::Shell::GetInstance()->accelerator_controller();
  if (start_location_ == BEZEL_START_BOTTOM && event->delta_y() < 0)
    // We leave the work to switch to the next window to our accelerators.
    accelerator->AcceleratorPressed(
        ui::Accelerator(ui::VKEY_LWIN, ui::EF_CONTROL_DOWN));
  else
    return false;
  // No further notifications for this gesture.
  return true;
}

bool SystemGestureEventFilter::HandleApplicationControl(
    aura::GestureEvent* event) {
  ash::AcceleratorController* accelerator =
      ash::Shell::GetInstance()->accelerator_controller();
  if (start_location_ == BEZEL_START_LEFT && event->delta_x() > 0)
    // We leave the work to switch to the next window to our accelerators.
    accelerator->AcceleratorPressed(
        ui::Accelerator(ui::VKEY_F5, ui::EF_SHIFT_DOWN));
  else if (start_location_ == BEZEL_START_RIGHT && event->delta_x() < 0)
    // We leave the work to switch to the previous window to our accelerators.
    accelerator->AcceleratorPressed(
        ui::Accelerator(ui::VKEY_F5, ui::EF_NONE));
  else
    return false;
  // No further notifications for this gesture.
  return true;
}

}  // namespace internal
}  // namespace ash
