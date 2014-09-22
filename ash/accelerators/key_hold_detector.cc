// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accelerators/key_hold_detector.h"

#include <X11/Xlib.h>

#undef RootWindow
#undef Status

#include "ash/shell.h"
#include "base/message_loop/message_loop.h"
#include "ui/aura/window_tracker.h"
#include "ui/aura/window_tree_host.h"
#include "ui/events/event_dispatcher.h"
#include "ui/events/event_processor.h"

namespace ash {
namespace {

void DispatchPressedEvent(const ui::KeyEvent& key_event,
                          scoped_ptr<aura::WindowTracker> tracker) {
  // The target window may be gone.
  if (tracker->windows().empty())
    return;
  ui::KeyEvent event(key_event);
  aura::Window* target = *(tracker->windows().begin());
  ui::EventDispatchDetails result ALLOW_UNUSED =
      target->GetHost()->event_processor()->OnEventFromSource(&event);
}

void PostPressedEvent(ui::KeyEvent* event) {
  // Modify RELEASED event to PRESSED event.
  const ui::KeyEvent pressed_event(
      ui::ET_KEY_PRESSED,
      event->key_code(),
      event->code(),
      event->flags() | ui::EF_SHIFT_DOWN | ui::EF_IS_SYNTHESIZED);
  scoped_ptr<aura::WindowTracker> tracker(new aura::WindowTracker);
  tracker->Add(static_cast<aura::Window*>(event->target()));

  base::MessageLoopForUI::current()->PostTask(
      FROM_HERE,
      base::Bind(&DispatchPressedEvent, pressed_event, base::Passed(&tracker)));
}

}  // namespace

KeyHoldDetector::KeyHoldDetector(scoped_ptr<Delegate> delegate)
    : state_(INITIAL),
      delegate_(delegate.Pass()) {}

KeyHoldDetector::~KeyHoldDetector() {}

void KeyHoldDetector::OnKeyEvent(ui::KeyEvent* event) {
  if (!delegate_->ShouldProcessEvent(event))
    return;

  if (delegate_->IsStartEvent(event)) {
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
        delegate_->OnKeyHold(event);
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
        delegate_->OnKeyUnhold(event);
        event->StopPropagation();
        break;
      }
    }
    state_ = INITIAL;
  }
}

}  // namespace ash
