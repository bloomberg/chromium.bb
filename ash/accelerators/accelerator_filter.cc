// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accelerators/accelerator_filter.h"

#include "ash/accelerators/accelerator_controller.h"
#include "ash/shell.h"
#include "ui/aura/event.h"
#include "ui/aura/root_window.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/accelerators/accelerator_manager.h"

namespace {

const int kModifierFlagMask = (ui::EF_SHIFT_DOWN |
                               ui::EF_CONTROL_DOWN |
                               ui::EF_ALT_DOWN);

// Returns true if an Ash accelerator should be processed now.
bool ShouldProcessAcceleratorsNow(aura::Window* target) {
  if (!target)
    return true;
  if (target == ash::Shell::GetInstance()->GetRootWindow())
    return true;
  // Unless |target| is the root window, return false to let the custom focus
  // manager (see ash/shell.cc) handle Ash accelerators.
  return false;
}

}  // namespace

namespace ash {
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
                                          aura::KeyEvent* event) {
  const ui::EventType type = event->type();
  if (type != ui::ET_KEY_PRESSED && type != ui::ET_KEY_RELEASED)
    return false;
  if (event->is_char())
    return false;
  if (!ShouldProcessAcceleratorsNow(target))
    return false;

  ui::Accelerator accelerator(event->key_code(),
                              event->flags() & kModifierFlagMask);
  accelerator.set_type(type);
  return Shell::GetInstance()->accelerator_controller()->Process(accelerator);
}

bool AcceleratorFilter::PreHandleMouseEvent(aura::Window* target,
                                            aura::MouseEvent* event) {
  return false;
}

ui::TouchStatus AcceleratorFilter::PreHandleTouchEvent(
    aura::Window* target,
    aura::TouchEvent* event) {
  return ui::TOUCH_STATUS_UNKNOWN;
}

ui::GestureStatus AcceleratorFilter::PreHandleGestureEvent(
    aura::Window* target,
    aura::GestureEvent* event) {
  return ui::GESTURE_STATUS_UNKNOWN;
}

}  // namespace internal
}  // namespace ash
