// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/caps_lock_rewriter.h"

#include "base/chromeos/chromeos_version.h"
#include "chromeos/ime/input_method_manager.h"
#include "chromeos/ime/xkeyboard.h"
#include "ui/base/events/event.h"

namespace chromeos {

CapsLockRewriter::CapsLockRewriter()
    : state_(NONE),
      xkeyboard_for_testing_(NULL),
      trigger_key_(ui::VKEY_F17),
      trigger_key_down_(false),
      saw_other_key_while_trigger_key_down_(false),
      long_press_duration_(
          base::TimeDelta::FromMilliseconds(kDefaultLongPressDurationInMs)),
      double_press_duration_(
          base::TimeDelta::FromMilliseconds(kDefaultDoublePressDurationInMs)) {
  // Use an available key when running on the dev box.
  if (!base::chromeos::IsRunningOnChromeOS())
    trigger_key_ = ui::VKEY_NUMPAD0;
}

CapsLockRewriter::~CapsLockRewriter() {}

void CapsLockRewriter::RewriteEvent(ui::KeyEvent* event,
                                    const base::TimeTicks& now) {
  const bool is_trigger_key = event->key_code() == trigger_key_;

  input_method::XKeyboard* xkeyboard = xkeyboard_for_testing_ ?
      xkeyboard_for_testing_ :
      chromeos::input_method::InputMethodManager::Get()->GetXKeyboard();
  const bool caps_lock = xkeyboard->CapsLockIsEnabled();

  bool should_reset = false;
  switch (state_) {
    case NONE:
      if (!caps_lock &&
          is_trigger_key &&
          event->type() == ui::ET_KEY_PRESSED) {
        state_ = TRANSIENT;
        xkeyboard->SetCapsLockEnabled(true);
        trigger_key_down_tick_ = now;
        trigger_key_down_ = true;
        saw_other_key_while_trigger_key_down_ = false;
      }
      break;
    case TRANSIENT:
      if (!caps_lock) {
        state_ = NONE;
        break;
      }

      if (trigger_key_down_ && !is_trigger_key)
        saw_other_key_while_trigger_key_down_ = true;

      if (event->type() == ui::ET_KEY_PRESSED && is_trigger_key) {
        const base::TimeDelta time_since_last_press =
            now - trigger_key_down_tick_;
        if (time_since_last_press < double_press_duration_) {
          state_ = ON;
          trigger_key_down_ = true;
          saw_other_key_while_trigger_key_down_ = false;
        }
      }

      if (event->type() == ui::ET_KEY_RELEASED) {
        should_reset = true;
        if (is_trigger_key) {
          if (!saw_other_key_while_trigger_key_down_ && trigger_key_down_) {
            trigger_key_down_ = false;
            should_reset = false;
            const base::TimeDelta trigger_key_held_time =
                now - trigger_key_down_tick_;
            if (trigger_key_held_time > long_press_duration_)
              state_ = ON;
          }
        } else if (trigger_key_down_) {
          should_reset = false;
        }
      }

      break;
    case ON:
      if (!caps_lock) {
        state_ = NONE;
        break;
      }

      if (trigger_key_down_ && !is_trigger_key) {
        saw_other_key_while_trigger_key_down_ = true;
        state_ = TRANSIENT;
      }

      if (event->type() == ui::ET_KEY_RELEASED && is_trigger_key) {
        if (trigger_key_down_)
          trigger_key_down_ = false;
        else
          should_reset = true;
      }

      break;
  }

  if (caps_lock && should_reset) {
    state_ = NONE;
    xkeyboard->SetCapsLockEnabled(false);
  }

  if (is_trigger_key)
    event->StopPropagation();
}

}  // namespace chromeos
