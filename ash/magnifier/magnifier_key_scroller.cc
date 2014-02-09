// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/magnifier/magnifier_key_scroller.h"

#include <X11/Xlib.h>

#undef RootWindow
#undef Status

#include "ash/ash_switches.h"
#include "ash/magnifier/magnification_controller.h"
#include "ash/shell.h"
#include "base/command_line.h"
#include "base/message_loop/message_loop.h"
#include "ui/aura/root_window.h"
#include "ui/aura/window_tracker.h"

namespace ash {
namespace {

bool magnifier_key_scroller_enabled = false;

void ScrollScreen(ui::KeyEvent* event) {
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

void DispatchPressedEvent(XEvent native_event,
                          scoped_ptr<aura::WindowTracker> tracker) {
  // The target window may be gone.
  if (tracker->windows().empty())
    return;
  aura::Window* target = *(tracker->windows().begin());
  ui::KeyEvent event(&native_event, false);
  event.set_flags(event.flags() | ui::EF_IS_SYNTHESIZED);
  ui::EventDispatchDetails result ALLOW_UNUSED =
      target->GetDispatcher()->OnEventFromSource(&event);
}

void PostPressedEvent(ui::KeyEvent* event) {
  // Modify RELEASED event to PRESSED event.
  XEvent xkey = *(event->native_event());
  xkey.xkey.type = KeyPress;
  xkey.xkey.state |= ShiftMask;
  scoped_ptr<aura::WindowTracker> tracker(new aura::WindowTracker);
  tracker->Add(static_cast<aura::Window*>(event->target()));

  base::MessageLoopForUI::current()->PostTask(
      FROM_HERE,
      base::Bind(&DispatchPressedEvent, xkey, base::Passed(&tracker)));
}

}  // namespace

// static
bool MagnifierKeyScroller::IsEnabled() {
  return (magnifier_key_scroller_enabled ||
          CommandLine::ForCurrentProcess()->HasSwitch(
              switches::kAshEnableMagnifierKeyScroller)) &&
      ash::Shell::GetInstance()->magnification_controller()->IsEnabled();
}

// static
void MagnifierKeyScroller::SetEnabled(bool enabled) {
  magnifier_key_scroller_enabled = enabled;
}

MagnifierKeyScroller::MagnifierKeyScroller()
    : state_(INITIAL) {}

MagnifierKeyScroller::~MagnifierKeyScroller() {}

void MagnifierKeyScroller::OnKeyEvent(
    ui::KeyEvent* event) {
  if (!IsEnabled())
    return;

  if (event->key_code() != ui::VKEY_UP &&
      event->key_code() != ui::VKEY_DOWN &&
      event->key_code() != ui::VKEY_LEFT &&
      event->key_code() != ui::VKEY_RIGHT) {
    return;
  }

  if (event->type() == ui::ET_KEY_PRESSED &&
      event->flags() & ui::EF_SHIFT_DOWN) {
    switch (state_) {
      case INITIAL:
        // Pass through posted event.
        if (event->flags() & ui::EF_IS_SYNTHESIZED) {
          event->set_flags(event->flags() & ~ui::EF_IS_SYNTHESIZED);
          return;
        }
        state_ = PRESSED;
        // Don't process ET_KEY_PRESSED event yet. The ET_KEY_PRESSED
        // event will be generated upon ET_KEY_RELEASEED event below.
        event->StopPropagation();
        break;
      case PRESSED:
        state_ = HOLD;
        // pass through
      case HOLD:
        ScrollScreen(event);
        event->StopPropagation();
        break;
      }
  } else if (event->type() == ui::ET_KEY_RELEASED) {
    switch (state_) {
      case INITIAL:
        break;
      case PRESSED: {
        PostPressedEvent(event);
        event->StopPropagation();
        break;
      }
      case HOLD: {
        MagnificationController* controller =
            Shell::GetInstance()->magnification_controller();
        controller->SetScrollDirection(MagnificationController::SCROLL_NONE);
        event->StopPropagation();
        break;
      }
    }
    state_ = INITIAL;
  }
}

}  // namespace ash
