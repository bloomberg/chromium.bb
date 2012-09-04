// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/event_rewriter.h"

#include <vector>

#include "ash/shell.h"
#include "base/logging.h"
#include "base/string_util.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "ui/aura/root_window.h"
#include "ui/base/events/event.h"
#include "ui/base/keycodes/keyboard_code_conversion.h"

#if defined(OS_CHROMEOS)
#include <X11/extensions/XInput2.h>
#include <X11/keysym.h>
#include <X11/Xlib.h>

// Get rid of a macro from Xlib.h that conflicts with OwnershipService class.
#undef Status

#include "base/chromeos/chromeos_version.h"
#include "chrome/browser/chromeos/input_method/input_method_manager.h"
#include "chrome/browser/chromeos/input_method/xkeyboard.h"
#include "chrome/browser/chromeos/login/base_login_display_host.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/chromeos/xinput_hierarchy_changed_event_listener.h"
#include "chrome/common/pref_names.h"
#include "ui/base/keycodes/keyboard_code_conversion_x.h"
#include "ui/base/x/x11_util.h"

using chromeos::input_method::InputMethodManager;
#endif

namespace {

const int kBadDeviceId = -1;

#if defined(OS_CHROMEOS)
// A key code and a flag we should use when a key is remapped to |remap_to|.
const struct ModifierRemapping {
  int remap_to;
  int flag;
  unsigned int native_modifier;
  ui::KeyboardCode keycode;
  KeySym native_keysyms[4];  // left, right, shift+left, shift+right.
} kModifierRemappings[] = {
  { chromeos::input_method::kSearchKey, 0, Mod4Mask, ui::VKEY_LWIN,
    { XK_Super_L, XK_Super_L, XK_Super_L, XK_Super_L }},
  { chromeos::input_method::kControlKey, ui::EF_CONTROL_DOWN, ControlMask,
    ui::VKEY_CONTROL,
    { XK_Control_L, XK_Control_R, XK_Control_L, XK_Control_R }},
  { chromeos::input_method::kAltKey, ui::EF_ALT_DOWN, Mod1Mask,
    ui::VKEY_MENU, { XK_Alt_L, XK_Alt_R, XK_Meta_L, XK_Meta_R }},
  { chromeos::input_method::kVoidKey, 0, 0U, ui::VKEY_UNKNOWN,
    { XK_VoidSymbol, XK_VoidSymbol, XK_VoidSymbol, XK_VoidSymbol }},
  { chromeos::input_method::kCapsLockKey, 0, 0U, ui::VKEY_CAPITAL,
    { XK_Caps_Lock, XK_Caps_Lock, XK_Caps_Lock, XK_Caps_Lock }},
};

const ModifierRemapping* kModifierRemappingCtrl = &kModifierRemappings[1];

// A structure for converting |native_modifier| to a pair of |flag| and
// |pref_name|.
const struct ModifierFlagToPrefName {
  unsigned int native_modifier;
  int flag;
  const char* pref_name;
} kModifierFlagToPrefName[] = {
  { Mod4Mask, 0, prefs::kLanguageXkbRemapSearchKeyTo },
  { ControlMask, ui::EF_CONTROL_DOWN, prefs::kLanguageXkbRemapControlKeyTo },
  { Mod1Mask, ui::EF_ALT_DOWN, prefs::kLanguageXkbRemapAltKeyTo },
};

// Gets a remapped key for |pref_name| key. For example, to find out which
// key Search is currently remapped to, call the function with
// prefs::kLanguageXkbRemapSearchKeyTo.
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

bool IsRight(KeySym native_keysym) {
  switch (native_keysym) {
    case XK_Alt_R:
    case XK_Control_R:
    case XK_Hyper_R:
    case XK_Meta_R:
    case XK_Shift_R:
    case XK_Super_R:
      return true;
    default:
      break;
  }
  return false;
}
#endif

const PrefService* GetPrefService() {
  Profile* profile = ProfileManager::GetDefaultProfile();
  if (profile)
    return profile->GetPrefs();
  return NULL;
}

}  // namespace

