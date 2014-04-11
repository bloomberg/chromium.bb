// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/events/event_rewriter.h"

#include <X11/extensions/XInput2.h>
#include <X11/keysym.h>
#include <X11/XF86keysym.h>
#include <X11/Xlib.h>
// Get rid of macros from Xlib.h that conflicts with other parts of the code.
#undef RootWindow
#undef Status

#include <vector>

#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/prefs/pref_service.h"
#include "base/strings/string_util.h"
#include "base/sys_info.h"
#include "chrome/browser/chromeos/events/keyboard_driven_event_rewriter.h"
#include "chrome/browser/chromeos/events/xinput_hierarchy_changed_event_listener.h"
#include "chrome/browser/chromeos/login/login_display_host_impl.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/pref_names.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/ime/ime_keyboard.h"
#include "chromeos/ime/input_method_manager.h"
#include "ui/base/x/x11_util.h"
#include "ui/events/event.h"
#include "ui/events/event_utils.h"
#include "ui/events/keycodes/keyboard_code_conversion_x.h"
#include "ui/events/platform/platform_event_source.h"
#include "ui/wm/core/window_util.h"

namespace {

const int kBadDeviceId = -1;

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
  { chromeos::input_method::kEscapeKey, 0, 0U, ui::VKEY_ESCAPE,
    { XK_Escape, XK_Escape, XK_Escape, XK_Escape }},
};

const ModifierRemapping* kModifierRemappingCtrl = &kModifierRemappings[1];

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

bool HasDiamondKey() {
  return CommandLine::ForCurrentProcess()->HasSwitch(
      chromeos::switches::kHasChromeOSDiamondKey);
}

bool IsISOLevel5ShiftUsedByCurrentInputMethod() {
  // Since both German Neo2 XKB layout and Caps Lock depend on Mod3Mask,
  // it's not possible to make both features work. For now, we don't remap
  // Mod3Mask when Neo2 is in use.
  // TODO(yusukes): Remove the restriction.
  chromeos::input_method::InputMethodManager* manager =
      chromeos::input_method::InputMethodManager::Get();
  return manager->IsISOLevel5ShiftUsedByCurrentInputMethod();
}

}  // namespace

