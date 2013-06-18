// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/sticky_keys.h"

#include <X11/Xlib.h>
#undef RootWindow

#include "base/basictypes.h"
#include "base/debug/stack_trace.h"
#include "ui/aura/root_window.h"
#include "ui/base/events/event.h"
#include "ui/base/keycodes/keyboard_code_conversion.h"

namespace ash {

namespace {

// An implementation of StickyKeysHandler::StickyKeysHandlerDelegate.
class StickyKeysHandlerDelegateImpl :
    public StickyKeysHandler::StickyKeysHandlerDelegate {
 public:
  StickyKeysHandlerDelegateImpl();
  virtual ~StickyKeysHandlerDelegateImpl();

  // StickyKeysHandlerDelegate overrides.
  virtual void DispatchKeyEvent(ui::KeyEvent* event,
                                aura::Window* target) OVERRIDE;
 private:
  DISALLOW_COPY_AND_ASSIGN(StickyKeysHandlerDelegateImpl);
};

StickyKeysHandlerDelegateImpl::StickyKeysHandlerDelegateImpl() {
}

StickyKeysHandlerDelegateImpl::~StickyKeysHandlerDelegateImpl() {
}

void StickyKeysHandlerDelegateImpl::DispatchKeyEvent(ui::KeyEvent* event,
                                                     aura::Window* target) {
  DCHECK(target);
  target->GetRootWindow()->AsRootWindowHostDelegate()->OnHostKeyEvent(event);
}

}  // namespace

///////////////////////////////////////////////////////////////////////////////
//  StickyKeys
StickyKeys::StickyKeys()
    : shift_sticky_key_(
          new StickyKeysHandler(ui::EF_SHIFT_DOWN,
                              new StickyKeysHandlerDelegateImpl())),
      alt_sticky_key_(
          new StickyKeysHandler(ui::EF_ALT_DOWN,
                                new StickyKeysHandlerDelegateImpl())),
      ctrl_sticky_key_(
          new StickyKeysHandler(ui::EF_CONTROL_DOWN,
                                new StickyKeysHandlerDelegateImpl())) {
}

StickyKeys::~StickyKeys() {
}

bool StickyKeys::HandleKeyEvent(ui::KeyEvent* event) {
  return shift_sticky_key_->HandleKeyEvent(event) ||
      alt_sticky_key_->HandleKeyEvent(event) ||
      ctrl_sticky_key_->HandleKeyEvent(event);
}

///////////////////////////////////////////////////////////////////////////////
//  StickyKeysHandler
StickyKeysHandler::StickyKeysHandler(ui::EventFlags target_modifier_flag,
                                     StickyKeysHandlerDelegate* delegate)
    : modifier_flag_(target_modifier_flag),
      current_state_(DISABLED),
      keyevent_from_myself_(false),
      delegate_(delegate) {
}

StickyKeysHandler::~StickyKeysHandler() {
}

StickyKeysHandler::StickyKeysHandlerDelegate::StickyKeysHandlerDelegate() {
}

StickyKeysHandler::StickyKeysHandlerDelegate::~StickyKeysHandlerDelegate() {
}

bool StickyKeysHandler::HandleKeyEvent(ui::KeyEvent* event) {
  if (keyevent_from_myself_)
    return false;  // Do not handle self-generated key event.
  switch (current_state_) {
    case DISABLED:
      return HandleDisabledState(event);
    case ENABLED:
      return HandleEnabledState(event);
    case LOCKED:
      return HandleLockedState(event);
  }
  NOTREACHED();
  return false;
}

StickyKeysHandler::KeyEventType
    StickyKeysHandler::TranslateKeyEvent(ui::KeyEvent* event) {
  bool is_target_key = false;
  if (event->key_code() == ui::VKEY_SHIFT ||
      event->key_code() == ui::VKEY_LSHIFT ||
      event->key_code() == ui::VKEY_RSHIFT) {
    is_target_key = (modifier_flag_ == ui::EF_SHIFT_DOWN);
  } else if (event->key_code() == ui::VKEY_CONTROL ||
      event->key_code() == ui::VKEY_LCONTROL ||
      event->key_code() == ui::VKEY_RCONTROL) {
    is_target_key = (modifier_flag_ == ui::EF_CONTROL_DOWN);
  } else if (event->key_code() == ui::VKEY_MENU ||
      event->key_code() == ui::VKEY_LMENU ||
      event->key_code() == ui::VKEY_RMENU) {
    is_target_key = (modifier_flag_ == ui::EF_ALT_DOWN);
  } else {
    return event->type() == ui::ET_KEY_PRESSED ?
        NORMAL_KEY_DOWN : NORMAL_KEY_UP;
  }

  if (is_target_key) {
    return event->type() == ui::ET_KEY_PRESSED ?
        TARGET_MODIFIER_DOWN : TARGET_MODIFIER_UP;
  }
  return event->type() == ui::ET_KEY_PRESSED ?
      OTHER_MODIFIER_DOWN : OTHER_MODIFIER_UP;
}

bool StickyKeysHandler::HandleDisabledState(ui::KeyEvent* event) {
  switch (TranslateKeyEvent(event)) {
    case TARGET_MODIFIER_UP:
      current_state_ = ENABLED;
      modifier_up_event_.reset(event->Copy());
      return true;
    case TARGET_MODIFIER_DOWN:
    case NORMAL_KEY_DOWN:
    case NORMAL_KEY_UP:
    case OTHER_MODIFIER_DOWN:
    case OTHER_MODIFIER_UP:
      return false;
  }
  NOTREACHED();
  return false;
}

bool StickyKeysHandler::HandleEnabledState(ui::KeyEvent* event) {
  switch (TranslateKeyEvent(event)) {
    case NORMAL_KEY_UP:
    case TARGET_MODIFIER_DOWN:
      return true;
    case TARGET_MODIFIER_UP:
      current_state_ = LOCKED;
      modifier_up_event_.reset();
      return true;
    case NORMAL_KEY_DOWN: {
      DCHECK(modifier_up_event_.get());
      aura::Window* target = static_cast<aura::Window*>(event->target());
      DCHECK(target);

      current_state_ = DISABLED;
      AppendModifier(event);
      // We can't post event, so dispatch current keyboard event first then
      // dispatch next keyboard event.
      keyevent_from_myself_ = true;
      delegate_->DispatchKeyEvent(event, target);
      delegate_->DispatchKeyEvent(modifier_up_event_.get(), target);
      keyevent_from_myself_ = false;
      return true;
    }
    case OTHER_MODIFIER_DOWN:
    case OTHER_MODIFIER_UP:
      return false;
  }
  NOTREACHED();
  return false;
}

bool StickyKeysHandler::HandleLockedState(ui::KeyEvent* event) {
  switch (TranslateKeyEvent(event)) {
    case TARGET_MODIFIER_DOWN:
      return true;
    case TARGET_MODIFIER_UP:
      current_state_ = DISABLED;
      return false;
    case NORMAL_KEY_DOWN:
    case NORMAL_KEY_UP:
      AppendModifier(event);
      return false;
    case OTHER_MODIFIER_DOWN:
    case OTHER_MODIFIER_UP:
      return false;
  }
  NOTREACHED();
  return false;
}

void StickyKeysHandler::AppendModifier(ui::KeyEvent* event) {
  XEvent* xev = event->native_event();
  XKeyEvent* xkey = &(xev->xkey);
  switch (modifier_flag_) {
    case ui::EF_CONTROL_DOWN:
      xkey->state |= ControlMask;
      break;
    case ui::EF_ALT_DOWN:
      xkey->state |= Mod1Mask;
      break;
    case ui::EF_SHIFT_DOWN:
      xkey->state |= ShiftMask;
      break;
    default:
      NOTREACHED();
  }
  event->set_flags(event->flags() | modifier_flag_);
  event->set_character(ui::GetCharacterFromKeyCode(event->key_code(),
                                                   event->flags()));
  event->NormalizeFlags();
}

}  // namespace ash