EventRewriter::EventRewriter()
    : last_device_id_(kBadDeviceId),
#if defined(OS_CHROMEOS)
      xkeyboard_(NULL),
#endif
      pref_service_(NULL) {
  // The ash shell isn't instantiated for our unit tests.
  if (ash::Shell::HasInstance())
    ash::Shell::GetPrimaryRootWindow()->AddRootWindowObserver(this);
#if defined(OS_CHROMEOS)
  if (base::chromeos::IsRunningOnChromeOS()) {
    chromeos::XInputHierarchyChangedEventListener::GetInstance()
        ->AddObserver(this);
  }
  RefreshKeycodes();
#endif
}

EventRewriter::~EventRewriter() {
  if (ash::Shell::HasInstance())
    ash::Shell::GetPrimaryRootWindow()->RemoveRootWindowObserver(this);
#if defined(OS_CHROMEOS)
  if (base::chromeos::IsRunningOnChromeOS()) {
    chromeos::XInputHierarchyChangedEventListener::GetInstance()
        ->RemoveObserver(this);
  }
#endif
}

EventRewriter::DeviceType EventRewriter::DeviceAddedForTesting(
    int device_id,
    const std::string& device_name) {
  return DeviceAddedInternal(device_id, device_name);
}

// static
EventRewriter::DeviceType EventRewriter::GetDeviceType(
    const std::string& device_name) {
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
      return kDeviceAppleKeyboard;
  }

  return kDeviceUnknown;
}

void EventRewriter::RewriteForTesting(ui::KeyEvent* event) {
  Rewrite(event);
}

ash::EventRewriterDelegate::Action EventRewriter::RewriteOrFilterKeyEvent(
    ui::KeyEvent* event) {
  if (event->HasNativeEvent())
    Rewrite(event);
  return ash::EventRewriterDelegate::ACTION_REWRITE_EVENT;
}

ash::EventRewriterDelegate::Action EventRewriter::RewriteOrFilterLocatedEvent(
    ui::LocatedEvent* event) {
  if (event->HasNativeEvent())
    RewriteLocatedEvent(event);
  return ash::EventRewriterDelegate::ACTION_REWRITE_EVENT;
}

void EventRewriter::OnKeyboardMappingChanged(const aura::RootWindow* root) {
#if defined(OS_CHROMEOS)
  RefreshKeycodes();
#endif
}

#if defined(OS_CHROMEOS)
void EventRewriter::DeviceAdded(int device_id) {
  DCHECK_NE(XIAllDevices, device_id);
  DCHECK_NE(XIAllMasterDevices, device_id);
  if (device_id == XIAllDevices || device_id == XIAllMasterDevices) {
    LOG(ERROR) << "Unexpected device_id passed: " << device_id;
    return;
  }

  int ndevices_return = 0;
  XIDeviceInfo* device_info = XIQueryDevice(ui::GetXDisplay(),
                                            device_id,
                                            &ndevices_return);

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
    DeviceAddedInternal(device_info[i].deviceid, device_info[i].name);
  }

  XIFreeDeviceInfo(device_info);
}

void EventRewriter::DeviceRemoved(int device_id) {
  device_id_to_type_.erase(device_id);
}

void EventRewriter::DeviceKeyPressedOrReleased(int device_id) {
  std::map<int, DeviceType>::const_iterator iter =
      device_id_to_type_.find(device_id);
  if (iter == device_id_to_type_.end()) {
    // |device_id| is unknown. This means the device was connected before
    // booting the OS. Query the name of the device and add it to the map.
    DeviceAdded(device_id);
  }

  last_device_id_ = device_id;
}