namespace chromeos {

EventRewriter::EventRewriter()
    : last_device_id_(kBadDeviceId),
      keyboard_for_testing_(NULL),
      keyboard_driven_event_rewriter_(new KeyboardDrivenEventRewriter),
      pref_service_for_testing_(NULL) {
  ui::PlatformEventSource::GetInstance()->AddPlatformEventObserver(this);
  if (base::SysInfo::IsRunningOnChromeOS()) {
    XInputHierarchyChangedEventListener::GetInstance()->AddObserver(this);
  }
  RefreshKeycodes();
}

EventRewriter::~EventRewriter() {
  ui::PlatformEventSource::GetInstance()->RemovePlatformEventObserver(this);
  if (base::SysInfo::IsRunningOnChromeOS()) {
    XInputHierarchyChangedEventListener::GetInstance()->RemoveObserver(this);
  }
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

void EventRewriter::RewriteForTesting(XEvent* event) {
  Rewrite(event);
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

void EventRewriter::WillProcessEvent(const ui::PlatformEvent& event) {
  XEvent* xevent = event;
  if (xevent->type == KeyPress || xevent->type == KeyRelease) {
    Rewrite(xevent);
  } else if (xevent->type == GenericEvent) {
    XIDeviceEvent* xievent = static_cast<XIDeviceEvent*>(xevent->xcookie.data);
    if (xievent->evtype == XI_KeyPress || xievent->evtype == XI_KeyRelease) {
      if (xievent->deviceid == xievent->sourceid)
        DeviceKeyPressedOrReleased(xievent->deviceid);
    } else {
      RewriteLocatedEvent(xevent);
    }
  } else if (xevent->type == MappingNotify) {
    if (xevent->xmapping.request == MappingModifier ||
        xevent->xmapping.request == MappingKeyboard) {
      RefreshKeycodes();
    }
  }
}

void EventRewriter::DidProcessEvent(const ui::PlatformEvent& event) {
}

void EventRewriter::DeviceHierarchyChanged() {}

void EventRewriter::DeviceAdded(int device_id) {
  DCHECK_NE(XIAllDevices, device_id);
  DCHECK_NE(XIAllMasterDevices, device_id);
  if (device_id == XIAllDevices || device_id == XIAllMasterDevices) {
    LOG(ERROR) << "Unexpected device_id passed: " << device_id;
    return;
  }

  int ndevices_return = 0;
  XIDeviceInfo* device_info = XIQueryDevice(gfx::GetXDisplay(),
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

void EventRewriter::RefreshKeycodes() {
  keysym_to_keycode_map_.clear();
}

KeyCode EventRewriter::NativeKeySymToNativeKeycode(KeySym keysym) {
  if (keysym_to_keycode_map_.count(keysym))
    return keysym_to_keycode_map_[keysym];

  XDisplay* display = gfx::GetXDisplay();
  KeyCode keycode = XKeysymToKeycode(display, keysym);
  keysym_to_keycode_map_[keysym] = keycode;
  return keycode;
}

bool EventRewriter::TopRowKeysAreFunctionKeys(XEvent* event) const {
  const PrefService* prefs = GetPrefService();
  if (prefs &&
      prefs->FindPreference(prefs::kLanguageSendFunctionKeys) &&
      prefs->GetBoolean(prefs::kLanguageSendFunctionKeys))
    return true;

  ash::wm::WindowState* state = ash::wm::GetActiveWindowState();
  return state ? state->top_row_keys_are_function_keys() : false;
}

bool EventRewriter::RewriteWithKeyboardRemappingsByKeySym(
    const KeyboardRemapping* remappings,
    size_t num_remappings,
    KeySym keysym,
    unsigned int native_mods,
    KeySym* remapped_native_keysym,
    unsigned int* remapped_native_mods) {
  for (size_t i = 0; i < num_remappings; ++i) {
    const KeyboardRemapping& map = remappings[i];

    if (keysym != map.input_keysym)
      continue;
    unsigned int matched_mods = native_mods & map.input_native_mods;
    if (matched_mods != map.input_native_mods)
      continue;

    *remapped_native_keysym = map.output_keysym;
    *remapped_native_mods = (native_mods & ~map.input_native_mods) |
                            map.output_native_mods;
    return true;
  }

  return false;
}

bool EventRewriter::RewriteWithKeyboardRemappingsByKeyCode(
    const KeyboardRemapping* remappings,
    size_t num_remappings,
    KeyCode keycode,
    unsigned int native_mods,
    KeySym* remapped_native_keysym,
    unsigned int* remapped_native_mods) {
  for (size_t i = 0; i < num_remappings; ++i) {
    const KeyboardRemapping& map = remappings[i];

    KeyCode input_keycode = NativeKeySymToNativeKeycode(map.input_keysym);
    if (keycode != input_keycode)
      continue;
    unsigned int matched_mods = native_mods & map.input_native_mods;
    if (matched_mods != map.input_native_mods)
      continue;

    *remapped_native_keysym = map.output_keysym;
    *remapped_native_mods = (native_mods & ~map.input_native_mods) |
                            map.output_native_mods;
    return true;
  }

  return false;
}

const PrefService* EventRewriter::GetPrefService() const {
  if (pref_service_for_testing_)
    return pref_service_for_testing_;
  Profile* profile = ProfileManager::GetActiveUserProfile();
  return profile ? profile->GetPrefs() : NULL;
}

void EventRewriter::Rewrite(XEvent* event) {
  // Do not rewrite an event sent by ui_controls::SendKeyPress(). See
  // crbug.com/136465.
  if (event->xkey.send_event)
    return;

  // Keyboard driven rewriting happen first. Skip further processing if event is
  // changed.
  if (keyboard_driven_event_rewriter_->RewriteIfKeyboardDrivenOnLoginScreen(
          event)) {
    return;
  }

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
    unsigned int original_native_modifiers,
    unsigned int* remapped_native_modifiers) const {
  // TODO(glotov): remove the following condition when we do not restart chrome
  // when user logs in as guest. See Rewrite() for details.
  if (UserManager::Get()->IsLoggedInAsGuest() &&
      LoginDisplayHostImpl::default_host()) {
    return;
  }

  const PrefService* pref_service = GetPrefService();
  if (!pref_service)
    return;

  // When a diamond key is not available, a Mod2Mask should not treated as a
  // configurable modifier because Mod2Mask may be worked as NumLock mask.
  // (cf. http://crbug.com/173956)
  const bool skip_mod2 = !HasDiamondKey();
  // If Mod3 is used by the current input method, don't allow the CapsLock
  // pref to remap it, or the keyboard behavior will be broken.
  const bool skip_mod3 = IsISOLevel5ShiftUsedByCurrentInputMethod();

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
        *remapped_native_modifiers |= remapped_key->native_modifier;
      } else {
        *remapped_native_modifiers |=
            kModifierFlagToPrefName[i].native_modifier;
      }
    }
  }

  unsigned int native_mask = Mod4Mask | ControlMask | Mod1Mask;
  if (!skip_mod2)
    native_mask |= Mod2Mask;
  if (!skip_mod3)
    native_mask |= Mod3Mask;
  *remapped_native_modifiers =
      (original_native_modifiers & ~native_mask) |
      *remapped_native_modifiers;
}

bool EventRewriter::RewriteModifiers(XEvent* event) {
  DCHECK(event->type == KeyPress || event->type == KeyRelease);
  // Do nothing if we have just logged in as guest but have not restarted chrome
  // process yet (so we are still on the login screen). In this situations we
  // have no user profile so can not do anything useful.
  // Note that currently, unlike other accounts, when user logs in as guest, we
  // restart chrome process. In future this is to be changed.
  // TODO(glotov): remove the following condition when we do not restart chrome
  // when user logs in as guest.
  if (UserManager::Get()->IsLoggedInAsGuest() &&
      LoginDisplayHostImpl::default_host())
    return false;

  const PrefService* pref_service = GetPrefService();
  if (!pref_service)
    return false;

  DCHECK_EQ(input_method::kControlKey, kModifierRemappingCtrl->remap_to);

  XKeyEvent* xkey = &event->xkey;
  KeySym keysym = XLookupKeysym(xkey, 0);
  ui::KeyboardCode original_keycode = ui::KeyboardCodeFromNative(event);
  ui::KeyboardCode remapped_keycode = original_keycode;
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
    // is pressed (with one exception: when
    // IsISOLevel5ShiftUsedByCurrentInputMethod() is true, the key generates
    // XK_ISO_Level3_Shift with Mod3Mask, not XF86XK_Launch7).
    case XF86XK_Launch7:
      remapped_key =
          GetRemappedKey(prefs::kLanguageRemapCapsLockKeyTo, *pref_service);
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
    int flags = ui::EventFlagsFromNative(event);
    remapped_keycode = remapped_key->keycode;
    const size_t level = ((flags & ui::EF_SHIFT_DOWN) ? (1 << 1) : 0) +
        (IsRight(keysym) ? (1 << 0) : 0);
    const KeySym native_keysym = remapped_key->native_keysyms[level];
    remapped_native_keycode = NativeKeySymToNativeKeycode(native_keysym);
  }

  // Next, remap modifier bits.
  unsigned int remapped_native_modifiers = 0U;
  GetRemappedModifierMasks(xkey->state, &remapped_native_modifiers);

  // Toggle Caps Lock if the remapped key is ui::VKEY_CAPITAL, but do nothing if
  // the original key is ui::VKEY_CAPITAL (i.e. a Caps Lock key on an external
  // keyboard is pressed) since X can handle that case.
  if (event->type == KeyPress &&
      original_keycode != ui::VKEY_CAPITAL &&
      remapped_keycode == ui::VKEY_CAPITAL) {
    input_method::ImeKeyboard* keyboard = keyboard_for_testing_ ?
        keyboard_for_testing_ :
        input_method::InputMethodManager::Get()->GetImeKeyboard();
    keyboard->SetCapsLockEnabled(!keyboard->CapsLockIsEnabled());
  }

  OverwriteEvent(event, remapped_native_keycode, remapped_native_modifiers);
  return true;
}

bool EventRewriter::RewriteNumPadKeys(XEvent* event) {
  DCHECK(event->type == KeyPress || event->type == KeyRelease);
  bool rewritten = false;
  XKeyEvent* xkey = &event->xkey;
  const KeySym keysym = XLookupKeysym(xkey, 0);
  switch (keysym) {
    case XK_KP_Insert:
      OverwriteEvent(event, NativeKeySymToNativeKeycode(XK_KP_0),
                     xkey->state | Mod2Mask);
      rewritten = true;
      break;
    case XK_KP_Delete:
      OverwriteEvent(event, NativeKeySymToNativeKeycode(XK_KP_Decimal),
                     xkey->state | Mod2Mask);
      rewritten = true;
      break;
    case XK_KP_End:
      OverwriteEvent(event, NativeKeySymToNativeKeycode(XK_KP_1),
                     xkey->state | Mod2Mask);
      rewritten = true;
      break;
    case XK_KP_Down:
      OverwriteEvent(event, NativeKeySymToNativeKeycode(XK_KP_2),
                     xkey->state | Mod2Mask);
      rewritten = true;
      break;
    case XK_KP_Next:
      OverwriteEvent(event, NativeKeySymToNativeKeycode(XK_KP_3),
                     xkey->state | Mod2Mask);
      rewritten = true;
      break;
    case XK_KP_Left:
      OverwriteEvent(event, NativeKeySymToNativeKeycode(XK_KP_4),
                     xkey->state | Mod2Mask);
      rewritten = true;
      break;
    case XK_KP_Begin:
      OverwriteEvent(event, NativeKeySymToNativeKeycode(XK_KP_5),
                     xkey->state | Mod2Mask);
      rewritten = true;
      break;
    case XK_KP_Right:
      OverwriteEvent(event, NativeKeySymToNativeKeycode(XK_KP_6),
                     xkey->state | Mod2Mask);
      rewritten = true;
      break;
    case XK_KP_Home:
      OverwriteEvent(event, NativeKeySymToNativeKeycode(XK_KP_7),
                     xkey->state | Mod2Mask);
      rewritten = true;
      break;
    case XK_KP_Up:
      OverwriteEvent(event, NativeKeySymToNativeKeycode(XK_KP_8),
                     xkey->state | Mod2Mask);
      rewritten = true;
      break;
    case XK_KP_Prior:
      OverwriteEvent(event, NativeKeySymToNativeKeycode(XK_KP_9),
                     xkey->state | Mod2Mask);
      rewritten = true;
      break;
    case XK_KP_Divide:
    case XK_KP_Multiply:
    case XK_KP_Subtract:
    case XK_KP_Add:
    case XK_KP_Enter:
      // Add Mod2Mask for consistency.
      OverwriteEvent(event, xkey->keycode, xkey->state | Mod2Mask);
      rewritten = true;
      break;
    default:
      break;
  }
  return rewritten;
}

bool EventRewriter::RewriteExtendedKeys(XEvent* event) {
  DCHECK(event->type == KeyPress || event->type == KeyRelease);
  XKeyEvent* xkey = &event->xkey;
  const KeySym keysym = XLookupKeysym(xkey, 0);

  KeySym remapped_native_keysym = 0;
  unsigned int remapped_native_mods = 0;
  bool rewritten = false;

  if (xkey->state & Mod4Mask) {
    // Allow Search to avoid rewriting extended keys.
    static const KeyboardRemapping kAvoidRemappings[] = {
      { // Alt+Backspace
        XK_BackSpace, Mod1Mask | Mod4Mask,
        XK_BackSpace, Mod1Mask,
      },
      { // Control+Alt+Up
        XK_Up, Mod1Mask | ControlMask | Mod4Mask,
        XK_Up, Mod1Mask | ControlMask,
      },
      { // Alt+Up
        XK_Up, Mod1Mask | Mod4Mask,
        XK_Up, Mod1Mask,
      },
      { // Control+Alt+Down
        XK_Down, Mod1Mask | ControlMask | Mod4Mask,
        XK_Down, Mod1Mask | ControlMask,
      },
      { // Alt+Down
        XK_Down, Mod1Mask | Mod4Mask,
        XK_Down, Mod1Mask,
      }
    };

    rewritten = RewriteWithKeyboardRemappingsByKeySym(
        kAvoidRemappings,
        arraysize(kAvoidRemappings),
        keysym,
        xkey->state,
        &remapped_native_keysym,
        &remapped_native_mods);
  }

  if (!rewritten) {
    static const KeyboardRemapping kSearchRemappings[] = {
      { // Search+BackSpace -> Delete
        XK_BackSpace, Mod4Mask,
        XK_Delete, 0
      },
      { // Search+Left -> Home
        XK_Left, Mod4Mask,
        XK_Home, 0
      },
      { // Search+Up -> Prior (aka PageUp)
        XK_Up, Mod4Mask,
        XK_Prior, 0
      },
      { // Search+Right -> End
        XK_Right, Mod4Mask,
        XK_End, 0
      },
      { // Search+Down -> Next (aka PageDown)
        XK_Down, Mod4Mask,
        XK_Next, 0
      },
      { // Search+Period -> Insert
        XK_period, Mod4Mask,
        XK_Insert, 0
      }
    };

    rewritten = RewriteWithKeyboardRemappingsByKeySym(
        kSearchRemappings,
        arraysize(kSearchRemappings),
        keysym,
        xkey->state,
        &remapped_native_keysym,
        &remapped_native_mods);
  }

  if (!rewritten) {
    static const KeyboardRemapping kNonSearchRemappings[] = {
      { // Alt+BackSpace -> Delete
        XK_BackSpace, Mod1Mask,
        XK_Delete, 0
      },
      { // Control+Alt+Up -> Home
        XK_Up, Mod1Mask | ControlMask,
        XK_Home, 0
      },
      { // Alt+Up -> Prior (aka PageUp)
        XK_Up, Mod1Mask,
        XK_Prior, 0
      },
      { // Control+Alt+Down -> End
        XK_Down, Mod1Mask | ControlMask,
        XK_End, 0
      },
      { // Alt+Down -> Next (aka PageDown)
        XK_Down, Mod1Mask,
        XK_Next, 0
      }
    };

    rewritten = RewriteWithKeyboardRemappingsByKeySym(
        kNonSearchRemappings,
        arraysize(kNonSearchRemappings),
        keysym,
        xkey->state,
        &remapped_native_keysym,
        &remapped_native_mods);
  }

  if (!rewritten)
    return false;

  OverwriteEvent(event,
                 NativeKeySymToNativeKeycode(remapped_native_keysym),
                 remapped_native_mods);
  return true;
}

bool EventRewriter::RewriteFunctionKeys(XEvent* event) {
  DCHECK(event->type == KeyPress || event->type == KeyRelease);
  XKeyEvent* xkey = &event->xkey;
  const KeySym keysym = XLookupKeysym(xkey, 0);

  KeySym remapped_native_keysym = 0;
  unsigned int remapped_native_mods = 0;
  bool rewritten = false;

  // By default the top row (F1-F12) keys are special keys for back, forward,
  // brightness, volume, etc. However, windows for v2 apps can optionally
  // request raw function keys for these keys.
  bool top_row_keys_are_special_keys = !TopRowKeysAreFunctionKeys(event);

  if ((xkey->state & Mod4Mask) && top_row_keys_are_special_keys) {
    // Allow Search to avoid rewriting F1-F12.
    static const KeyboardRemapping kFkeysToFkeys[] = {
      { XK_F1, Mod4Mask, XK_F1, },
      { XK_F2, Mod4Mask, XK_F2, },
      { XK_F3, Mod4Mask, XK_F3, },
      { XK_F4, Mod4Mask, XK_F4, },
      { XK_F5, Mod4Mask, XK_F5, },
      { XK_F6, Mod4Mask, XK_F6, },
      { XK_F7, Mod4Mask, XK_F7, },
      { XK_F8, Mod4Mask, XK_F8, },
      { XK_F9, Mod4Mask, XK_F9, },
      { XK_F10, Mod4Mask, XK_F10, },
      { XK_F11, Mod4Mask, XK_F11, },
      { XK_F12, Mod4Mask, XK_F12, },
    };

    rewritten = RewriteWithKeyboardRemappingsByKeySym(
        kFkeysToFkeys,
        arraysize(kFkeysToFkeys),
        keysym,
        xkey->state,
        &remapped_native_keysym,
        &remapped_native_mods);
  }

  if (!rewritten) {
    static const KeyboardRemapping kFkeysToSpecialKeys[] = {
      { XK_F1, 0, XF86XK_Back, 0 },
      { XK_F2, 0, XF86XK_Forward, 0 },
      { XK_F3, 0, XF86XK_Reload, 0 },
      { XK_F4, 0, XF86XK_LaunchB, 0 },
      { XK_F5, 0, XF86XK_LaunchA, 0 },
      { XK_F6, 0, XF86XK_MonBrightnessDown, 0 },
      { XK_F7, 0, XF86XK_MonBrightnessUp, 0 },
      { XK_F8, 0, XF86XK_AudioMute, 0 },
      { XK_F9, 0, XF86XK_AudioLowerVolume, 0 },
      { XK_F10, 0, XF86XK_AudioRaiseVolume, 0 },
    };

    if (top_row_keys_are_special_keys) {
      // Rewrite the F1-F12 keys on a Chromebook keyboard to special keys.
      rewritten = RewriteWithKeyboardRemappingsByKeySym(
          kFkeysToSpecialKeys,
          arraysize(kFkeysToSpecialKeys),
          keysym,
          xkey->state,
          &remapped_native_keysym,
          &remapped_native_mods);
    } else if (xkey->state & Mod4Mask) {
      // Use Search + F1-F12 for the special keys.
      rewritten = RewriteWithKeyboardRemappingsByKeySym(
          kFkeysToSpecialKeys,
          arraysize(kFkeysToSpecialKeys),
          keysym,
          xkey->state & ~Mod4Mask,
          &remapped_native_keysym,
          &remapped_native_mods);
    }
  }

  if (!rewritten && (xkey->state & Mod4Mask)) {
    // Remap Search+<number> to F<number>.
    // We check the keycode here instead of the keysym, as these keys have
    // different keysyms when modifiers are pressed, such as shift.

    // TODO(danakj): On some i18n keyboards, these choices will be bad and we
    // should make layout-specific choices here. For eg. on a french keyboard
    // "-" and "6" are the same key, so F11 will not be accessible.
    static const KeyboardRemapping kNumberKeysToFkeys[] = {
      { XK_1, Mod4Mask, XK_F1, 0 },
      { XK_2, Mod4Mask, XK_F2, 0 },
      { XK_3, Mod4Mask, XK_F3, 0 },
      { XK_4, Mod4Mask, XK_F4, 0 },
      { XK_5, Mod4Mask, XK_F5, 0 },
      { XK_6, Mod4Mask, XK_F6, 0 },
      { XK_7, Mod4Mask, XK_F7, 0 },
      { XK_8, Mod4Mask, XK_F8, 0 },
      { XK_9, Mod4Mask, XK_F9, 0 },
      { XK_0, Mod4Mask, XK_F10, 0 },
      { XK_minus, Mod4Mask, XK_F11, 0 },
      { XK_equal, Mod4Mask, XK_F12, 0 }
    };

    rewritten = RewriteWithKeyboardRemappingsByKeyCode(
        kNumberKeysToFkeys,
        arraysize(kNumberKeysToFkeys),
        xkey->keycode,
        xkey->state,
        &remapped_native_keysym,
        &remapped_native_mods);
  }

  if (!rewritten)
    return false;

  OverwriteEvent(event,
                 NativeKeySymToNativeKeycode(remapped_native_keysym),
                 remapped_native_mods);
  return true;
}

void EventRewriter::RewriteLocatedEvent(XEvent* event) {
  DCHECK_EQ(GenericEvent, event->type);
  XIDeviceEvent* xievent = static_cast<XIDeviceEvent*>(event->xcookie.data);
  if (xievent->evtype != XI_ButtonPress && xievent->evtype != XI_ButtonRelease)
    return;

  // First, remap modifier masks.
  unsigned int remapped_native_modifiers = 0U;
  GetRemappedModifierMasks(xievent->mods.effective, &remapped_native_modifiers);
  xievent->mods.effective = remapped_native_modifiers;

  // Then, remap Alt+Button1 to Button3.
  if ((xievent->evtype == XI_ButtonPress ||
       pressed_device_ids_.count(xievent->sourceid)) &&
      (xievent->mods.effective & Mod1Mask) && xievent->detail == Button1) {
    xievent->mods.effective &= ~Mod1Mask;
    xievent->detail = Button3;
    if (xievent->evtype == XI_ButtonRelease) {
      // On the release clear the left button from the existing state and the
      // mods, and set the right button.
      XISetMask(xievent->buttons.mask, Button3);
      XIClearMask(xievent->buttons.mask, Button1);
      xievent->mods.effective &= ~Button1Mask;
      pressed_device_ids_.erase(xievent->sourceid);
    } else {
      pressed_device_ids_.insert(xievent->sourceid);
    }
  }
}

void EventRewriter::OverwriteEvent(XEvent* event,
                                   unsigned int new_native_keycode,
                                   unsigned int new_native_state) {
  DCHECK(event->type == KeyPress || event->type == KeyRelease);
  XKeyEvent* xkey = &event->xkey;
  xkey->keycode = new_native_keycode;
  xkey->state = new_native_state;
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

}  // namespace chromeos
