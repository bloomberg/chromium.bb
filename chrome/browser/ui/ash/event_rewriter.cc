// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/event_rewriter.h"

#include <vector>

#include "ash/shell.h"
#include "base/logging.h"
#include "base/prefs/pref_service.h"
#include "base/string_util.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "ui/aura/root_window.h"
#include "ui/base/events/event.h"
#include "ui/base/events/event_utils.h"
#include "ui/base/keycodes/keyboard_code_conversion.h"

#if defined(OS_CHROMEOS)
#include <X11/extensions/XInput2.h>
#include <X11/keysym.h>
#include <X11/XF86keysym.h>
#include <X11/Xlib.h>

// Get rid of a macro from Xlib.h that conflicts with OwnershipService class.
#undef Status

#include "base/chromeos/chromeos_version.h"
#include "base/command_line.h"
#include "chrome/browser/chromeos/input_method/input_method_configuration.h"
#include "chrome/browser/chromeos/input_method/input_method_manager.h"
#include "chrome/browser/chromeos/login/base_login_display_host.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/chromeos/xinput_hierarchy_changed_event_listener.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chromeos/ime/xkeyboard.h"
#include "ui/base/keycodes/keyboard_code_conversion_x.h"
#include "ui/base/x/x11_util.h"

using chromeos::input_method::GetInputMethodManager;
#endif

namespace {

const int kBadDeviceId = -1;

#if defined(OS_CHROMEOS)
const char kNeo2LayoutId[] = "xkb:de:neo:ger";

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
const ModifierRemapping* kModifierRemappingCapsLock = &kModifierRemappings[4];

// A structure for converting |native_modifier| to a pair of |flag| and
// |pref_name|.
const struct ModifierFlagToPrefName {
  unsigned int native_modifier;
  int flag;
  const char* pref_name;
} kModifierFlagToPrefName[] = {
  // TODO(yusukes): When the device has a Chrome keyboard (i.e. the one without
  // Caps Lock), we should not check kLanguageRemapCapsLockKeyTo.
  { Mod3Mask, 0, prefs::kLanguageRemapCapsLockKeyTo },
  { Mod4Mask, 0, prefs::kLanguageRemapSearchKeyTo },
  { ControlMask, ui::EF_CONTROL_DOWN, prefs::kLanguageRemapControlKeyTo },
  { Mod1Mask, ui::EF_ALT_DOWN, prefs::kLanguageRemapAltKeyTo },
  { Mod2Mask, 0, prefs::kLanguageRemapDiamondKeyTo },
};

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

bool HasChromeOSKeyboard() {
  return CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kHasChromeOSKeyboard);
}

bool HasDiamondKey() {
  return CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kHasChromeOSDiamondKey);
}

bool IsMod3UsedByCurrentInputMethod() {
  // Since both German Neo2 XKB layout and Caps Lock depend on Mod3Mask,
  // it's not possible to make both features work. For now, we don't remap
  // Mod3Mask when Neo2 is in use.
  // TODO(yusukes): Remove the restriction.
  return GetInputMethodManager()->GetCurrentInputMethod().id() == kNeo2LayoutId;
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
  keysym_to_keycode_map_.clear();
}

KeyCode EventRewriter::NativeKeySymToNativeKeycode(KeySym keysym) {
  if (keysym_to_keycode_map_.count(keysym))
    return keysym_to_keycode_map_[keysym];

  Display* display = ui::GetXDisplay();
  KeyCode keycode = XKeysymToKeycode(display, keysym);
  keysym_to_keycode_map_[keysym] = keycode;
  return keycode;
}

bool EventRewriter::RewriteWithKeyboardRemappingsByKeySym(
    const KeyboardRemapping* remappings,
    size_t num_remappings,
    KeySym keysym,
    unsigned int native_mods,
    unsigned int mods,
    KeySym* remapped_native_keysym,
    unsigned int* remapped_native_mods,
    ui::KeyboardCode* remapped_keycode,
    unsigned int* remapped_mods) {
  for (size_t i = 0; i < num_remappings; ++i) {
    const KeyboardRemapping& map = remappings[i];

    if (keysym != map.input_keysym)
      continue;
    unsigned int matched_mods = native_mods & map.input_native_mods;
    if (matched_mods != map.input_native_mods)
      continue;

    *remapped_native_keysym = map.output_keysym;
    *remapped_keycode = map.output_keycode;
    *remapped_native_mods = (native_mods & ~map.input_native_mods) |
                            map.output_native_mods;
    *remapped_mods = (mods & ~map.input_mods) | map.output_mods;
    return true;
  }

  return false;
}