void EventRewriter::RefreshKeycodes() {
  Display* display = ui::GetXDisplay();
  control_l_xkeycode_ = XKeysymToKeycode(display, XK_Control_L);
  control_r_xkeycode_ = XKeysymToKeycode(display, XK_Control_R);
  alt_l_xkeycode_ = XKeysymToKeycode(display, XK_Alt_L);
  alt_r_xkeycode_ = XKeysymToKeycode(display, XK_Alt_R);
  meta_l_xkeycode_ = XKeysymToKeycode(display, XK_Meta_L);
  meta_r_xkeycode_ = XKeysymToKeycode(display, XK_Meta_R);
  windows_l_xkeycode_ = XKeysymToKeycode(display, XK_Super_L);
  caps_lock_xkeycode_ = XKeysymToKeycode(display, XK_Caps_Lock);
  void_symbol_xkeycode_ = XKeysymToKeycode(display, XK_VoidSymbol);
  delete_xkeycode_ = XKeysymToKeycode(display, XK_Delete);
  home_xkeycode_ = XKeysymToKeycode(display, XK_Home);
  end_xkeycode_ = XKeysymToKeycode(display, XK_End);
  prior_xkeycode_ = XKeysymToKeycode(display, XK_Prior);
  next_xkeycode_ = XKeysymToKeycode(display, XK_Next);
  kp_0_xkeycode_ = XKeysymToKeycode(display, XK_KP_0);
  kp_1_xkeycode_ = XKeysymToKeycode(display, XK_KP_1);
  kp_2_xkeycode_ = XKeysymToKeycode(display, XK_KP_2);
  kp_3_xkeycode_ = XKeysymToKeycode(display, XK_KP_3);
  kp_4_xkeycode_ = XKeysymToKeycode(display, XK_KP_4);
  kp_5_xkeycode_ = XKeysymToKeycode(display, XK_KP_5);
  kp_6_xkeycode_ = XKeysymToKeycode(display, XK_KP_6);
  kp_7_xkeycode_ = XKeysymToKeycode(display, XK_KP_7);
  kp_8_xkeycode_ = XKeysymToKeycode(display, XK_KP_8);
  kp_9_xkeycode_ = XKeysymToKeycode(display, XK_KP_9);
  kp_decimal_xkeycode_ = XKeysymToKeycode(display, XK_KP_Decimal);
}

KeyCode EventRewriter::NativeKeySymToNativeKeycode(KeySym keysym) {
  switch (keysym) {
    case XK_Control_L:
      return control_l_xkeycode_;
    case XK_Control_R:
      return control_r_xkeycode_;
    case XK_Alt_L:
      return alt_l_xkeycode_;
    case XK_Alt_R:
      return alt_r_xkeycode_;
    case XK_Meta_L:
      return meta_l_xkeycode_;
    case XK_Meta_R:
      return meta_r_xkeycode_;
    case XK_Super_L:
      return windows_l_xkeycode_;
    case XK_Caps_Lock:
      return caps_lock_xkeycode_;
    case XK_VoidSymbol:
      return void_symbol_xkeycode_;
    case XK_Delete:
      return delete_xkeycode_;
    case XK_Home:
      return home_xkeycode_;
    case XK_End:
      return end_xkeycode_;
    case XK_Prior:
      return prior_xkeycode_;
    case XK_Next:
      return next_xkeycode_;
    case XK_KP_0:
      return kp_0_xkeycode_;
    case XK_KP_1:
      return kp_1_xkeycode_;
    case XK_KP_2:
      return kp_2_xkeycode_;
    case XK_KP_3:
      return kp_3_xkeycode_;
    case XK_KP_4:
      return kp_4_xkeycode_;
    case XK_KP_5:
      return kp_5_xkeycode_;
    case XK_KP_6:
      return kp_6_xkeycode_;
    case XK_KP_7:
      return kp_7_xkeycode_;
    case XK_KP_8:
      return kp_8_xkeycode_;
    case XK_KP_9:
      return kp_9_xkeycode_;
    case XK_KP_Decimal:
      return kp_decimal_xkeycode_;
    default:
      break;
  }
  return 0U;
}
#endif

