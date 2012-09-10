// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accelerators/accelerator_filter.h"

#include "ash/accelerators/accelerator_controller.h"
#include "ash/shell.h"
#include "ash/wm/window_util.h"
#include "ui/aura/root_window.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/accelerators/accelerator_manager.h"
#include "ui/base/events/event.h"

namespace ash {
namespace {

const int kModifierFlagMask = (ui::EF_SHIFT_DOWN |
                               ui::EF_CONTROL_DOWN |
                               ui::EF_ALT_DOWN);

// Returns true if the |accelerator| should be processed now, inside Ash's env
// event filter.
bool ShouldProcessAcceleratorsNow(const ui::Accelerator& accelerator,
                                  aura::Window* target) {
  if (!target)
    return true;
  if (target == Shell::GetPrimaryRootWindow())
    return true;

  // A full screen window should be able to handle all key events including the
  // reserved ones.
  if (wm::IsWindowFullscreen(target)) {
    // TODO(yusukes): On Chrome OS, only browser and flash windows can be full
    // screen. Launching an app in "open full-screen" mode is not supported yet.
    // That makes the IsWindowFullscreen() check above almost meaningless
    // because a browser and flash window do handle Ash accelerators anyway
    // before they're passed to a page or flash content.
    return false;
  }

  if (Shell::GetInstance()->GetAppListTargetVisibility())
    return true;

  // Unless |target| is in the full screen state, handle reserved accelerators
  // such as Alt+Tab now.
  return Shell::GetInstance()->accelerator_controller()->IsReservedAccelerator(
      accelerator);
}

}  // namespace

namespace internal {

////////////////////////////////////////////////////////////////////////////////
// AcceleratorFilter, public:

AcceleratorFilter::AcceleratorFilter() {
}

AcceleratorFilter::~AcceleratorFilter() {
}

////////////////////////////////////////////////////////////////////////////////
// AcceleratorFilter, EventFilter implementation:

bool AcceleratorFilter::PreHandleKeyEvent(aura::Window* target,
                                          ui::KeyEvent* event) {
  const ui::EventType type = event->type();
  if (type != ui::ET_KEY_PRESSED && type != ui::ET_KEY_RELEASED)
    return false;
  if (event->is_char())
    return false;

  ui::Accelerator accelerator(event->key_code(),
                              event->flags() & kModifierFlagMask);
  accelerator.set_type(type);

  if (!ShouldProcessAcceleratorsNow(accelerator, target))
    return false;
  return Shell::GetInstance()->accelerator_controller()->Process(accelerator);
}

bool AcceleratorFilter::PreHandleMouseEvent(aura::Window* target,
                                            ui::MouseEvent* event) {
  return false;
}

ui::TouchStatus AcceleratorFilter::PreHandleTouchEvent(
    aura::Window* target,
    ui::TouchEvent* event) {
  return ui::TOUCH_STATUS_UNKNOWN;
}

ui::EventResult AcceleratorFilter::PreHandleGestureEvent(
    aura::Window* target,
    ui::GestureEvent* event) {
  return ui::ER_UNHANDLED;
}

}  // namespace internal
}  // namespace ash