bool EventRewriter::RewriteWithKeyboardRemappingsByKeyCode(
    const KeyboardRemapping* remappings,
    size_t num_remappings,
    KeyCode keycode,
    unsigned int native_mods,
    unsigned int mods,
    KeySym* remapped_native_keysym,
    unsigned int* remapped_native_mods,
    ui::KeyboardCode* remapped_keycode,
    unsigned int* remapped_mods) {
  for (size_t i = 0; i < num_remappings; ++i) {
    const KeyboardRemapping& map = remappings[i];

    KeyCode input_keycode = NativeKeySymToNativeKeycode(map.input_keysym);
    if (keycode != input_keycode)
      continue;
    unsigned int matched_mods = native_mods & map.input_native_mods;
    if (matched_mods != map.input_native_mods)
      continue;

    *remapped_native_keysym = map.output_keysym;
    *remapped_keycode = map.output_keycode;
    *remapped_native_mods = (native_mods & ~map.input_native_mods) |
                            map.output_native_mods;
    *remapped_mods = (mods & ~map.input_mods) | map.output_mods;
    return true;
  }

  return false;
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
  RewriteExtendedKeys(event);
  RewriteFunctionKeys(event);
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

  // When a diamond key is not available, a Mod2Mask should not treated as a
  // configurable modifier because Mod2Mask may be worked as NumLock mask.
  // (cf. http://crbug.com/173956)
  const bool skip_mod2 = !HasDiamondKey();
  // When a Chrome OS keyboard is available, the configuration UI for Caps Lock
  // is not shown. Therefore, ignore the kLanguageRemapCapsLockKeyTo syncable
  // pref. If Mod3 is in use, don't check the pref either.
  const bool skip_mod3 =
    HasChromeOSKeyboard() || IsMod3UsedByCurrentInputMethod();

  for (size_t i = 0; i < arraysize(kModifierFlagToPrefName); ++i) {
    if ((skip_mod2 && kModifierFlagToPrefName[i].native_modifier == Mod2Mask) ||
        (skip_mod3 && kModifierFlagToPrefName[i].native_modifier == Mod3Mask)) {
      continue;
    }
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

  unsigned int native_mask = Mod4Mask | ControlMask | Mod1Mask;
  if (!skip_mod2)
    native_mask |= Mod2Mask;
  if (!skip_mod3)
    native_mask |= Mod3Mask;
  *remapped_native_modifiers =
      (original_native_modifiers & ~native_mask) |
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
  const ModifierRemapping* remapped_key = NULL;
  switch (keysym) {
    // On Chrome OS, XF86XK_Launch6 (F15) with Mod2Mask is sent when Diamond
    // key is pressed.
    case XF86XK_Launch6:
      // When diamond key is not available, the configuration UI for Diamond
      // key is not shown. Therefore, ignore the kLanguageRemapDiamondKeyTo
      // syncable pref.
      if (HasDiamondKey())
        remapped_key =
            GetRemappedKey(prefs::kLanguageRemapDiamondKeyTo, *pref_service);
      // Default behavior is Ctrl key.
      if (!remapped_key)
        remapped_key = kModifierRemappingCtrl;
      break;
    // On Chrome OS, XF86XK_Launch7 (F16) with Mod3Mask is sent when Caps Lock
    // is pressed (with one exception: when IsMod3UsedByCurrentInputMethod() is
    // true, the key generates XK_ISO_Level3_Shift with Mod3Mask, not
    // XF86XK_Launch7).
    case XF86XK_Launch7:
      // When a Chrome OS keyboard is available, the configuration UI for Caps
      // Lock is not shown. Therefore, ignore the kLanguageRemapCapsLockKeyTo
      // syncable pref.
      if (!HasChromeOSKeyboard())
        remapped_key =
            GetRemappedKey(prefs::kLanguageRemapCapsLockKeyTo, *pref_service);
      // Default behavior is Caps Lock key.
      if (!remapped_key)
        remapped_key = kModifierRemappingCapsLock;
      break;
    case XK_Super_L:
    case XK_Super_R:
      // Rewrite Command-L/R key presses on an Apple keyboard to Control-L/R.
      if (IsAppleKeyboard())
        remapped_key = kModifierRemappingCtrl;
      else
        remapped_key =
            GetRemappedKey(prefs::kLanguageRemapSearchKeyTo, *pref_service);
      // Default behavior is Super key, hence don't remap the event if the pref
      // is unavailable.
      break;
    case XK_Control_L:
    case XK_Control_R:
      remapped_key =
          GetRemappedKey(prefs::kLanguageRemapControlKeyTo, *pref_service);
      break;
    case XK_Alt_L:
    case XK_Alt_R:
    case XK_Meta_L:
    case XK_Meta_R:
      remapped_key =
          GetRemappedKey(prefs::kLanguageRemapAltKeyTo, *pref_service);
      break;
    default:
      break;
  }

  if (remapped_key) {
    remapped_keycode = remapped_key->keycode;
    const size_t level = (event->IsShiftDown() ? (1 << 1) : 0) +
        (IsRight(keysym) ? (1 << 0) : 0);
    const KeySym native_keysym = remapped_key->native_keysyms[level];
    remapped_native_keycode = NativeKeySymToNativeKeycode(native_keysym);
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
    chromeos::input_method::XKeyboard* xkeyboard =
        xkeyboard_ ? xkeyboard_ : GetInputMethodManager()->GetXKeyboard();
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
      OverwriteEvent(event, NativeKeySymToNativeKeycode(XK_KP_0),
                     xkey->state | Mod2Mask,
                     ui::VKEY_NUMPAD0, event->flags());
      rewritten = true;
      break;
    case XK_KP_Delete:
      OverwriteEvent(event, NativeKeySymToNativeKeycode(XK_KP_Decimal),
                     xkey->state | Mod2Mask,
                     ui::VKEY_DECIMAL, event->flags());
      rewritten = true;
      break;
    case XK_KP_End:
      OverwriteEvent(event, NativeKeySymToNativeKeycode(XK_KP_1),
                     xkey->state | Mod2Mask,
                     ui::VKEY_NUMPAD1, event->flags());
      rewritten = true;
      break;
    case XK_KP_Down:
      OverwriteEvent(event, NativeKeySymToNativeKeycode(XK_KP_2),
                     xkey->state | Mod2Mask,
                     ui::VKEY_NUMPAD2, event->flags());
      rewritten = true;
      break;
    case XK_KP_Next:
      OverwriteEvent(event, NativeKeySymToNativeKeycode(XK_KP_3),
                     xkey->state | Mod2Mask,
                     ui::VKEY_NUMPAD3, event->flags());
      rewritten = true;
      break;
    case XK_KP_Left:
      OverwriteEvent(event, NativeKeySymToNativeKeycode(XK_KP_4),
                     xkey->state | Mod2Mask,
                     ui::VKEY_NUMPAD4, event->flags());
      rewritten = true;
      break;
    case XK_KP_Begin:
      OverwriteEvent(event, NativeKeySymToNativeKeycode(XK_KP_5),
                     xkey->state | Mod2Mask,
                     ui::VKEY_NUMPAD5, event->flags());
      rewritten = true;
      break;
    case XK_KP_Right:
      OverwriteEvent(event, NativeKeySymToNativeKeycode(XK_KP_6),
                     xkey->state | Mod2Mask,
                     ui::VKEY_NUMPAD6, event->flags());
      rewritten = true;
      break;
    case XK_KP_Home:
      OverwriteEvent(event, NativeKeySymToNativeKeycode(XK_KP_7),
                     xkey->state | Mod2Mask,
                     ui::VKEY_NUMPAD7, event->flags());
      rewritten = true;
      break;
    case XK_KP_Up:
      OverwriteEvent(event, NativeKeySymToNativeKeycode(XK_KP_8),
                     xkey->state | Mod2Mask,
                     ui::VKEY_NUMPAD8, event->flags());
      rewritten = true;
      break;
    case XK_KP_Prior:
      OverwriteEvent(event, NativeKeySymToNativeKeycode(XK_KP_9),
                     xkey->state | Mod2Mask,
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

bool EventRewriter::RewriteExtendedKeys(ui::KeyEvent* event) {
#if defined(OS_CHROMEOS)
  XEvent* xev = event->native_event();
  XKeyEvent* xkey = &(xev->xkey);
  const KeySym keysym = XLookupKeysym(xkey, 0);

  KeySym remapped_native_keysym = 0;
  unsigned int remapped_native_mods = 0;
  ui::KeyboardCode remapped_keycode = ui::VKEY_UNKNOWN;
  unsigned int remapped_mods = 0;

  if (xkey->state & Mod4Mask) {
    // Allow Search to avoid rewriting extended keys.
    static const KeyboardRemapping kAvoidRemappings[] = {
      { // Alt+Backspace
        XK_BackSpace,
        ui::EF_ALT_DOWN, Mod1Mask | Mod4Mask,
        XK_BackSpace, ui::VKEY_BACK,
        ui::EF_ALT_DOWN, Mod1Mask,
      },
      { // Control+Alt+Up
        XK_Up,
        ui::EF_ALT_DOWN | ui::EF_CONTROL_DOWN,
        Mod1Mask | ControlMask | Mod4Mask,
        XK_Up, ui::VKEY_UP,
        ui::EF_ALT_DOWN | ui::EF_CONTROL_DOWN, Mod1Mask | ControlMask,
      },
      { // Alt+Up
        XK_Up,
        ui::EF_ALT_DOWN, Mod1Mask | Mod4Mask,
        XK_Up, ui::VKEY_UP,
        ui::EF_ALT_DOWN, Mod1Mask,
      },
      { // Control+Alt+Down
        XK_Down,
        ui::EF_ALT_DOWN | ui::EF_CONTROL_DOWN,
        Mod1Mask | ControlMask | Mod4Mask,
        XK_Down, ui::VKEY_DOWN,
        ui::EF_ALT_DOWN | ui::EF_CONTROL_DOWN, Mod1Mask | ControlMask,
      },
      { // Alt+Down
        XK_Down,
        ui::EF_ALT_DOWN, Mod1Mask | Mod4Mask,
        XK_Down, ui::VKEY_DOWN,
        ui::EF_ALT_DOWN, Mod1Mask,
      }
    };

    RewriteWithKeyboardRemappingsByKeySym(kAvoidRemappings,
                                          arraysize(kAvoidRemappings),
                                          keysym,
                                          xkey->state,
                                          event->flags(),
                                          &remapped_native_keysym,
                                          &remapped_native_mods,
                                          &remapped_keycode,
                                          &remapped_mods);
  }

  if (remapped_keycode == ui::VKEY_UNKNOWN) {
    static const KeyboardRemapping kSearchRemappings[] = {
      { // Search+BackSpace -> Delete
        XK_BackSpace,
        0, Mod4Mask,
        XK_Delete, ui::VKEY_DELETE,
        0, 0
      },
      { // Search+Left -> Home
        XK_Left,
        0, Mod4Mask,
        XK_Home, ui::VKEY_HOME,
        0, 0
      },
      { // Search+Up -> Prior (aka PageUp)
        XK_Up,
        0, Mod4Mask,
        XK_Prior, ui::VKEY_PRIOR,
        0, 0
      },
      { // Search+Right -> End
        XK_Right,
        0, Mod4Mask,
        XK_End, ui::VKEY_END,
        0, 0
      },
      { // Search+Down -> Next (aka PageDown)
        XK_Down,
        0, Mod4Mask,
        XK_Next, ui::VKEY_NEXT,
        0, 0
      },
      { // Search+Period -> Insert
        XK_period,
        0, Mod4Mask,
        XK_Insert, ui::VKEY_INSERT,
        0, 0
      }
    };

    RewriteWithKeyboardRemappingsByKeySym(kSearchRemappings,
                                          arraysize(kSearchRemappings),
                                          keysym,
                                          xkey->state,
                                          event->flags(),
                                          &remapped_native_keysym,
                                          &remapped_native_mods,
                                          &remapped_keycode,
                                          &remapped_mods);
  }

  if (remapped_keycode == ui::VKEY_UNKNOWN) {
    static const KeyboardRemapping kNonSearchRemappings[] = {
      { // Alt+BackSpace -> Delete
        XK_BackSpace,
        ui::EF_ALT_DOWN, Mod1Mask,
        XK_Delete, ui::VKEY_DELETE,
        0, 0
      },
      { // Control+Alt+Up -> Home
        XK_Up,
        ui::EF_ALT_DOWN | ui::EF_CONTROL_DOWN, Mod1Mask | ControlMask,
        XK_Home, ui::VKEY_HOME,
        0, 0
      },
      { // Alt+Up -> Prior (aka PageUp)
        XK_Up,
        ui::EF_ALT_DOWN, Mod1Mask,
        XK_Prior, ui::VKEY_PRIOR,
        0, 0
      },
      { // Control+Alt+Down -> End
        XK_Down,
        ui::EF_ALT_DOWN | ui::EF_CONTROL_DOWN, Mod1Mask | ControlMask,
        XK_End, ui::VKEY_END,
        0, 0
      },
      { // Alt+Down -> Next (aka PageDown)
        XK_Down,
        ui::EF_ALT_DOWN, Mod1Mask,
        XK_Next, ui::VKEY_NEXT,
        0, 0
      }
    };

    RewriteWithKeyboardRemappingsByKeySym(kNonSearchRemappings,
                                          arraysize(kNonSearchRemappings),
                                          keysym,
                                          xkey->state,
                                          event->flags(),
                                          &remapped_native_keysym,
                                          &remapped_native_mods,
                                          &remapped_keycode,
                                          &remapped_mods);
  }

  if (remapped_keycode == ui::VKEY_UNKNOWN)
    return false;

  OverwriteEvent(event,
                 NativeKeySymToNativeKeycode(remapped_native_keysym),
                 remapped_native_mods,
                 remapped_keycode,
                 remapped_mods);
  return true;
#else
  // TODO(yusukes): Support Ash on other platforms if needed.
  return false;
#endif
}

bool EventRewriter::RewriteFunctionKeys(ui::KeyEvent* event) {
#if defined(OS_CHROMEOS)
  XEvent* xev = event->native_event();
  XKeyEvent* xkey = &(xev->xkey);
  const KeySym keysym = XLookupKeysym(xkey, 0);

  KeySym remapped_native_keysym = 0;
  unsigned int remapped_native_mods = 0;
  ui::KeyboardCode remapped_keycode = ui::VKEY_UNKNOWN;
  unsigned int remapped_mods = 0;

  if (xkey->state & Mod4Mask) {
    // Allow Search to avoid rewriting F1-F12.
    static const KeyboardRemapping kFkeysToFkeys[] = {
      { XK_F1, 0, Mod4Mask, XK_F1, ui::VKEY_F1, },
      { XK_F2, 0, Mod4Mask, XK_F2, ui::VKEY_F2, },
      { XK_F3, 0, Mod4Mask, XK_F3, ui::VKEY_F3, },
      { XK_F4, 0, Mod4Mask, XK_F4, ui::VKEY_F4, },
      { XK_F5, 0, Mod4Mask, XK_F5, ui::VKEY_F5, },
      { XK_F6, 0, Mod4Mask, XK_F6, ui::VKEY_F6, },
      { XK_F7, 0, Mod4Mask, XK_F7, ui::VKEY_F7, },
      { XK_F8, 0, Mod4Mask, XK_F8, ui::VKEY_F8, },
      { XK_F9, 0, Mod4Mask, XK_F9, ui::VKEY_F9, },
      { XK_F10, 0, Mod4Mask, XK_F10, ui::VKEY_F10, },
      { XK_F11, 0, Mod4Mask, XK_F11, ui::VKEY_F11, },
      { XK_F12, 0, Mod4Mask, XK_F12, ui::VKEY_F12, },
    };

    RewriteWithKeyboardRemappingsByKeySym(kFkeysToFkeys,
                                          arraysize(kFkeysToFkeys),
                                          keysym,
                                          xkey->state,
                                          event->flags(),
                                          &remapped_native_keysym,
                                          &remapped_native_mods,
                                          &remapped_keycode,
                                          &remapped_mods);
  }

  if (remapped_keycode == ui::VKEY_UNKNOWN) {
    // Rewrite the actual F1-F12 keys on a Chromebook keyboard to special keys.
    static const KeyboardRemapping kFkeysToSpecialKeys[] = {
      { XK_F1, 0, 0, XF86XK_Back, ui::VKEY_BROWSER_BACK, 0, 0 },
      { XK_F2, 0, 0, XF86XK_Forward, ui::VKEY_BROWSER_FORWARD, 0, 0 },
      { XK_F3, 0, 0, XF86XK_Reload, ui::VKEY_BROWSER_REFRESH, 0, 0 },
      { XK_F4, 0, 0, XF86XK_LaunchB, ui::VKEY_MEDIA_LAUNCH_APP2, 0, 0 },
      { XK_F5, 0, 0, XF86XK_LaunchA, ui::VKEY_MEDIA_LAUNCH_APP1, 0, 0 },
      { XK_F6, 0, 0, XF86XK_MonBrightnessDown, ui::VKEY_BRIGHTNESS_DOWN, 0, 0 },
      { XK_F7, 0, 0, XF86XK_MonBrightnessUp, ui::VKEY_BRIGHTNESS_UP, 0, 0 },
      { XK_F8, 0, 0, XF86XK_AudioMute, ui::VKEY_VOLUME_MUTE, 0, 0 },
      { XK_F9, 0, 0, XF86XK_AudioLowerVolume, ui::VKEY_VOLUME_DOWN, 0, 0 },
      { XK_F10, 0, 0, XF86XK_AudioRaiseVolume, ui::VKEY_VOLUME_UP, 0, 0 },
    };

    RewriteWithKeyboardRemappingsByKeySym(kFkeysToSpecialKeys,
                                          arraysize(kFkeysToSpecialKeys),
                                          keysym,
                                          xkey->state,
                                          event->flags(),
                                          &remapped_native_keysym,
                                          &remapped_native_mods,
                                          &remapped_keycode,
                                          &remapped_mods);
  }

  if (remapped_keycode == ui::VKEY_UNKNOWN && xkey->state & Mod4Mask) {
    // Remap Search+<number> to F<number>.
    // We check the keycode here instead of the keysym, as these keys have
    // different keysyms when modifiers are pressed, such as shift.

    // TODO(danakj): On some i18n keyboards, these choices will be bad and we
    // should make layout-specific choices here. For eg. on a french keyboard
    // "-" and "6" are the same key, so F11 will not be accessible.
    static const KeyboardRemapping kNumberKeysToFkeys[] = {
      { XK_1, 0, Mod4Mask, XK_F1, ui::VKEY_F1, 0, 0 },
      { XK_2, 0, Mod4Mask, XK_F2, ui::VKEY_F2, 0, 0 },
      { XK_3, 0, Mod4Mask, XK_F3, ui::VKEY_F3, 0, 0 },
      { XK_4, 0, Mod4Mask, XK_F4, ui::VKEY_F4, 0, 0 },
      { XK_5, 0, Mod4Mask, XK_F5, ui::VKEY_F5, 0, 0 },
      { XK_6, 0, Mod4Mask, XK_F6, ui::VKEY_F6, 0, 0 },
      { XK_7, 0, Mod4Mask, XK_F7, ui::VKEY_F7, 0, 0 },
      { XK_8, 0, Mod4Mask, XK_F8, ui::VKEY_F8, 0, 0 },
      { XK_9, 0, Mod4Mask, XK_F9, ui::VKEY_F9, 0, 0 },
      { XK_0, 0, Mod4Mask, XK_F10, ui::VKEY_F10, 0, 0 },
      { XK_minus, 0, Mod4Mask, XK_F11, ui::VKEY_F11, 0, 0 },
      { XK_equal, 0, Mod4Mask, XK_F12, ui::VKEY_F12, 0, 0 }
    };

    RewriteWithKeyboardRemappingsByKeyCode(kNumberKeysToFkeys,
                                           arraysize(kNumberKeysToFkeys),
                                           xkey->keycode,
                                           xkey->state,
                                           event->flags(),
                                           &remapped_native_keysym,
                                           &remapped_native_mods,
                                           &remapped_keycode,
                                           &remapped_mods);
  }

  if (remapped_keycode == ui::VKEY_UNKNOWN)
    return false;

  OverwriteEvent(event,
                 NativeKeySymToNativeKeycode(remapped_native_keysym),
                 remapped_native_mods,
                 remapped_keycode,
                 remapped_mods);
  return true;
#else
  // TODO(danakj): Support Ash on other platforms if needed.
  return false;
#endif
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
