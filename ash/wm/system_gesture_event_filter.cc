// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/system_gesture_event_filter.h"

#include "ash/accelerators/accelerator_controller.h"
#include "ash/accelerators/accelerator_table.h"
#include "ash/ash_switches.h"
#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "ash/shell_window_ids.h"
#include "ash/wm/gestures/bezel_gesture_handler.h"
#include "ash/wm/gestures/long_press_affordance_handler.h"
#include "ash/wm/gestures/system_pinch_handler.h"
#include "ash/wm/gestures/two_finger_drag_handler.h"
#include "ash/wm/property_util.h"
#include "ash/wm/window_util.h"
#include "base/command_line.h"
#include "ui/aura/root_window.h"
#include "ui/base/events/event.h"
#include "ui/base/ui_base_switches.h"

#if defined(OS_CHROMEOS)
#include "ui/base/touch/touch_factory.h"
#endif

namespace {

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
      system_gestures_enabled_(CommandLine::ForCurrentProcess()->
          HasSwitch(ash::switches::kAshEnableAdvancedGestures)),
      bezel_gestures_(new BezelGestureHandler),
      long_press_affordance_(new LongPressAffordanceHandler),
      two_finger_drag_(new TwoFingerDragHandler) {
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

ui::EventResult SystemGestureEventFilter::PreHandleGestureEvent(
    aura::Window* target,
    ui::GestureEvent* event) {
  touch_uma_.RecordGestureEvent(target, *event);
  long_press_affordance_->ProcessEvent(target, event,
      event->GetLowestTouchId());

  if (!target || target == target->GetRootWindow()) {
    bezel_gestures_->ProcessGestureEvent(target, *event);
    return ui::ER_CONSUMED;
  }

  if (two_finger_drag_->ProcessGestureEvent(target, *event))
    return ui::ER_CONSUMED;

  if (!system_gestures_enabled_)
    return ui::ER_UNHANDLED;

  aura::Window* system_target = GetTargetForSystemGestureEvent(target);
  if (!system_target)
    return ui::ER_UNHANDLED;

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
        return ui::ER_CONSUMED;
    }
    return ui::ER_UNHANDLED;
  }

  WindowPinchHandlerMap::iterator find = pinch_handlers_.find(system_target);
  if (find != pinch_handlers_.end()) {
    SystemGestureStatus status =
        (*find).second->ProcessGestureEvent(*event);
    if (status == SYSTEM_GESTURE_END)
      ClearGestureHandlerForWindow(system_target);
    return ui::ER_CONSUMED;
  } else {
    if (event->type() == ui::ET_GESTURE_BEGIN &&
        event->details().touch_points() >=
        SystemPinchHandler::kSystemGesturePoints) {
      pinch_handlers_[system_target] = new SystemPinchHandler(system_target);
      system_target->AddObserver(this);
      return ui::ER_CONSUMED;
    }
  }

  return ui::ER_UNHANDLED;
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
}  // namespace internal
}  // namespace ash