void EventRewriter::Rewrite(ui::KeyEvent* event) {
#if defined(OS_CHROMEOS)
  // Do not rewrite an event sent by ui_controls::SendKeyPress(). See
  // crbug.com/136465.
  if (event->native_event()->xkey.send_event)
    return;
#endif
  RewriteModifiers(event);
  RewriteNumPadKeys(event);
  RewriteBackspaceAndArrowKeys(event);
  // TODO(yusukes): Implement crosbug.com/27167 (allow sending function keys).
}

bool EventRewriter::IsAppleKeyboard() const {
  if (last_device_id_ == kBadDeviceId)
    return false;

  // Check which device generated |event|.
  std::map<int, DeviceType>::const_iterator iter =
      device_id_to_type_.find(last_device_id_);
  if (iter == device_id_to_type_.end()) {
    LOG(ERROR) << "Device ID " << last_device_id_ << " is unknown.";
    return false;
  }

  const DeviceType type = iter->second;
  return type == kDeviceAppleKeyboard;
}

void EventRewriter::GetRemappedModifierMasks(
    int original_flags,
    unsigned int original_native_modifiers,
    int* remapped_flags,
    unsigned int* remapped_native_modifiers) const {
#if defined(OS_CHROMEOS)
  // TODO(glotov): remove the following condition when we do not restart chrome
  // when user logs in as guest. See Rewrite() for details.
  if (chromeos::UserManager::Get()->IsLoggedInAsGuest() &&
      chromeos::BaseLoginDisplayHost::default_host()) {
    return;
  }

  const PrefService* pref_service =
      pref_service_ ? pref_service_ : GetPrefService();
  if (!pref_service)
    return;

  for (size_t i = 0; i < arraysize(kModifierFlagToPrefName); ++i) {
    if (original_native_modifiers &
        kModifierFlagToPrefName[i].native_modifier) {
      const ModifierRemapping* remapped_key =
          GetRemappedKey(kModifierFlagToPrefName[i].pref_name, *pref_service);
      // Rewrite Command-L/R key presses on an Apple keyboard to Control-L/R.
      if (IsAppleKeyboard() &&
          (kModifierFlagToPrefName[i].native_modifier == Mod4Mask)) {
        remapped_key = kModifierRemappingCtrl;
      }
      if (remapped_key) {
        *remapped_flags |= remapped_key->flag;
        *remapped_native_modifiers |= remapped_key->native_modifier;
      } else {
        *remapped_flags |= kModifierFlagToPrefName[i].flag;
        *remapped_native_modifiers |=
            kModifierFlagToPrefName[i].native_modifier;
      }
    }
  }

  *remapped_flags =
      (original_flags & ~(ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN)) |
      *remapped_flags;
  *remapped_native_modifiers =
      (original_native_modifiers & ~(Mod4Mask | ControlMask | Mod1Mask)) |
      *remapped_native_modifiers;
#endif
}

