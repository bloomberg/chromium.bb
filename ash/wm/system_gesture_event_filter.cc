// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/system_gesture_event_filter.h"

#include "ash/shell.h"
#include "ash/accelerators/accelerator_controller.h"
#include "ash/system/brightness/brightness_control_delegate.h"
#include "ash/volume_control_delegate.h"
#include "ui/aura/event.h"
#include "ui/aura/root_window.h"
#include "ui/gfx/screen.h"

namespace ash {
namespace internal {

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
  // TODO(tdresser) handle system level gesture events
  if (event->type() == ui::ET_GESTURE_THREE_FINGER_SWIPE)
    return ui::GESTURE_STATUS_CONSUMED;
  if (!target || target == Shell::GetRootWindow()) {
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
  return ui::GESTURE_STATUS_UNKNOWN;
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
        ui::Accelerator(ui::VKEY_LWIN, false, true, false));
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
        ui::Accelerator(ui::VKEY_F5, true, false, false));
  else if (start_location_ == BEZEL_START_RIGHT && event->delta_x() < 0)
    // We leave the work to switch to the previous window to our accelerators.
    accelerator->AcceleratorPressed(
        ui::Accelerator(ui::VKEY_F5, false, false, false));
  else
    return false;
  // No further notifications for this gesture.
  return true;
}

}  // namespace internal
}  // namespace ash
