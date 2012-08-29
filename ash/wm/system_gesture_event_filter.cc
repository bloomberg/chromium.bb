// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/system_gesture_event_filter.h"

#include "ash/accelerators/accelerator_controller.h"
#include "ash/accelerators/accelerator_table.h"
#include "ash/ash_switches.h"
#include "ash/launcher/launcher.h"
#include "ash/root_window_controller.h"
#include "ash/screen_ash.h"
#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "ash/shell_window_ids.h"
#include "ash/system/brightness/brightness_control_delegate.h"
#include "ash/system/tray/system_tray_delegate.h"
#include "ash/volume_control_delegate.h"
#include "ash/wm/gestures/long_press_affordance_handler.h"
#include "ash/wm/gestures/system_pinch_handler.h"
#include "ash/wm/property_util.h"
#include "ash/wm/window_resizer.h"
#include "ash/wm/window_util.h"
#include "base/command_line.h"
#include "ui/aura/root_window.h"
#include "ui/base/event.h"
#include "ui/base/ui_base_switches.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/point.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/screen.h"
#include "ui/gfx/size.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

#if defined(OS_CHROMEOS)
#include "ui/base/touch/touch_factory.h"
#endif

namespace {
// Device bezel operation constants (volume/brightness slider).

// This is the minimal brightness value allowed for the display.
const double kMinBrightnessPercent = 5.0;
// For device operation, the finger is not allowed to enter the screen more
// then this fraction of the size of the screen.
const double kAllowableScreenOverlapForDeviceCommand = 0.0005;

// TODO(skuhne): The noise reduction can be removed when / if we are adding a
// more general reduction.
// To avoid unwanted noise activation, the first 'n' events are being ignored
// for bezel device gestures.
const int kIgnoreFirstBezelDeviceEvents = 10;
// Within these 'n' huge coordinate changes are not allowed. The threshold is
// given in fraction of screen resolution changes.
const double kBezelNoiseDeltaFilter = 0.1;
// To avoid the most frequent noise (extreme locations) the bezel percent
// sliders will not cover the entire screen. We scale therefore the percent
// value by this many percent for minima and maxima extension.
// (Range extends to -kMinMaxInsetPercent .. 100 + kMinMaxInsetPercent).
const double kMinMaxInsetPercent = 5.0;
// To make it possible to reach minimas and maximas easily a range extension
// of -kMinMaxCutOffPercent .. 100 + kMinMaxCutOffPercent will be clamped to
// 0..100%. Everything beyond that will be ignored.
const double kMinMaxCutOffPercent = 2.0;

aura::Window* GetTargetForSystemGestureEvent(aura::Window* target) {
  aura::Window* system_target = target;
  if (!system_target || system_target == target->GetRootWindow())
    system_target = ash::wm::GetActiveWindow();
  if (system_target)
    system_target = system_target->GetToplevelWindow();
  return system_target;
}

}  // namespace

