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
}  // namespace

namespace ash {
namespace internal {

////////////////////////////////////////////////////////////////////////////////
// AcceleratorFilter, public:

AcceleratorFilter::AcceleratorFilter()
    : EventFilter(aura::RootWindow::GetInstance()) {
}

AcceleratorFilter::~AcceleratorFilter() {
}

////////////////////////////////////////////////////////////////////////////////
// AcceleratorFilter, EventFilter implementation:

bool AcceleratorFilter::PreHandleKeyEvent(aura::Window* target,
                                          aura::KeyEvent* event) {
  if (event->type() == ui::ET_KEY_PRESSED && !event->is_char()) {
    return Shell::GetInstance()->accelerator_controller()->Process(
        ui::Accelerator(event->key_code(),
                        event->flags() & kModifierFlagMask));
  }
  return false;
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