bool EventRewriter::RewriteModifiers(ui::KeyEvent* event) {
  // Do nothing if we have just logged in as guest but have not restarted chrome
  // process yet (so we are still on the login screen). In this situations we
  // have no user profile so can not do anything useful.
  // Note that currently, unlike other accounts, when user logs in as guest, we
  // restart chrome process. In future this is to be changed.
  // TODO(glotov): remove the following condition when we do not restart chrome
  // when user logs in as guest.
 #if defined(OS_CHROMEOS)
   if (chromeos::UserManager::Get()->IsLoggedInAsGuest() &&
       chromeos::BaseLoginDisplayHost::default_host())
     return false;
 #endif  // defined(OS_CHROMEOS)
  const PrefService* pref_service =
      pref_service_ ? pref_service_ : GetPrefService();
  if (!pref_service)
    return false;

#if defined(OS_CHROMEOS)
  DCHECK_EQ(chromeos::input_method::kControlKey,
            kModifierRemappingCtrl->remap_to);

  XEvent* xev = event->native_event();
  XKeyEvent* xkey = &(xev->xkey);
  KeySym keysym = XLookupKeysym(xkey, 0);

  ui::KeyboardCode remapped_keycode = event->key_code();
  KeyCode remapped_native_keycode = xkey->keycode;

  // First, remap |keysym|.
  const char* pref_name = NULL;
  switch (keysym) {
    case XK_Super_L:
    case XK_Super_R:
      pref_name = prefs::kLanguageXkbRemapSearchKeyTo;
      break;
    case XK_Control_L:
    case XK_Control_R:
      pref_name = prefs::kLanguageXkbRemapControlKeyTo;
      break;
    case XK_Alt_L:
    case XK_Alt_R:
    case XK_Meta_L:
    case XK_Meta_R:
      pref_name = prefs::kLanguageXkbRemapAltKeyTo;
      break;
    default:
      break;
  }
  if (pref_name) {
    const ModifierRemapping* remapped_key =
        GetRemappedKey(pref_name, *pref_service);
    // Rewrite Command-L/R key presses on an Apple keyboard to Control-L/R.
    if (IsAppleKeyboard() && (keysym == XK_Super_L || keysym == XK_Super_R))
      remapped_key = kModifierRemappingCtrl;
    if (remapped_key) {
      remapped_keycode = remapped_key->keycode;
      const size_t level = (event->IsShiftDown() ? (1 << 1) : 0) +
          (IsRight(keysym) ? (1 << 0) : 0);
      const KeySym native_keysym = remapped_key->native_keysyms[level];
      remapped_native_keycode = NativeKeySymToNativeKeycode(native_keysym);
    }
  }

  // Next, remap modifier bits.
  int remapped_flags = 0;
  unsigned int remapped_native_modifiers = 0U;
  GetRemappedModifierMasks(event->flags(), xkey->state,
                           &remapped_flags, &remapped_native_modifiers);

  // Toggle Caps Lock if the remapped key is ui::VKEY_CAPITAL, but do nothing if
  // the original key is ui::VKEY_CAPITAL (i.e. a Caps Lock key on an external
  // keyboard is pressed) since X can handle that case.
  if ((event->type() == ui::ET_KEY_PRESSED) &&
      (event->key_code() != ui::VKEY_CAPITAL) &&
      (remapped_keycode == ui::VKEY_CAPITAL)) {
    chromeos::input_method::XKeyboard* xkeyboard = xkeyboard_ ?
        xkeyboard_ : InputMethodManager::GetInstance()->GetXKeyboard();
    xkeyboard->SetCapsLockEnabled(!xkeyboard->CapsLockIsEnabled());
  }

  OverwriteEvent(event,
                 remapped_native_keycode, remapped_native_modifiers,
                 remapped_keycode, remapped_flags);
  return true;
#else
  // TODO(yusukes): Support Ash on other platforms if needed.
  return false;
#endif
}

