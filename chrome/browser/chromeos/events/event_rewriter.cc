// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/events/event_rewriter.h"

#include <vector>

#include "ash/sticky_keys/sticky_keys_controller.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/prefs/pref_service.h"
#include "base/strings/string_util.h"
#include "base/sys_info.h"
#include "chrome/browser/chromeos/login/ui/login_display_host_impl.h"
#include "chrome/browser/chromeos/login/users/user_manager.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/pref_names.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/ime/ime_keyboard.h"
#include "chromeos/ime/input_method_manager.h"
#include "ui/events/event.h"
#include "ui/events/event_utils.h"
#include "ui/events/keycodes/keyboard_code_conversion.h"
#include "ui/wm/core/window_util.h"

#if defined(USE_X11)
#include <X11/extensions/XInput2.h>
#include <X11/Xlib.h>
// Get rid of macros from Xlib.h that conflicts with other parts of the code.
#undef RootWindow
#undef Status

#include "ui/base/x/x11_util.h"
#include "ui/events/keycodes/keyboard_code_conversion_x.h"
#endif

namespace chromeos {

namespace {

// Table of key properties of remappable keys and/or remapping targets.
// This is searched in two distinct ways:
//  - |remap_to| is an |input_method::ModifierKey|, which is the form
//    held in user preferences. |GetRemappedKey()| maps this to the
//    corresponding |key_code| and characterstic event |flag|.
//  - |flag| is a |ui::EventFlags|. |GetRemappedModifierMasks()| maps this
//    to the corresponding user preference |pref_name| for that flag's
//    key, so that it can then be remapped as above.
// In addition |kModifierRemappingCtrl| is a direct reference to the
// Control key entry in the table, used in handling Chromebook Diamond
// and Apple Command keys.
const struct ModifierRemapping {
  int remap_to;
  int flag;
  ui::KeyboardCode key_code;
  const char* pref_name;
} kModifierRemappings[] = {
      {input_method::kSearchKey, ui::EF_COMMAND_DOWN, ui::VKEY_LWIN,
       prefs::kLanguageRemapSearchKeyTo},
      {input_method::kControlKey, ui::EF_CONTROL_DOWN, ui::VKEY_CONTROL,
       prefs::kLanguageRemapControlKeyTo},
      {input_method::kAltKey, ui::EF_ALT_DOWN, ui::VKEY_MENU,
       prefs::kLanguageRemapAltKeyTo},
      {input_method::kVoidKey, 0, ui::VKEY_UNKNOWN, NULL},
      {input_method::kCapsLockKey, ui::EF_MOD3_DOWN, ui::VKEY_CAPITAL,
       prefs::kLanguageRemapCapsLockKeyTo},
      {input_method::kEscapeKey, 0, ui::VKEY_ESCAPE, NULL},
      {0, 0, ui::VKEY_F15, prefs::kLanguageRemapDiamondKeyTo},
};

const ModifierRemapping* kModifierRemappingCtrl = &kModifierRemappings[1];

// Gets a remapped key for |pref_name| key. For example, to find out which
// key Search is currently remapped to, call the function with
// prefs::kLanguageRemapSearchKeyTo.
const ModifierRemapping* GetRemappedKey(const std::string& pref_name,
                                        const PrefService& pref_service) {
  if (!pref_service.FindPreference(pref_name.c_str()))
    return NULL;  // The |pref_name| hasn't been registered. On login screen?
  const int value = pref_service.GetInteger(pref_name.c_str());
  for (size_t i = 0; i < arraysize(kModifierRemappings); ++i) {
    if (value == kModifierRemappings[i].remap_to)
      return &kModifierRemappings[i];
  }
  return NULL;
}

bool HasDiamondKey() {
  return CommandLine::ForCurrentProcess()->HasSwitch(
      chromeos::switches::kHasChromeOSDiamondKey);
}

bool IsISOLevel5ShiftUsedByCurrentInputMethod() {
  // Since both German Neo2 XKB layout and Caps Lock depend on Mod3Mask,
  // it's not possible to make both features work. For now, we don't remap
  // Mod3Mask when Neo2 is in use.
  // TODO(yusukes): Remove the restriction.
  input_method::InputMethodManager* manager =
      input_method::InputMethodManager::Get();
  return manager->IsISOLevel5ShiftUsedByCurrentInputMethod();
}

EventRewriter::DeviceType GetDeviceType(const std::string& device_name) {
  std::vector<std::string> tokens;
  Tokenize(device_name, " .", &tokens);

  // If the |device_name| contains the two words, "apple" and "keyboard", treat
  // it as an Apple keyboard.
  bool found_apple = false;
  bool found_keyboard = false;
  for (size_t i = 0; i < tokens.size(); ++i) {
    if (!found_apple && LowerCaseEqualsASCII(tokens[i], "apple"))
      found_apple = true;
    if (!found_keyboard && LowerCaseEqualsASCII(tokens[i], "keyboard"))
      found_keyboard = true;
    if (found_apple && found_keyboard)
      return EventRewriter::kDeviceAppleKeyboard;
  }

  return EventRewriter::kDeviceUnknown;
}

}  // namespace

EventRewriter::EventRewriter(ash::StickyKeysController* sticky_keys_controller)
    : last_keyboard_device_id_(ui::ED_UNKNOWN_DEVICE),
      ime_keyboard_for_testing_(NULL),
      pref_service_for_testing_(NULL),
      sticky_keys_controller_(sticky_keys_controller),
      current_diamond_key_modifier_flags_(ui::EF_NONE) {
}

EventRewriter::~EventRewriter() {
}

EventRewriter::DeviceType EventRewriter::KeyboardDeviceAddedForTesting(
    int device_id,
    const std::string& device_name) {
  // Tests must avoid XI2 reserved device IDs.
  DCHECK((device_id < 0) || (device_id > 1));
  return KeyboardDeviceAddedInternal(device_id, device_name);
}

void EventRewriter::RewriteMouseButtonEventForTesting(
    const ui::MouseEvent& event,
    scoped_ptr<ui::Event>* rewritten_event) {
  RewriteMouseButtonEvent(event, rewritten_event);
}

ui::EventRewriteStatus EventRewriter::RewriteEvent(
    const ui::Event& event,
    scoped_ptr<ui::Event>* rewritten_event) {
  if ((event.type() == ui::ET_KEY_PRESSED) ||
      (event.type() == ui::ET_KEY_RELEASED)) {
    return RewriteKeyEvent(static_cast<const ui::KeyEvent&>(event),
                           rewritten_event);
  }
  if ((event.type() == ui::ET_MOUSE_PRESSED) ||
      (event.type() == ui::ET_MOUSE_RELEASED)) {
    return RewriteMouseButtonEvent(static_cast<const ui::MouseEvent&>(event),
                                   rewritten_event);
  }
  if (event.type() == ui::ET_MOUSEWHEEL) {
    return RewriteMouseWheelEvent(
        static_cast<const ui::MouseWheelEvent&>(event), rewritten_event);
  }
  if ((event.type() == ui::ET_TOUCH_PRESSED) ||
      (event.type() == ui::ET_TOUCH_RELEASED)) {
    return RewriteTouchEvent(static_cast<const ui::TouchEvent&>(event),
                             rewritten_event);
  }
  if (event.IsScrollEvent()) {
    return RewriteScrollEvent(static_cast<const ui::ScrollEvent&>(event),
                              rewritten_event);
  }
  return ui::EVENT_REWRITE_CONTINUE;
}

ui::EventRewriteStatus EventRewriter::NextDispatchEvent(
    const ui::Event& last_event,
    scoped_ptr<ui::Event>* new_event) {
  if (sticky_keys_controller_) {
    // In the case of sticky keys, we know what the events obtained here are:
    // modifier key releases that match the ones previously discarded. So, we
    // know that they don't have to be passed through the post-sticky key
    // rewriting phases, |RewriteExtendedKeys()| and |RewriteFunctionKeys()|,
    // because those phases do nothing with modifier key releases.
    return sticky_keys_controller_->NextDispatchEvent(new_event);
  }
  NOTREACHED();
  return ui::EVENT_REWRITE_CONTINUE;
}

void EventRewriter::BuildRewrittenKeyEvent(
    const ui::KeyEvent& key_event,
    ui::KeyboardCode key_code,
    int flags,
    scoped_ptr<ui::Event>* rewritten_event) {
  ui::KeyEvent* rewritten_key_event = NULL;
#if defined(USE_X11)
  XEvent* xev = key_event.native_event();
  if (xev) {
    XEvent xkeyevent;
    // Convert all XI2-based key events into X11 core-based key events,
    // until consumers no longer depend on receiving X11 core events.
    if (xev->type == GenericEvent)
      ui::InitXKeyEventFromXIDeviceEvent(*xev, &xkeyevent);
    else
      xkeyevent.xkey = xev->xkey;

    unsigned int original_x11_keycode = xkeyevent.xkey.keycode;
    // Update native event to match rewritten |ui::Event|.
    // The X11 keycode represents a physical key position, so it shouldn't
    // change unless we have actually changed keys, not just modifiers.
    // This is one guard against problems like crbug.com/390263.
    if (key_event.key_code() != key_code) {
      xkeyevent.xkey.keycode =
          XKeyCodeForWindowsKeyCode(key_code, flags, gfx::GetXDisplay());
    }
    ui::KeyEvent x11_key_event(&xkeyevent);
    rewritten_key_event = new ui::KeyEvent(x11_key_event);

    // For numpad keys, the key char should always NOT be changed because
    // XKeyCodeForWindowsKeyCode method cannot handle non-US keyboard layout.
    // The correct key char can be got from original X11 keycode but not for the
    // rewritten X11 keycode.
    // For Shift+NumpadKey cases, use the rewritten X11 keycode (US layout).
    // Please see crbug.com/335644.
    if (key_code >= ui::VKEY_NUMPAD0 && key_code <= ui::VKEY_DIVIDE) {
      XEvent numpad_xevent;
      numpad_xevent.xkey = xkeyevent.xkey;
      // Remove the shift state before getting key char.
      // Because X11/XKB sometimes returns unexpected key char for
      // Shift+NumpadKey. e.g. Shift+Numpad_4 returns 'D', etc.
      numpad_xevent.xkey.state &= ~ShiftMask;
      numpad_xevent.xkey.state |= Mod2Mask;  // Always set NumLock mask.
      if (!(flags & ui::EF_SHIFT_DOWN))
        numpad_xevent.xkey.keycode = original_x11_keycode;
      rewritten_key_event->set_character(
          ui::GetCharacterFromXEvent(&numpad_xevent));
    }
  }
#endif
  if (!rewritten_key_event)
    rewritten_key_event = new ui::KeyEvent(key_event);
  rewritten_key_event->set_flags(flags);
  rewritten_key_event->set_key_code(key_code);
#if defined(USE_X11)
  ui::UpdateX11EventForFlags(rewritten_key_event);
  rewritten_key_event->NormalizeFlags();
#endif
  rewritten_event->reset(rewritten_key_event);
}

void EventRewriter::DeviceKeyPressedOrReleased(int device_id) {
  if (!device_id_to_type_.count(device_id))
    KeyboardDeviceAdded(device_id);
  last_keyboard_device_id_ = device_id;
}

const PrefService* EventRewriter::GetPrefService() const {
  if (pref_service_for_testing_)
    return pref_service_for_testing_;
  Profile* profile = ProfileManager::GetActiveUserProfile();
  return profile ? profile->GetPrefs() : NULL;
}

bool EventRewriter::IsAppleKeyboard() const {
  if (last_keyboard_device_id_ == ui::ED_UNKNOWN_DEVICE)
    return false;

  // Check which device generated |event|.
  std::map<int, DeviceType>::const_iterator iter =
      device_id_to_type_.find(last_keyboard_device_id_);
  if (iter == device_id_to_type_.end()) {
    LOG(ERROR) << "Device ID " << last_keyboard_device_id_ << " is unknown.";
    return false;
  }

  const DeviceType type = iter->second;
  return type == kDeviceAppleKeyboard;
}

bool EventRewriter::TopRowKeysAreFunctionKeys(const ui::KeyEvent& event) const {
  const PrefService* prefs = GetPrefService();
  if (prefs && prefs->FindPreference(prefs::kLanguageSendFunctionKeys) &&
      prefs->GetBoolean(prefs::kLanguageSendFunctionKeys))
    return true;

  ash::wm::WindowState* state = ash::wm::GetActiveWindowState();
  return state ? state->top_row_keys_are_function_keys() : false;
}

int EventRewriter::GetRemappedModifierMasks(const PrefService& pref_service,
                                            const ui::Event& event,
                                            int original_flags) const {
  int unmodified_flags = original_flags;
  int rewritten_flags = current_diamond_key_modifier_flags_;
  for (size_t i = 0; unmodified_flags && (i < arraysize(kModifierRemappings));
       ++i) {
    const ModifierRemapping* remapped_key = NULL;
    if (!(unmodified_flags & kModifierRemappings[i].flag))
      continue;
    switch (kModifierRemappings[i].flag) {
      case ui::EF_COMMAND_DOWN:
        // Rewrite Command key presses on an Apple keyboard to Control.
        if (IsAppleKeyboard()) {
          DCHECK_EQ(ui::EF_CONTROL_DOWN, kModifierRemappingCtrl->flag);
          remapped_key = kModifierRemappingCtrl;
        }
        break;
      case ui::EF_MOD3_DOWN:
        // If EF_MOD3_DOWN is used by the current input method, leave it alone;
        // it is not remappable.
        if (IsISOLevel5ShiftUsedByCurrentInputMethod())
          continue;
        // Otherwise, Mod3Mask is set on X events when the Caps Lock key
        // is down, but, if Caps Lock is remapped, CapsLock is NOT set,
        // because pressing the key does not invoke caps lock. So, the
        // kModifierRemappings[] table uses EF_MOD3_DOWN for the Caps
        // Lock remapping.
        break;
      default:
        break;
    }
    if (!remapped_key && kModifierRemappings[i].pref_name) {
      remapped_key =
          GetRemappedKey(kModifierRemappings[i].pref_name, pref_service);
    }
    if (remapped_key) {
      unmodified_flags &= ~kModifierRemappings[i].flag;
      rewritten_flags |= remapped_key->flag;
    }
  }
  return rewritten_flags | unmodified_flags;
}

bool EventRewriter::RewriteWithKeyboardRemappingsByKeyCode(
    const KeyboardRemapping* remappings,
    size_t num_remappings,
    const MutableKeyState& input,
    MutableKeyState* remapped_state) {
  for (size_t i = 0; i < num_remappings; ++i) {
    const KeyboardRemapping& map = remappings[i];
    if (input.key_code != map.input_key_code)
      continue;
    if ((input.flags & map.input_flags) != map.input_flags)
      continue;
    remapped_state->key_code = map.output_key_code;
    remapped_state->flags = (input.flags & ~map.input_flags) | map.output_flags;
    return true;
  }
  return false;
}

ui::EventRewriteStatus EventRewriter::RewriteKeyEvent(
    const ui::KeyEvent& key_event,
    scoped_ptr<ui::Event>* rewritten_event) {
  if (key_event.source_device_id() != ui::ED_UNKNOWN_DEVICE)
    DeviceKeyPressedOrReleased(key_event.source_device_id());
  MutableKeyState state = {key_event.flags(), key_event.key_code()};
  // Do not rewrite an event sent by ui_controls::SendKeyPress(). See
  // crbug.com/136465.
  if (!(key_event.flags() & ui::EF_FINAL)) {
    RewriteModifierKeys(key_event, &state);
    RewriteNumPadKeys(key_event, &state);
  }
  ui::EventRewriteStatus status = ui::EVENT_REWRITE_CONTINUE;
  if (sticky_keys_controller_) {
    status = sticky_keys_controller_->RewriteKeyEvent(
        key_event, state.key_code, &state.flags);
    if (status == ui::EVENT_REWRITE_DISCARD)
      return ui::EVENT_REWRITE_DISCARD;
  }
  if (!(key_event.flags() & ui::EF_FINAL)) {
    RewriteExtendedKeys(key_event, &state);
    RewriteFunctionKeys(key_event, &state);
  }
  if ((key_event.flags() == state.flags) &&
      (key_event.key_code() == state.key_code) &&
#if defined(USE_X11)
      // TODO(kpschoedel): This test is present because several consumers of
      // key events depend on having a native core X11 event, so we rewrite
      // all XI2 key events (GenericEvent) into corresponding core X11 key
      // events. Remove this when event consumers no longer care about
      // native X11 event details (crbug.com/380349).
      (!key_event.HasNativeEvent() ||
       (key_event.native_event()->type != GenericEvent)) &&
#endif
      (status == ui::EVENT_REWRITE_CONTINUE)) {
    return ui::EVENT_REWRITE_CONTINUE;
  }
  // Sticky keys may have returned a result other than |EVENT_REWRITE_CONTINUE|,
  // in which case we need to preserve that return status. Alternatively, we
  // might be here because key_event changed, in which case we need to
  // return |EVENT_REWRITE_REWRITTEN|.
  if (status == ui::EVENT_REWRITE_CONTINUE)
    status = ui::EVENT_REWRITE_REWRITTEN;
  BuildRewrittenKeyEvent(
      key_event, state.key_code, state.flags, rewritten_event);
  return status;
}

ui::EventRewriteStatus EventRewriter::RewriteMouseButtonEvent(
    const ui::MouseEvent& mouse_event,
    scoped_ptr<ui::Event>* rewritten_event) {
  int flags = mouse_event.flags();
  RewriteLocatedEvent(mouse_event, &flags);
  ui::EventRewriteStatus status = ui::EVENT_REWRITE_CONTINUE;
  if (sticky_keys_controller_)
    status = sticky_keys_controller_->RewriteMouseEvent(mouse_event, &flags);
  int changed_button = ui::EF_NONE;
  if ((mouse_event.type() == ui::ET_MOUSE_PRESSED) ||
      (mouse_event.type() == ui::ET_MOUSE_RELEASED)) {
    changed_button = RewriteModifierClick(mouse_event, &flags);
  }
  if ((mouse_event.flags() == flags) &&
      (status == ui::EVENT_REWRITE_CONTINUE)) {
    return ui::EVENT_REWRITE_CONTINUE;
  }
  if (status == ui::EVENT_REWRITE_CONTINUE)
    status = ui::EVENT_REWRITE_REWRITTEN;
  ui::MouseEvent* rewritten_mouse_event = new ui::MouseEvent(mouse_event);
  rewritten_event->reset(rewritten_mouse_event);
  rewritten_mouse_event->set_flags(flags);
#if defined(USE_X11)
  ui::UpdateX11EventForFlags(rewritten_mouse_event);
#endif
  if (changed_button != ui::EF_NONE) {
    rewritten_mouse_event->set_changed_button_flags(changed_button);
#if defined(USE_X11)
    ui::UpdateX11EventForChangedButtonFlags(rewritten_mouse_event);
#endif
  }
  return status;
}

ui::EventRewriteStatus EventRewriter::RewriteMouseWheelEvent(
    const ui::MouseWheelEvent& wheel_event,
    scoped_ptr<ui::Event>* rewritten_event) {
  if (!sticky_keys_controller_)
    return ui::EVENT_REWRITE_CONTINUE;
  int flags = wheel_event.flags();
  ui::EventRewriteStatus status =
      sticky_keys_controller_->RewriteMouseEvent(wheel_event, &flags);
  if ((wheel_event.flags() == flags) &&
      (status == ui::EVENT_REWRITE_CONTINUE)) {
    return ui::EVENT_REWRITE_CONTINUE;
  }
  if (status == ui::EVENT_REWRITE_CONTINUE)
    status = ui::EVENT_REWRITE_REWRITTEN;
  ui::MouseWheelEvent* rewritten_wheel_event =
      new ui::MouseWheelEvent(wheel_event);
  rewritten_event->reset(rewritten_wheel_event);
  rewritten_wheel_event->set_flags(flags);
#if defined(USE_X11)
  ui::UpdateX11EventForFlags(rewritten_wheel_event);
#endif
  return status;
}

ui::EventRewriteStatus EventRewriter::RewriteTouchEvent(
    const ui::TouchEvent& touch_event,
    scoped_ptr<ui::Event>* rewritten_event) {
  int flags = touch_event.flags();
  RewriteLocatedEvent(touch_event, &flags);
  if (touch_event.flags() == flags)
    return ui::EVENT_REWRITE_CONTINUE;
  ui::TouchEvent* rewritten_touch_event = new ui::TouchEvent(touch_event);
  rewritten_event->reset(rewritten_touch_event);
  rewritten_touch_event->set_flags(flags);
#if defined(USE_X11)
  ui::UpdateX11EventForFlags(rewritten_touch_event);
#endif
  return ui::EVENT_REWRITE_REWRITTEN;
}

ui::EventRewriteStatus EventRewriter::RewriteScrollEvent(
    const ui::ScrollEvent& scroll_event,
    scoped_ptr<ui::Event>* rewritten_event) {
  int flags = scroll_event.flags();
  ui::EventRewriteStatus status = ui::EVENT_REWRITE_CONTINUE;
  if (sticky_keys_controller_)
    status = sticky_keys_controller_->RewriteScrollEvent(scroll_event, &flags);
  if (status == ui::EVENT_REWRITE_CONTINUE)
    return status;
  ui::ScrollEvent* rewritten_scroll_event = new ui::ScrollEvent(scroll_event);
  rewritten_event->reset(rewritten_scroll_event);
  rewritten_scroll_event->set_flags(flags);
#if defined(USE_X11)
  ui::UpdateX11EventForFlags(rewritten_scroll_event);
#endif
  return status;
}

void EventRewriter::RewriteModifierKeys(const ui::KeyEvent& key_event,
                                        MutableKeyState* state) {
  DCHECK(key_event.type() == ui::ET_KEY_PRESSED ||
         key_event.type() == ui::ET_KEY_RELEASED);

  // Do nothing if we have just logged in as guest but have not restarted chrome
  // process yet (so we are still on the login screen). In this situations we
  // have no user profile so can not do anything useful.
  // Note that currently, unlike other accounts, when user logs in as guest, we
  // restart chrome process. In future this is to be changed.
  // TODO(glotov): remove the following condition when we do not restart chrome
  // when user logs in as guest.
  // TODO(kpschoedel): check whether this is still necessary.
  if (UserManager::Get()->IsLoggedInAsGuest() &&
      LoginDisplayHostImpl::default_host())
    return;

  const PrefService* pref_service = GetPrefService();
  if (!pref_service)
    return;

  MutableKeyState incoming = *state;
  state->flags = ui::EF_NONE;
  int characteristic_flag = ui::EF_NONE;

  // First, remap the key code.
  const ModifierRemapping* remapped_key = NULL;
  switch (incoming.key_code) {
    // On Chrome OS, F15 (XF86XK_Launch6) with NumLock (Mod2Mask) is sent
    // when Diamond key is pressed.
    case ui::VKEY_F15:
      // When diamond key is not available, the configuration UI for Diamond
      // key is not shown. Therefore, ignore the kLanguageRemapDiamondKeyTo
      // syncable pref.
      if (HasDiamondKey())
        remapped_key =
            GetRemappedKey(prefs::kLanguageRemapDiamondKeyTo, *pref_service);
      // Default behavior of F15 is Control, even if --has-chromeos-diamond-key
      // is absent, according to unit test comments.
      if (!remapped_key) {
        DCHECK_EQ(ui::VKEY_CONTROL, kModifierRemappingCtrl->key_code);
        remapped_key = kModifierRemappingCtrl;
      }
      // F15 is not a modifier key, so we need to track its state directly.
      if (key_event.type() == ui::ET_KEY_PRESSED) {
        int remapped_flag = remapped_key->flag;
        if (remapped_key->remap_to == input_method::kCapsLockKey)
          remapped_flag |= ui::EF_CAPS_LOCK_DOWN;
        current_diamond_key_modifier_flags_ = remapped_flag;
      } else {
        current_diamond_key_modifier_flags_ = ui::EF_NONE;
      }
      break;
    // On Chrome OS, XF86XK_Launch7 (F16) with Mod3Mask is sent when Caps Lock
    // is pressed (with one exception: when
    // IsISOLevel5ShiftUsedByCurrentInputMethod() is true, the key generates
    // XK_ISO_Level3_Shift with Mod3Mask, not XF86XK_Launch7).
    case ui::VKEY_F16:
      characteristic_flag = ui::EF_CAPS_LOCK_DOWN;
      remapped_key =
          GetRemappedKey(prefs::kLanguageRemapCapsLockKeyTo, *pref_service);
      break;
    case ui::VKEY_LWIN:
    case ui::VKEY_RWIN:
      characteristic_flag = ui::EF_COMMAND_DOWN;
      // Rewrite Command-L/R key presses on an Apple keyboard to Control.
      if (IsAppleKeyboard()) {
        DCHECK_EQ(ui::VKEY_CONTROL, kModifierRemappingCtrl->key_code);
        remapped_key = kModifierRemappingCtrl;
      } else {
        remapped_key =
            GetRemappedKey(prefs::kLanguageRemapSearchKeyTo, *pref_service);
      }
      // Default behavior is Super key, hence don't remap the event if the pref
      // is unavailable.
      break;
    case ui::VKEY_CONTROL:
      characteristic_flag = ui::EF_CONTROL_DOWN;
      remapped_key =
          GetRemappedKey(prefs::kLanguageRemapControlKeyTo, *pref_service);
      break;
    case ui::VKEY_MENU:
      // ALT key
      characteristic_flag = ui::EF_ALT_DOWN;
      remapped_key =
          GetRemappedKey(prefs::kLanguageRemapAltKeyTo, *pref_service);
      break;
    default:
      break;
  }

  if (remapped_key) {
    state->key_code = remapped_key->key_code;
    incoming.flags |= characteristic_flag;
    characteristic_flag = remapped_key->flag;
  }

  // Next, remap modifier bits.
  state->flags |=
      GetRemappedModifierMasks(*pref_service, key_event, incoming.flags);
  if (key_event.type() == ui::ET_KEY_PRESSED)
    state->flags |= characteristic_flag;
  else
    state->flags &= ~characteristic_flag;

  // Toggle Caps Lock if the remapped key is ui::VKEY_CAPITAL, but do nothing if
  // the original key is ui::VKEY_CAPITAL (i.e. a Caps Lock key on an external
  // keyboard is pressed) since X can handle that case.
  if (key_event.type() == ui::ET_KEY_PRESSED &&
      incoming.key_code != ui::VKEY_CAPITAL &&
      state->key_code == ui::VKEY_CAPITAL) {
    chromeos::input_method::ImeKeyboard* ime_keyboard =
        ime_keyboard_for_testing_
            ? ime_keyboard_for_testing_
            : chromeos::input_method::InputMethodManager::Get()
                  ->GetImeKeyboard();
    ime_keyboard->SetCapsLockEnabled(!ime_keyboard->CapsLockIsEnabled());
  }
}

void EventRewriter::RewriteNumPadKeys(const ui::KeyEvent& key_event,
                                      MutableKeyState* state) {
  DCHECK(key_event.type() == ui::ET_KEY_PRESSED ||
         key_event.type() == ui::ET_KEY_RELEASED);
  if (!(state->flags & ui::EF_NUMPAD_KEY))
    return;
  MutableKeyState incoming = *state;

  static const KeyboardRemapping kNumPadRemappings[] = {
      {ui::VKEY_INSERT, ui::EF_NUMPAD_KEY, ui::VKEY_NUMPAD0, ui::EF_NUMPAD_KEY},
      {ui::VKEY_DELETE, ui::EF_NUMPAD_KEY, ui::VKEY_DECIMAL, ui::EF_NUMPAD_KEY},
      {ui::VKEY_END, ui::EF_NUMPAD_KEY, ui::VKEY_NUMPAD1, ui::EF_NUMPAD_KEY},
      {ui::VKEY_DOWN, ui::EF_NUMPAD_KEY, ui::VKEY_NUMPAD2, ui::EF_NUMPAD_KEY},
      {ui::VKEY_NEXT, ui::EF_NUMPAD_KEY, ui::VKEY_NUMPAD3, ui::EF_NUMPAD_KEY},
      {ui::VKEY_LEFT, ui::EF_NUMPAD_KEY, ui::VKEY_NUMPAD4, ui::EF_NUMPAD_KEY},
      {ui::VKEY_CLEAR, ui::EF_NUMPAD_KEY, ui::VKEY_NUMPAD5, ui::EF_NUMPAD_KEY},
      {ui::VKEY_RIGHT, ui::EF_NUMPAD_KEY, ui::VKEY_NUMPAD6, ui::EF_NUMPAD_KEY},
      {ui::VKEY_HOME, ui::EF_NUMPAD_KEY, ui::VKEY_NUMPAD7, ui::EF_NUMPAD_KEY},
      {ui::VKEY_UP, ui::EF_NUMPAD_KEY, ui::VKEY_NUMPAD8, ui::EF_NUMPAD_KEY},
      {ui::VKEY_PRIOR, ui::EF_NUMPAD_KEY, ui::VKEY_NUMPAD9, ui::EF_NUMPAD_KEY},
  };

  RewriteWithKeyboardRemappingsByKeyCode(
      kNumPadRemappings, arraysize(kNumPadRemappings), incoming, state);
}

void EventRewriter::RewriteExtendedKeys(const ui::KeyEvent& key_event,
                                        MutableKeyState* state) {
  DCHECK(key_event.type() == ui::ET_KEY_PRESSED ||
         key_event.type() == ui::ET_KEY_RELEASED);

  MutableKeyState incoming = *state;
  bool rewritten = false;

  if ((incoming.flags & (ui::EF_COMMAND_DOWN | ui::EF_ALT_DOWN)) ==
      (ui::EF_COMMAND_DOWN | ui::EF_ALT_DOWN)) {
    // Allow Search to avoid rewriting extended keys.
    static const KeyboardRemapping kAvoidRemappings[] = {
        {  // Alt+Backspace
         ui::VKEY_BACK, ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN, ui::VKEY_BACK,
         ui::EF_ALT_DOWN,
        },
        {  // Control+Alt+Up
         ui::VKEY_UP,
         ui::EF_ALT_DOWN | ui::EF_CONTROL_DOWN | ui::EF_COMMAND_DOWN,
         ui::VKEY_UP, ui::EF_ALT_DOWN | ui::EF_CONTROL_DOWN,
        },
        {  // Alt+Up
         ui::VKEY_UP, ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN, ui::VKEY_UP,
         ui::EF_ALT_DOWN,
        },
        {  // Control+Alt+Down
         ui::VKEY_DOWN,
         ui::EF_ALT_DOWN | ui::EF_CONTROL_DOWN | ui::EF_COMMAND_DOWN,
         ui::VKEY_DOWN, ui::EF_ALT_DOWN | ui::EF_CONTROL_DOWN,
        },
        {  // Alt+Down
         ui::VKEY_DOWN, ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN, ui::VKEY_DOWN,
         ui::EF_ALT_DOWN,
        }};

    rewritten = RewriteWithKeyboardRemappingsByKeyCode(
        kAvoidRemappings, arraysize(kAvoidRemappings), incoming, state);
  }

  if (!rewritten && (incoming.flags & ui::EF_COMMAND_DOWN)) {
    static const KeyboardRemapping kSearchRemappings[] = {
        {  // Search+BackSpace -> Delete
         ui::VKEY_BACK, ui::EF_COMMAND_DOWN, ui::VKEY_DELETE, 0},
        {  // Search+Left -> Home
         ui::VKEY_LEFT, ui::EF_COMMAND_DOWN, ui::VKEY_HOME, 0},
        {  // Search+Up -> Prior (aka PageUp)
         ui::VKEY_UP, ui::EF_COMMAND_DOWN, ui::VKEY_PRIOR, 0},
        {  // Search+Right -> End
         ui::VKEY_RIGHT, ui::EF_COMMAND_DOWN, ui::VKEY_END, 0},
        {  // Search+Down -> Next (aka PageDown)
         ui::VKEY_DOWN, ui::EF_COMMAND_DOWN, ui::VKEY_NEXT, 0},
        {  // Search+Period -> Insert
         ui::VKEY_OEM_PERIOD, ui::EF_COMMAND_DOWN, ui::VKEY_INSERT, 0}};

    rewritten = RewriteWithKeyboardRemappingsByKeyCode(
        kSearchRemappings, arraysize(kSearchRemappings), incoming, state);
  }

  if (!rewritten && (incoming.flags & ui::EF_ALT_DOWN)) {
    static const KeyboardRemapping kNonSearchRemappings[] = {
        {  // Alt+BackSpace -> Delete
         ui::VKEY_BACK, ui::EF_ALT_DOWN, ui::VKEY_DELETE, 0},
        {  // Control+Alt+Up -> Home
         ui::VKEY_UP, ui::EF_ALT_DOWN | ui::EF_CONTROL_DOWN, ui::VKEY_HOME, 0},
        {  // Alt+Up -> Prior (aka PageUp)
         ui::VKEY_UP, ui::EF_ALT_DOWN, ui::VKEY_PRIOR, 0},
        {  // Control+Alt+Down -> End
         ui::VKEY_DOWN, ui::EF_ALT_DOWN | ui::EF_CONTROL_DOWN, ui::VKEY_END, 0},
        {  // Alt+Down -> Next (aka PageDown)
         ui::VKEY_DOWN, ui::EF_ALT_DOWN, ui::VKEY_NEXT, 0}};

    rewritten = RewriteWithKeyboardRemappingsByKeyCode(
        kNonSearchRemappings, arraysize(kNonSearchRemappings), incoming, state);
  }
}

void EventRewriter::RewriteFunctionKeys(const ui::KeyEvent& key_event,
                                        MutableKeyState* state) {
  CHECK(key_event.type() == ui::ET_KEY_PRESSED ||
        key_event.type() == ui::ET_KEY_RELEASED);
  MutableKeyState incoming = *state;
  bool rewritten = false;

  if ((incoming.key_code >= ui::VKEY_F1) &&
      (incoming.key_code <= ui::VKEY_F24)) {
    // By default the top row (F1-F12) keys are system keys for back, forward,
    // brightness, volume, etc. However, windows for v2 apps can optionally
    // request raw function keys for these keys.
    bool top_row_keys_are_function_keys = TopRowKeysAreFunctionKeys(key_event);
    bool search_is_pressed = (incoming.flags & ui::EF_COMMAND_DOWN) != 0;

    //  Search? Top Row   Result
    //  ------- --------  ------
    //  No      Fn        Unchanged
    //  No      System    Fn -> System
    //  Yes     Fn        Fn -> System
    //  Yes     System    Search+Fn -> Fn
    if (top_row_keys_are_function_keys == search_is_pressed) {
      // Rewrite the F1-F12 keys on a Chromebook keyboard to system keys.
      static const KeyboardRemapping kFkeysToSystemKeys[] = {
          {ui::VKEY_F1, 0, ui::VKEY_BROWSER_BACK, 0},
          {ui::VKEY_F2, 0, ui::VKEY_BROWSER_FORWARD, 0},
          {ui::VKEY_F3, 0, ui::VKEY_BROWSER_REFRESH, 0},
          {ui::VKEY_F4, 0, ui::VKEY_MEDIA_LAUNCH_APP2, 0},
          {ui::VKEY_F5, 0, ui::VKEY_MEDIA_LAUNCH_APP1, 0},
          {ui::VKEY_F6, 0, ui::VKEY_BRIGHTNESS_DOWN, 0},
          {ui::VKEY_F7, 0, ui::VKEY_BRIGHTNESS_UP, 0},
          {ui::VKEY_F8, 0, ui::VKEY_VOLUME_MUTE, 0},
          {ui::VKEY_F9, 0, ui::VKEY_VOLUME_DOWN, 0},
          {ui::VKEY_F10, 0, ui::VKEY_VOLUME_UP, 0},
      };
      MutableKeyState incoming_without_command = incoming;
      incoming_without_command.flags &= ~ui::EF_COMMAND_DOWN;
      rewritten =
          RewriteWithKeyboardRemappingsByKeyCode(kFkeysToSystemKeys,
                                                 arraysize(kFkeysToSystemKeys),
                                                 incoming_without_command,
                                                 state);
    } else if (search_is_pressed) {
      // Allow Search to avoid rewriting F1-F12.
      state->flags &= ~ui::EF_COMMAND_DOWN;
      rewritten = true;
    }
  }

  if (!rewritten && (incoming.flags & ui::EF_COMMAND_DOWN)) {
    // Remap Search+<number> to F<number>.
    // We check the keycode here instead of the keysym, as these keys have
    // different keysyms when modifiers are pressed, such as shift.

    // TODO(danakj): On some i18n keyboards, these choices will be bad and we
    // should make layout-specific choices here. For eg. on a french keyboard
    // "-" and "6" are the same key, so F11 will not be accessible.
    static const KeyboardRemapping kNumberKeysToFkeys[] = {
        {ui::VKEY_1, ui::EF_COMMAND_DOWN, ui::VKEY_F1, 0},
        {ui::VKEY_2, ui::EF_COMMAND_DOWN, ui::VKEY_F2, 0},
        {ui::VKEY_3, ui::EF_COMMAND_DOWN, ui::VKEY_F3, 0},
        {ui::VKEY_4, ui::EF_COMMAND_DOWN, ui::VKEY_F4, 0},
        {ui::VKEY_5, ui::EF_COMMAND_DOWN, ui::VKEY_F5, 0},
        {ui::VKEY_6, ui::EF_COMMAND_DOWN, ui::VKEY_F6, 0},
        {ui::VKEY_7, ui::EF_COMMAND_DOWN, ui::VKEY_F7, 0},
        {ui::VKEY_8, ui::EF_COMMAND_DOWN, ui::VKEY_F8, 0},
        {ui::VKEY_9, ui::EF_COMMAND_DOWN, ui::VKEY_F9, 0},
        {ui::VKEY_0, ui::EF_COMMAND_DOWN, ui::VKEY_F10, 0},
        {ui::VKEY_OEM_MINUS, ui::EF_COMMAND_DOWN, ui::VKEY_F11, 0},
        {ui::VKEY_OEM_PLUS, ui::EF_COMMAND_DOWN, ui::VKEY_F12, 0}};
    rewritten = RewriteWithKeyboardRemappingsByKeyCode(
        kNumberKeysToFkeys, arraysize(kNumberKeysToFkeys), incoming, state);
  }
}

void EventRewriter::RewriteLocatedEvent(const ui::Event& event,
                                        int* flags) {
  const PrefService* pref_service = GetPrefService();
  if (!pref_service)
    return;
  *flags = GetRemappedModifierMasks(*pref_service, event, *flags);
}

int EventRewriter::RewriteModifierClick(const ui::MouseEvent& mouse_event,
                                        int* flags) {
  // Remap Alt+Button1 to Button3.
  const int kAltLeftButton = (ui::EF_ALT_DOWN | ui::EF_LEFT_MOUSE_BUTTON);
  if (((*flags & kAltLeftButton) == kAltLeftButton) &&
      ((mouse_event.type() == ui::ET_MOUSE_PRESSED) ||
       pressed_device_ids_.count(mouse_event.source_device_id()))) {
    *flags &= ~kAltLeftButton;
    *flags |= ui::EF_RIGHT_MOUSE_BUTTON;
    if (mouse_event.type() == ui::ET_MOUSE_PRESSED)
      pressed_device_ids_.insert(mouse_event.source_device_id());
    else
      pressed_device_ids_.erase(mouse_event.source_device_id());
    return ui::EF_RIGHT_MOUSE_BUTTON;
  }
  return ui::EF_NONE;
}

EventRewriter::DeviceType EventRewriter::KeyboardDeviceAddedInternal(
    int device_id,
    const std::string& device_name) {
  const DeviceType type = GetDeviceType(device_name);
  if (type == kDeviceAppleKeyboard) {
    VLOG(1) << "Apple keyboard '" << device_name << "' connected: "
            << "id=" << device_id;
  }
  // Always overwrite the existing device_id since the X server may reuse a
  // device id for an unattached device.
  device_id_to_type_[device_id] = type;
  return type;
}

void EventRewriter::KeyboardDeviceAdded(int device_id) {
#if defined(USE_X11)
  DCHECK_NE(XIAllDevices, device_id);
  DCHECK_NE(XIAllMasterDevices, device_id);
  if (device_id == XIAllDevices || device_id == XIAllMasterDevices) {
    LOG(ERROR) << "Unexpected device_id passed: " << device_id;
    return;
  }

  int ndevices_return = 0;
  XIDeviceInfo* device_info =
      XIQueryDevice(gfx::GetXDisplay(), device_id, &ndevices_return);

  // Since |device_id| is neither XIAllDevices nor XIAllMasterDevices,
  // the number of devices found should be either 0 (not found) or 1.
  if (!device_info) {
    LOG(ERROR) << "XIQueryDevice: Device ID " << device_id << " is unknown.";
    return;
  }

  DCHECK_EQ(1, ndevices_return);
  for (int i = 0; i < ndevices_return; ++i) {
    DCHECK_EQ(device_id, device_info[i].deviceid);  // see the comment above.
    DCHECK(device_info[i].name);
    KeyboardDeviceAddedInternal(device_info[i].deviceid, device_info[i].name);
  }

  XIFreeDeviceInfo(device_info);
#else
  KeyboardDeviceAddedInternal(device_id, "keyboard");
#endif
}

}  // namespace chromeos
