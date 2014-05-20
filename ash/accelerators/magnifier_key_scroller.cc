// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accelerators/magnifier_key_scroller.h"

#include "ash/accelerators/key_hold_detector.h"
#include "ash/ash_switches.h"
#include "ash/magnifier/magnification_controller.h"
#include "ash/shell.h"
#include "base/command_line.h"

namespace ash {
namespace {
bool magnifier_key_scroller_enabled = false;
}

// static
bool MagnifierKeyScroller::IsEnabled() {
  bool has_switch = false;
#if defined(OS_CHROMEOS)
  has_switch = CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kAshEnableMagnifierKeyScroller);
#endif

  return (magnifier_key_scroller_enabled || has_switch) &&
      ash::Shell::GetInstance()->magnification_controller()->IsEnabled();
}

// static
void MagnifierKeyScroller::SetEnabled(bool enabled) {
  magnifier_key_scroller_enabled = enabled;
}

// static
scoped_ptr<ui::EventHandler> MagnifierKeyScroller::CreateHandler() {
  scoped_ptr<KeyHoldDetector::Delegate> delegate(new MagnifierKeyScroller());
  return scoped_ptr<ui::EventHandler>(new KeyHoldDetector(delegate.Pass()));
}

bool MagnifierKeyScroller::ShouldProcessEvent(const ui::KeyEvent* event) const {
  return IsEnabled() &&
      (event->key_code() == ui::VKEY_UP ||
       event->key_code() == ui::VKEY_DOWN ||
       event->key_code() == ui::VKEY_LEFT ||
       event->key_code() == ui::VKEY_RIGHT);
}

bool MagnifierKeyScroller::IsStartEvent(const ui::KeyEvent* event) const {
  return event->type() == ui::ET_KEY_PRESSED &&
      event->flags() & ui::EF_SHIFT_DOWN;
}

void MagnifierKeyScroller::OnKeyHold(const ui::KeyEvent* event) {
  MagnificationController* controller =
      Shell::GetInstance()->magnification_controller();
  switch (event->key_code()) {
    case ui::VKEY_UP:
      controller->SetScrollDirection(MagnificationController::SCROLL_UP);
      break;
    case ui::VKEY_DOWN:
      controller->SetScrollDirection(MagnificationController::SCROLL_DOWN);
      break;
    case ui::VKEY_LEFT:
      controller->SetScrollDirection(MagnificationController::SCROLL_LEFT);
      break;
    case ui::VKEY_RIGHT:
      controller->SetScrollDirection(MagnificationController::SCROLL_RIGHT);
      break;
    default:
      NOTREACHED() << "Unknown keyboard_code:" << event->key_code();
  }
}

void MagnifierKeyScroller::OnKeyUnhold(const ui::KeyEvent* event) {
  MagnificationController* controller =
      Shell::GetInstance()->magnification_controller();
  controller->SetScrollDirection(MagnificationController::SCROLL_NONE);
}

MagnifierKeyScroller::MagnifierKeyScroller() {}

MagnifierKeyScroller::~MagnifierKeyScroller() {}

}  // namespace ash