bool EventRewriter::RewriteNumPadKeys(ui::KeyEvent* event) {
  bool rewritten = false;
#if defined(OS_CHROMEOS)
  XEvent* xev = event->native_event();
  XKeyEvent* xkey = &(xev->xkey);

  const KeySym keysym = XLookupKeysym(xkey, 0);
  switch (keysym) {
    case XK_KP_Insert:
      OverwriteEvent(event, kp_0_xkeycode_, xkey->state | Mod2Mask,
                     ui::VKEY_NUMPAD0, event->flags());
      rewritten = true;
      break;
    case XK_KP_Delete:
      OverwriteEvent(event, kp_decimal_xkeycode_, xkey->state | Mod2Mask,
                     ui::VKEY_DECIMAL, event->flags());
      rewritten = true;
      break;
    case XK_KP_End:
      OverwriteEvent(event, kp_1_xkeycode_, xkey->state | Mod2Mask,
                     ui::VKEY_NUMPAD1, event->flags());
      rewritten = true;
      break;
    case XK_KP_Down:
      OverwriteEvent(event, kp_2_xkeycode_, xkey->state | Mod2Mask,
                     ui::VKEY_NUMPAD2, event->flags());
      rewritten = true;
      break;
    case XK_KP_Next:
      OverwriteEvent(event, kp_3_xkeycode_, xkey->state | Mod2Mask,
                     ui::VKEY_NUMPAD3, event->flags());
      rewritten = true;
      break;
    case XK_KP_Left:
      OverwriteEvent(event, kp_4_xkeycode_, xkey->state | Mod2Mask,
                     ui::VKEY_NUMPAD4, event->flags());
      rewritten = true;
      break;
    case XK_KP_Begin:
      OverwriteEvent(event, kp_5_xkeycode_, xkey->state | Mod2Mask,
                     ui::VKEY_NUMPAD5, event->flags());
      rewritten = true;
      break;
    case XK_KP_Right:
      OverwriteEvent(event, kp_6_xkeycode_, xkey->state | Mod2Mask,
                     ui::VKEY_NUMPAD6, event->flags());
      rewritten = true;
      break;
    case XK_KP_Home:
      OverwriteEvent(event, kp_7_xkeycode_, xkey->state | Mod2Mask,
                     ui::VKEY_NUMPAD7, event->flags());
      rewritten = true;
      break;
    case XK_KP_Up:
      OverwriteEvent(event, kp_8_xkeycode_, xkey->state | Mod2Mask,
                     ui::VKEY_NUMPAD8, event->flags());
      rewritten = true;
      break;
    case XK_KP_Prior:
      OverwriteEvent(event, kp_9_xkeycode_, xkey->state | Mod2Mask,
                     ui::VKEY_NUMPAD9, event->flags());
      rewritten = true;
      break;
    case XK_KP_Divide:
    case XK_KP_Multiply:
    case XK_KP_Subtract:
    case XK_KP_Add:
    case XK_KP_Enter:
      // Add Mod2Mask for consistency.
      OverwriteEvent(event, xkey->keycode, xkey->state | Mod2Mask,
                     event->key_code(), event->flags());
      rewritten = true;
      break;
    default:
      break;
  }
#else
  // TODO(yusukes): Support Ash on other platforms if needed.
#endif
  return rewritten;
}

bool EventRewriter::RewriteBackspaceAndArrowKeys(ui::KeyEvent* event) {
  bool rewritten = false;
#if defined(OS_CHROMEOS)
  XEvent* xev = event->native_event();
  XKeyEvent* xkey = &(xev->xkey);

  const KeySym keysym = XLookupKeysym(xkey, 0);
  if (keysym == XK_BackSpace && (xkey->state & Mod1Mask)) {
    // Remap Alt+Backspace to Delete.
    OverwriteEvent(event, delete_xkeycode_, xkey->state & ~Mod1Mask,
                   ui::VKEY_DELETE, event->flags() & ~ui::EF_ALT_DOWN);
    rewritten = true;
  } else if (keysym == XK_Up &&
             (xkey->state & ControlMask) && (xkey->state & Mod1Mask)) {
    // Remap Ctrl+Alt+Up to Home.
    OverwriteEvent(event,
                   home_xkeycode_,
                   xkey->state & ~(Mod1Mask | ControlMask),
                   ui::VKEY_HOME,
                   event->flags() & ~(ui::EF_ALT_DOWN | ui::EF_CONTROL_DOWN));
    rewritten = true;
  } else if (keysym == XK_Up && (xkey->state & Mod1Mask)) {
    // Remap Alt+Up to Prior (aka PageUp).
    OverwriteEvent(event, prior_xkeycode_, xkey->state & ~Mod1Mask,
                 ui::VKEY_PRIOR, event->flags() & ~ui::EF_ALT_DOWN);
    rewritten = true;
  } else if (keysym == XK_Down &&
             (xkey->state & ControlMask) && (xkey->state & Mod1Mask)) {
    // Remap Ctrl+Alt+Down to End.
    OverwriteEvent(event,
                   end_xkeycode_,
                   xkey->state & ~(Mod1Mask | ControlMask),
                   ui::VKEY_END,
                   event->flags() & ~(ui::EF_ALT_DOWN | ui::EF_CONTROL_DOWN));
    rewritten = true;
  } else if (keysym == XK_Down && (xkey->state & Mod1Mask)) {
    // Remap Alt+Down to Next (aka PageDown).
    OverwriteEvent(event, next_xkeycode_, xkey->state & ~Mod1Mask,
                   ui::VKEY_NEXT, event->flags() & ~ui::EF_ALT_DOWN);
    rewritten = true;
  }
#else
  // TODO(yusukes): Support Ash on other platforms if needed.
#endif
  return rewritten;
}

