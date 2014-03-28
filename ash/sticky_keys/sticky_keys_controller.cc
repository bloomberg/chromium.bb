// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/sticky_keys/sticky_keys_controller.h"

#if defined(USE_X11)
#include <X11/extensions/XInput2.h>
#include <X11/Xlib.h>
#undef RootWindow
#endif

#include "ash/sticky_keys/sticky_keys_overlay.h"
#include "base/basictypes.h"
#include "base/debug/stack_trace.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tracker.h"
#include "ui/aura/window_tree_host.h"
#include "ui/events/event.h"
#include "ui/events/event_processor.h"
#include "ui/events/keycodes/keyboard_code_conversion.h"

namespace ash {

namespace {

// Returns true if the type of mouse event should be modified by sticky keys.
bool ShouldModifyMouseEvent(ui::MouseEvent* event) {
  ui::EventType type = event->type();
  return type == ui::ET_MOUSE_PRESSED || type == ui::ET_MOUSE_RELEASED ||
         type == ui::ET_MOUSEWHEEL;
}

// An implementation of StickyKeysHandler::StickyKeysHandlerDelegate.
class StickyKeysHandlerDelegateImpl :
    public StickyKeysHandler::StickyKeysHandlerDelegate {
 public:
  StickyKeysHandlerDelegateImpl();
  virtual ~StickyKeysHandlerDelegateImpl();

  // StickyKeysHandlerDelegate overrides.
  virtual void DispatchKeyEvent(ui::KeyEvent* event,
                                aura::Window* target) OVERRIDE;

  virtual void DispatchMouseEvent(ui::MouseEvent* event,
                                  aura::Window* target) OVERRIDE;

  virtual void DispatchScrollEvent(ui::ScrollEvent* event,
                                   aura::Window* target) OVERRIDE;
 private:
  void DispatchEvent(ui::Event* event, aura::Window* target);

  DISALLOW_COPY_AND_ASSIGN(StickyKeysHandlerDelegateImpl);
};

StickyKeysHandlerDelegateImpl::StickyKeysHandlerDelegateImpl() {
}

StickyKeysHandlerDelegateImpl::~StickyKeysHandlerDelegateImpl() {
}

void StickyKeysHandlerDelegateImpl::DispatchKeyEvent(ui::KeyEvent* event,
                                                     aura::Window* target) {
  DispatchEvent(event, target);
}

void StickyKeysHandlerDelegateImpl::DispatchMouseEvent(ui::MouseEvent* event,
                                                       aura::Window* target) {
  DCHECK(target);
  // We need to send a new, untransformed mouse event to the host.
  if (event->IsMouseWheelEvent()) {
    ui::MouseWheelEvent new_event(event->native_event());
    DispatchEvent(&new_event, target);
  } else {
    ui::MouseEvent new_event(event->native_event());
    DispatchEvent(&new_event, target);
  }
}

void StickyKeysHandlerDelegateImpl::DispatchScrollEvent(
    ui::ScrollEvent* event,
    aura::Window* target)  {
  DispatchEvent(event, target);
}

void StickyKeysHandlerDelegateImpl::DispatchEvent(ui::Event* event,
                                                  aura::Window* target) {
  DCHECK(target);
  ui::EventDispatchDetails details =
      target->GetHost()->event_processor()->OnEventFromSource(event);
  if (details.dispatcher_destroyed)
    return;
}

}  // namespace

///////////////////////////////////////////////////////////////////////////////
//  StickyKeys
StickyKeysController::StickyKeysController()
    : enabled_(false),
      mod3_enabled_(false),
      altgr_enabled_(false) {
}

StickyKeysController::~StickyKeysController() {
}

void StickyKeysController::Enable(bool enabled) {
  if (enabled_ != enabled) {
    enabled_ = enabled;

    // Reset key handlers when activating sticky keys to ensure all
    // the handlers' states are reset.
    if (enabled_) {
      shift_sticky_key_.reset(
          new StickyKeysHandler(ui::EF_SHIFT_DOWN,
                                new StickyKeysHandlerDelegateImpl()));
      alt_sticky_key_.reset(
          new StickyKeysHandler(ui::EF_ALT_DOWN,
                                new StickyKeysHandlerDelegateImpl()));
      altgr_sticky_key_.reset(
          new StickyKeysHandler(ui::EF_ALTGR_DOWN,
                                new StickyKeysHandlerDelegateImpl()));
      ctrl_sticky_key_.reset(
          new StickyKeysHandler(ui::EF_CONTROL_DOWN,
                                new StickyKeysHandlerDelegateImpl()));

      overlay_.reset(new StickyKeysOverlay());
      overlay_->SetModifierVisible(ui::EF_ALTGR_DOWN, altgr_enabled_);
    } else if (overlay_) {
      overlay_->Show(false);
    }
  }
}

void StickyKeysController::SetModifiersEnabled(bool mod3_enabled,
                                               bool altgr_enabled) {
  mod3_enabled_ = mod3_enabled;
  altgr_enabled_ = altgr_enabled;
  if (overlay_)
    overlay_->SetModifierVisible(ui::EF_ALTGR_DOWN, altgr_enabled_);
}

bool StickyKeysController::HandleKeyEvent(ui::KeyEvent* event) {
  return shift_sticky_key_->HandleKeyEvent(event) ||
      alt_sticky_key_->HandleKeyEvent(event) ||
      altgr_sticky_key_->HandleKeyEvent(event) ||
      ctrl_sticky_key_->HandleKeyEvent(event);
}

bool StickyKeysController::HandleMouseEvent(ui::MouseEvent* event) {
  return shift_sticky_key_->HandleMouseEvent(event) ||
      alt_sticky_key_->HandleMouseEvent(event) ||
      altgr_sticky_key_->HandleMouseEvent(event) ||
      ctrl_sticky_key_->HandleMouseEvent(event);
}

bool StickyKeysController::HandleScrollEvent(ui::ScrollEvent* event) {
  return shift_sticky_key_->HandleScrollEvent(event) ||
      alt_sticky_key_->HandleScrollEvent(event) ||
      altgr_sticky_key_->HandleScrollEvent(event) ||
      ctrl_sticky_key_->HandleScrollEvent(event);
}

void StickyKeysController::OnKeyEvent(ui::KeyEvent* event) {
  // Do not consume a translated key event which is generated by an IME.
  if (event->type() == ui::ET_TRANSLATED_KEY_PRESS ||
      event->type() == ui::ET_TRANSLATED_KEY_RELEASE) {
    return;
  }

  if (enabled_) {
    if (HandleKeyEvent(event))
      event->StopPropagation();
    UpdateOverlay();
  }
}

void StickyKeysController::OnMouseEvent(ui::MouseEvent* event) {
  if (enabled_) {
    if (HandleMouseEvent(event))
      event->StopPropagation();
    UpdateOverlay();
  }
}

void StickyKeysController::OnScrollEvent(ui::ScrollEvent* event) {
  if (enabled_) {
    if (HandleScrollEvent(event))
      event->StopPropagation();
    UpdateOverlay();
  }
}

void StickyKeysController::UpdateOverlay() {
  overlay_->SetModifierKeyState(
      ui::EF_SHIFT_DOWN, shift_sticky_key_->current_state());
  overlay_->SetModifierKeyState(
      ui::EF_CONTROL_DOWN, ctrl_sticky_key_->current_state());
  overlay_->SetModifierKeyState(
      ui::EF_ALT_DOWN, alt_sticky_key_->current_state());
  overlay_->SetModifierKeyState(
      ui::EF_ALTGR_DOWN, altgr_sticky_key_->current_state());

  bool key_in_use =
      shift_sticky_key_->current_state() != STICKY_KEY_STATE_DISABLED ||
      alt_sticky_key_->current_state() != STICKY_KEY_STATE_DISABLED ||
      altgr_sticky_key_->current_state() != STICKY_KEY_STATE_DISABLED ||
      ctrl_sticky_key_->current_state() != STICKY_KEY_STATE_DISABLED;

  overlay_->Show(enabled_ && key_in_use);
}

StickyKeysOverlay* StickyKeysController::GetOverlayForTest() {
  return overlay_.get();
}

///////////////////////////////////////////////////////////////////////////////
//  StickyKeysHandler
StickyKeysHandler::StickyKeysHandler(ui::EventFlags modifier_flag,
                                     StickyKeysHandlerDelegate* delegate)
    : modifier_flag_(modifier_flag),
      current_state_(STICKY_KEY_STATE_DISABLED),
      event_from_myself_(false),
      preparing_to_enable_(false),
      scroll_delta_(0),
      delegate_(delegate) {
}

StickyKeysHandler::~StickyKeysHandler() {
}

StickyKeysHandler::StickyKeysHandlerDelegate::StickyKeysHandlerDelegate() {
}

StickyKeysHandler::StickyKeysHandlerDelegate::~StickyKeysHandlerDelegate() {
}

bool StickyKeysHandler::HandleKeyEvent(ui::KeyEvent* event) {
  if (event_from_myself_)
    return false;  // Do not handle self-generated key event.
  switch (current_state_) {
    case STICKY_KEY_STATE_DISABLED:
      return HandleDisabledState(event);
    case STICKY_KEY_STATE_ENABLED:
      return HandleEnabledState(event);
    case STICKY_KEY_STATE_LOCKED:
      return HandleLockedState(event);
  }
  NOTREACHED();
  return false;
}

bool StickyKeysHandler::HandleMouseEvent(ui::MouseEvent* event) {
  if (ShouldModifyMouseEvent(event))
    preparing_to_enable_ = false;

  if (event_from_myself_ || current_state_ == STICKY_KEY_STATE_DISABLED
      || !ShouldModifyMouseEvent(event)) {
    return false;
  }
  DCHECK(current_state_ == STICKY_KEY_STATE_ENABLED ||
         current_state_ == STICKY_KEY_STATE_LOCKED);

  AppendModifier(event);
  // Only disable on the mouse released event in normal, non-locked mode.
  if (current_state_ == STICKY_KEY_STATE_ENABLED &&
      event->type() != ui::ET_MOUSE_PRESSED) {
    current_state_ = STICKY_KEY_STATE_DISABLED;
    DispatchEventAndReleaseModifier(event);
    return true;
  }

  return false;
}

bool StickyKeysHandler::HandleScrollEvent(ui::ScrollEvent* event) {
  preparing_to_enable_ = false;
  if (event_from_myself_ || current_state_ == STICKY_KEY_STATE_DISABLED)
    return false;
  DCHECK(current_state_ == STICKY_KEY_STATE_ENABLED ||
         current_state_ == STICKY_KEY_STATE_LOCKED);

  // We detect a direction change if the current |scroll_delta_| is assigned
  // and the offset of the current scroll event has the opposing sign.
  bool direction_changed = false;
  if (current_state_ == STICKY_KEY_STATE_ENABLED &&
      event->type() == ui::ET_SCROLL) {
    int offset = event->y_offset();
    if (scroll_delta_)
      direction_changed = offset * scroll_delta_ <= 0;
    scroll_delta_ = offset;
  }

  if (!direction_changed)
    AppendModifier(event);

  // We want to modify all the scroll events in the scroll sequence, which ends
  // with a fling start event. We also stop when the scroll sequence changes
  // direction.
  if (current_state_ == STICKY_KEY_STATE_ENABLED &&
      (event->type() == ui::ET_SCROLL_FLING_START || direction_changed)) {
    current_state_ = STICKY_KEY_STATE_DISABLED;
    scroll_delta_ = 0;
    DispatchEventAndReleaseModifier(event);
    return true;
  }

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
  } else if (event->key_code() == ui::VKEY_ALTGR) {
    is_target_key = (modifier_flag_ == ui::EF_ALTGR_DOWN);
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
      if (preparing_to_enable_) {
        preparing_to_enable_ = false;
        scroll_delta_ = 0;
        current_state_ = STICKY_KEY_STATE_ENABLED;
        modifier_up_event_.reset(new ui::KeyEvent(*event));
        return true;
      }
      return false;
    case TARGET_MODIFIER_DOWN:
      preparing_to_enable_ = true;
      return false;
    case NORMAL_KEY_DOWN:
      preparing_to_enable_ = false;
      return false;
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
      current_state_ = STICKY_KEY_STATE_LOCKED;
      modifier_up_event_.reset();
      return true;
    case NORMAL_KEY_DOWN: {
      current_state_ = STICKY_KEY_STATE_DISABLED;
      AppendModifier(event);
      DispatchEventAndReleaseModifier(event);
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
      current_state_ = STICKY_KEY_STATE_DISABLED;
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

void StickyKeysHandler::DispatchEventAndReleaseModifier(ui::Event* event) {
  DCHECK(event->IsKeyEvent() ||
         event->IsMouseEvent() ||
         event->IsScrollEvent());
  DCHECK(modifier_up_event_.get());
  aura::Window* target = static_cast<aura::Window*>(event->target());
  DCHECK(target);
  aura::Window* root_window = target->GetRootWindow();
  DCHECK(root_window);

  aura::WindowTracker window_tracker;
  window_tracker.Add(target);

  event_from_myself_ = true;
  if (event->IsKeyEvent()) {
    delegate_->DispatchKeyEvent(static_cast<ui::KeyEvent*>(event), target);
  } else if (event->IsMouseEvent()) {
    delegate_->DispatchMouseEvent(static_cast<ui::MouseEvent*>(event), target);
  } else {
    delegate_->DispatchScrollEvent(
        static_cast<ui::ScrollEvent*>(event), target);
  }

  // The action triggered above may have destroyed the event target, in which
  // case we will dispatch the modifier up event to the root window instead.
  aura::Window* modifier_up_target =
      window_tracker.Contains(target) ? target : root_window;
  delegate_->DispatchKeyEvent(modifier_up_event_.get(), modifier_up_target);
  event_from_myself_ = false;
}

void StickyKeysHandler::AppendNativeEventMask(unsigned int* state) {
#if defined(USE_X11)
  unsigned int& state_ref = *state;
  switch (modifier_flag_) {
    case ui::EF_CONTROL_DOWN:
      state_ref |= ControlMask;
      break;
    case ui::EF_ALT_DOWN:
      state_ref |= Mod1Mask;
      break;
    case ui::EF_ALTGR_DOWN:
      state_ref |= Mod5Mask;
      break;
    case ui::EF_SHIFT_DOWN:
      state_ref |= ShiftMask;
      break;
    default:
      NOTREACHED();
  }
#endif
}

void StickyKeysHandler::AppendModifier(ui::KeyEvent* event) {
#if defined(USE_X11)
  XEvent* xev = event->native_event();
  if (xev) {
    XKeyEvent* xkey = &(xev->xkey);
    AppendNativeEventMask(&xkey->state);
  }
#elif defined(USE_OZONE)
  NOTIMPLEMENTED() << "Modifier key is not handled";
#endif
  event->set_flags(event->flags() | modifier_flag_);
  event->set_character(ui::GetCharacterFromKeyCode(event->key_code(),
                                                   event->flags()));
  event->NormalizeFlags();
}

void StickyKeysHandler::AppendModifier(ui::MouseEvent* event) {
#if defined(USE_X11)
  // The native mouse event can either be a classic X button event or an
  // XInput2 button event.
  XEvent* xev = event->native_event();
  if (xev) {
    switch (xev->type) {
      case ButtonPress:
      case ButtonRelease: {
        XButtonEvent* xkey = &(xev->xbutton);
        AppendNativeEventMask(&xkey->state);
        break;
      }
      case GenericEvent: {
        XIDeviceEvent* xievent =
            static_cast<XIDeviceEvent*>(xev->xcookie.data);
        CHECK(xievent->evtype == XI_ButtonPress ||
              xievent->evtype == XI_ButtonRelease);
        AppendNativeEventMask(
            reinterpret_cast<unsigned int*>(&xievent->mods.effective));
        break;
      }
      default:
        NOTREACHED();
    }
  }
#elif defined(USE_OZONE)
  NOTIMPLEMENTED() << "Modifier key is not handled";
#endif
  event->set_flags(event->flags() | modifier_flag_);
}

void StickyKeysHandler::AppendModifier(ui::ScrollEvent* event) {
#if defined(USE_X11)
  XEvent* xev = event->native_event();
  if (xev) {
    XIDeviceEvent* xievent =
        static_cast<XIDeviceEvent*>(xev->xcookie.data);
    if (xievent) {
      AppendNativeEventMask(reinterpret_cast<unsigned int*>(
          &xievent->mods.effective));
    }
  }
#elif defined(USE_OZONE)
  NOTIMPLEMENTED() << "Modifier key is not handled";
#endif
  event->set_flags(event->flags() | modifier_flag_);
}

}  // namespace ash