namespace ash {
namespace internal {

SystemGestureEventFilter::SystemGestureEventFilter()
    : aura::EventFilter(),
      overlap_percent_(5),
      start_location_(BEZEL_START_UNSET),
      orientation_(SCROLL_ORIENTATION_UNSET),
      is_scrubbing_(false),
      system_gestures_enabled_(CommandLine::ForCurrentProcess()->
          HasSwitch(ash::switches::kAshEnableAdvancedGestures)),
      long_press_affordance_(new LongPressAffordanceHandler) {
}

SystemGestureEventFilter::~SystemGestureEventFilter() {
}

bool SystemGestureEventFilter::PreHandleKeyEvent(aura::Window* target,
                                                 ui::KeyEvent* event) {
  return false;
}

bool SystemGestureEventFilter::PreHandleMouseEvent(aura::Window* target,
                                                   ui::MouseEvent* event) {
#if defined(OS_CHROMEOS)
  if (event->type() == ui::ET_MOUSE_PRESSED && event->native_event() &&
      ui::TouchFactory::GetInstance()->IsTouchDevicePresent()) {
    Shell::GetInstance()->delegate()->RecordUserMetricsAction(
      UMA_MOUSE_DOWN);
  }
#endif
  return false;
}

ui::TouchStatus SystemGestureEventFilter::PreHandleTouchEvent(
    aura::Window* target,
    ui::TouchEvent* event) {
  touch_uma_.RecordTouchEvent(target, *event);
  long_press_affordance_->ProcessEvent(target, event, event->touch_id());
  return ui::TOUCH_STATUS_UNKNOWN;
}

ui::GestureStatus SystemGestureEventFilter::PreHandleGestureEvent(
    aura::Window* target, ui::GestureEvent* event) {
  touch_uma_.RecordGestureEvent(target, *event);
  long_press_affordance_->ProcessEvent(target, event,
      event->GetLowestTouchId());
  if (!target || target == target->GetRootWindow()) {
    switch (event->type()) {
      case ui::ET_GESTURE_SCROLL_BEGIN:
        HandleBezelGestureStart(target, event);
        break;
      case ui::ET_GESTURE_SCROLL_UPDATE:
        // Check if a valid start position has been set.
        if (start_location_ == BEZEL_START_UNSET)
          break;

        if (DetermineGestureOrientation(event))
          HandleBezelGestureUpdate(target, event);
        break;
      case ui::ET_GESTURE_SCROLL_END:
        HandleBezelGestureEnd();
        break;
      default:
        break;
    }
    return ui::GESTURE_STATUS_CONSUMED;
  }

  if (!system_gestures_enabled_)
    return ui::GESTURE_STATUS_UNKNOWN;

  aura::Window* system_target = GetTargetForSystemGestureEvent(target);
  if (!system_target)
    return ui::GESTURE_STATUS_UNKNOWN;

  RootWindowController* root_controller =
      GetRootWindowController(system_target->GetRootWindow());
  CHECK(root_controller);
  aura::Window* desktop_container = root_controller->GetContainer(
      ash::internal::kShellWindowId_DesktopBackgroundContainer);
  if (desktop_container->Contains(system_target)) {
    // The gesture was on the desktop window.
    if (event->type() == ui::ET_GESTURE_MULTIFINGER_SWIPE &&
        event->details().swipe_up() &&
        event->details().touch_points() ==
        SystemPinchHandler::kSystemGesturePoints) {
      ash::AcceleratorController* accelerator =
          ash::Shell::GetInstance()->accelerator_controller();
      if (accelerator->PerformAction(CYCLE_FORWARD_MRU, ui::Accelerator()))
        return ui::GESTURE_STATUS_CONSUMED;
    }
    return ui::GESTURE_STATUS_UNKNOWN;
  }

  WindowPinchHandlerMap::iterator find = pinch_handlers_.find(system_target);
  if (find != pinch_handlers_.end()) {
    SystemGestureStatus status =
        (*find).second->ProcessGestureEvent(*event);
    if (status == SYSTEM_GESTURE_END)
      ClearGestureHandlerForWindow(system_target);
    return ui::GESTURE_STATUS_CONSUMED;
  } else {
    if (event->type() == ui::ET_GESTURE_BEGIN &&
        event->details().touch_points() >=
        SystemPinchHandler::kSystemGesturePoints) {
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
  if (find == pinch_handlers_.end()) {
    // The handler may have already been removed.
    return;
  }
  delete (*find).second;
  pinch_handlers_.erase(find);
  window->RemoveObserver(this);
}

bool SystemGestureEventFilter::HandleDeviceControl(
    const gfx::Rect& screen,
    ui::GestureEvent* event) {
  // Get the slider position as value from the absolute position.
  // Note that the highest value is at the top.
  double percent = 100.0 - 100.0 * (event->y() - screen.y()) / screen.height();
  if (!DeNoiseBezelSliderPosition(percent)) {
    // Note: Even though this particular event might be noise, the gesture
    // itself is still valid and should not get cancelled.
    return false;
  }
  ash::AcceleratorController* accelerator =
      ash::Shell::GetInstance()->accelerator_controller();
  if (start_location_ == BEZEL_START_LEFT) {
    ash::BrightnessControlDelegate* delegate =
        accelerator->brightness_control_delegate();
    if (delegate)
      delegate->SetBrightnessPercent(
          LimitBezelBrightnessFromSlider(percent), true);
  } else if (start_location_ == BEZEL_START_RIGHT) {
    Shell::GetInstance()->tray_delegate()->GetVolumeControlDelegate()->
        SetVolumePercent(percent);
  } else {
    // No further events are necessary.
    return true;
  }

  // More notifications can be send.
  return false;
}

bool SystemGestureEventFilter::HandleLauncherControl(
    ui::GestureEvent* event) {
  if (start_location_ == BEZEL_START_BOTTOM &&
      event->details().scroll_y() < 0) {
    ash::AcceleratorController* accelerator =
        ash::Shell::GetInstance()->accelerator_controller();
    accelerator->PerformAction(FOCUS_LAUNCHER, ui::Accelerator());
  } else {
    return false;
  }
  // No further notifications for this gesture.
  return true;
}

bool SystemGestureEventFilter::HandleApplicationControl(
    ui::GestureEvent* event) {
  ash::AcceleratorController* accelerator =
      ash::Shell::GetInstance()->accelerator_controller();
  if (start_location_ == BEZEL_START_LEFT && event->details().scroll_x() > 0)
    accelerator->PerformAction(CYCLE_BACKWARD_LINEAR, ui::Accelerator());
  else if (start_location_ == BEZEL_START_RIGHT &&
             event->details().scroll_x() < 0)
    accelerator->PerformAction(CYCLE_FORWARD_LINEAR, ui::Accelerator());
  else
    return false;

  // No further notifications for this gesture.
  return true;
}

void SystemGestureEventFilter::HandleBezelGestureStart(
    aura::Window* target, ui::GestureEvent* event) {
  gfx::Rect screen = gfx::Screen::GetDisplayNearestWindow(target).bounds();
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

bool SystemGestureEventFilter::DetermineGestureOrientation(
    ui::GestureEvent* event) {
  if (orientation_ == SCROLL_ORIENTATION_UNSET) {
    if (!event->details().scroll_x() && !event->details().scroll_y())
      return false;

    // For left and right the scroll angle needs to be much steeper to
    // be accepted for a 'device configuration' gesture.
    if (start_location_ == BEZEL_START_LEFT ||
        start_location_ == BEZEL_START_RIGHT) {
      orientation_ = abs(event->details().scroll_y()) >
                     abs(event->details().scroll_x()) * 3 ?
          SCROLL_ORIENTATION_VERTICAL : SCROLL_ORIENTATION_HORIZONTAL;
    } else {
      orientation_ = abs(event->details().scroll_y()) >
                     abs(event->details().scroll_x()) ?
          SCROLL_ORIENTATION_VERTICAL : SCROLL_ORIENTATION_HORIZONTAL;
    }

    // Reset the delay counter for noise event filtering.
    initiation_delay_events_ = 0;
  }
  return true;
}

void SystemGestureEventFilter::HandleBezelGestureUpdate(
    aura::Window* target, ui::GestureEvent* event) {
  if (orientation_ == SCROLL_ORIENTATION_HORIZONTAL) {
    if (HandleApplicationControl(event))
      start_location_ = BEZEL_START_UNSET;
  } else {
    if (start_location_ == BEZEL_START_BOTTOM) {
      if (HandleLauncherControl(event))
        start_location_ = BEZEL_START_UNSET;
    } else {
      // Check if device gestures should be performed or not.
      if (CommandLine::ForCurrentProcess()->HasSwitch(
              ::switches::kDisableBezelTouch)) {
        start_location_ = BEZEL_START_UNSET;
        return;
      }
      gfx::Rect screen = gfx::Screen::GetDisplayNearestWindow(target).bounds();
      // Limit the user gesture "mostly" to the off screen area and check for
      // noise invocation.
      if (!GestureInBezelArea(screen, event) ||
          BezelGestureMightBeNoise(screen, event))
        return;
      if (HandleDeviceControl(screen, event))
        start_location_ = BEZEL_START_UNSET;
    }
  }
}

void SystemGestureEventFilter::HandleBezelGestureEnd() {
  // All which is needed is to set the gesture start location to undefined.
  start_location_ = BEZEL_START_UNSET;
}

bool SystemGestureEventFilter::GestureInBezelArea(
    const gfx::Rect& screen, ui::GestureEvent* event) {
  // Limit the gesture mostly to the off screen.
  double allowable_offset =
      screen.width() * kAllowableScreenOverlapForDeviceCommand;
  if ((start_location_ == BEZEL_START_LEFT &&
       event->x() > allowable_offset) ||
      (start_location_ == BEZEL_START_RIGHT &&
       event->x() < screen.width() - allowable_offset)) {
    start_location_ = BEZEL_START_UNSET;
    return false;
  }
  return true;
}

bool SystemGestureEventFilter::BezelGestureMightBeNoise(
    const gfx::Rect& screen, ui::GestureEvent* event) {
  // The first events will not trigger an action.
  if (initiation_delay_events_++ < kIgnoreFirstBezelDeviceEvents) {
    // When the values are too far apart we ignore it since it might
    // be random noise.
    double delta_y = event->details().scroll_y();
    double span_y = screen.height();
    if (abs(delta_y / span_y) > kBezelNoiseDeltaFilter)
      start_location_ = BEZEL_START_UNSET;
    return true;
  }
  return false;
}

bool SystemGestureEventFilter::DeNoiseBezelSliderPosition(double& percent) {
  // The range gets passed as 0..100% and is extended to the range of
  // (-kMinMaxInsetPercent) .. (100 + kMinMaxInsetPercent). This way we can
  // cut off the extreme upper and lower values which are prone to noise.
  // It additionally adds a "security buffer" which can then be clamped to the
  // extremes to empower the user to get to these values (0% and 100%).
  percent = percent * (100.0 + 2 * kMinMaxInsetPercent) / 100 -
      kMinMaxInsetPercent;
  // Values which fall outside of the acceptable inner range area get ignored.
  if (percent < -kMinMaxCutOffPercent ||
      percent > 100.0 + kMinMaxCutOffPercent)
    return false;
  // Excessive values get then clamped to the 0..100% range.
  percent = std::max(std::min(percent, 100.0), 0.0);
  return true;
}

double SystemGestureEventFilter::LimitBezelBrightnessFromSlider(
    double percent) {
  // Turning off the display makes no sense, so we map the accessible range to
  // kMinimumBrightness .. 100%.
  percent = (percent + kMinBrightnessPercent) * 100.0 /
      (100.0 + kMinBrightnessPercent);
  // Clamp to avoid rounding issues.
  return std::min(percent, 100.0);
}

}  // namespace internal
}  // namespace ash