void EventRewriter::RewriteLocatedEvent(ui::LocatedEvent* event) {
#if defined(OS_CHROMEOS)
  if (event->flags() & ui::EF_IS_SYNTHESIZED)
    return;

  XEvent* xevent = event->native_event();
  if (!xevent || xevent->type != GenericEvent)
    return;

  XIDeviceEvent* xievent = static_cast<XIDeviceEvent*>(xevent->xcookie.data);
  if (xievent->evtype != XI_ButtonPress && xievent->evtype != XI_ButtonRelease)
    return;

  // First, remap modifier masks.
  int remapped_flags = 0;
  unsigned int remapped_native_modifiers = 0U;
  GetRemappedModifierMasks(event->flags(), xievent->mods.effective,
                           &remapped_flags, &remapped_native_modifiers);
  xievent->mods.effective = remapped_native_modifiers;

  // Then, remap Alt+Button1 to Button3.
  if ((xievent->mods.effective & Mod1Mask) && xievent->detail == 1) {
    xievent->mods.effective &= ~Mod1Mask;
    xievent->detail = 3;
    if (xievent->evtype == XI_ButtonRelease) {
      // On the release clear the left button from the existing state and the
      // mods, and set the right button.
      XISetMask(xievent->buttons.mask, 3);
      XIClearMask(xievent->buttons.mask, 1);
      xievent->mods.effective &= ~Button1Mask;
    }
  }

  const int mouse_event_flags = event->flags() &
      (ui::EF_IS_DOUBLE_CLICK | ui::EF_IS_TRIPLE_CLICK | ui::EF_IS_NON_CLIENT |
       ui::EF_IS_SYNTHESIZED | ui::EF_FROM_TOUCH);
  event->set_flags(mouse_event_flags | ui::EventFlagsFromNative(xevent));
#else
  // TODO(yusukes): Support Ash on other platforms if needed.
#endif
}

void EventRewriter::OverwriteEvent(ui::KeyEvent* event,
                                   unsigned int new_native_keycode,
                                   unsigned int new_native_state,
                                   ui::KeyboardCode new_keycode,
                                   int new_flags) {
#if defined(OS_CHROMEOS)
  XEvent* xev = event->native_event();
  XKeyEvent* xkey = &(xev->xkey);
  xkey->keycode = new_native_keycode;
  xkey->state = new_native_state;
  event->set_key_code(new_keycode);
  event->set_character(ui::GetCharacterFromKeyCode(event->key_code(),
                                                   new_flags));
  event->set_flags(new_flags);
  event->NormalizeFlags();
#else
  // TODO(yusukes): Support Ash on other platforms if needed.
#endif
}

EventRewriter::DeviceType EventRewriter::DeviceAddedInternal(
    int device_id,
    const std::string& device_name) {
  const DeviceType type = EventRewriter::GetDeviceType(device_name);
  if (type == kDeviceAppleKeyboard) {
    VLOG(1) << "Apple keyboard '" << device_name << "' connected: "
            << "id=" << device_id;
  }
  // Always overwrite the existing device_id since the X server may reuse a
  // device id for an unattached device.
  device_id_to_type_[device_id] = type;
  return type;
}
