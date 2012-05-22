// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/input_method/xkeyboard.h"

#include <cstdlib>
#include <cstring>
#include <queue>
#include <set>
#include <utility>

#include "base/chromeos/chromeos_version.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/process_util.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "chrome/browser/chromeos/input_method/input_method_util.h"
#include "chrome/browser/chromeos/input_method/xkeyboard_data.h"
#include "content/public/browser/browser_thread.h"
#include "ui/base/x/x11_util.h"

// These includes conflict with base/tracked_objects.h so must come last.
#include <X11/XKBlib.h>
#include <X11/Xlib.h>
#include <glib.h>

using content::BrowserThread;

namespace chromeos {
namespace input_method {
namespace {

// The default keyboard layout name in the xorg config file.
const char kDefaultLayoutName[] = "us";

// The command we use to set the current XKB layout and modifier key mapping.
// TODO(yusukes): Use libxkbfile.so instead of the command (crosbug.com/13105)
const char kSetxkbmapCommand[] = "/usr/bin/setxkbmap";

// See the comment at ModifierKey in the .h file.
ModifierKey kCustomizableKeys[] = {
  kSearchKey,
  kLeftControlKey,
  kLeftAltKey
};

// A string for obtaining a mask value for Num Lock.
const char kNumLockVirtualModifierString[] = "NumLock";

class XKeyboardImpl : public XKeyboard {
 public:
  explicit XKeyboardImpl(const InputMethodUtil& util);
  virtual ~XKeyboardImpl() {}

  // Overridden from XKeyboard:
  virtual bool SetCurrentKeyboardLayoutByName(
      const std::string& layout_name) OVERRIDE;
  virtual bool RemapModifierKeys(const ModifierMap& modifier_map) OVERRIDE;
  virtual bool ReapplyCurrentKeyboardLayout() OVERRIDE;
  virtual void ReapplyCurrentModifierLockStatus() OVERRIDE;
  virtual void SetLockedModifiers(
      ModifierLockStatus new_caps_lock_status,
      ModifierLockStatus new_num_lock_status) OVERRIDE;
  virtual void SetNumLockEnabled(bool enable_num_lock) OVERRIDE;
  virtual void SetCapsLockEnabled(bool enable_caps_lock) OVERRIDE;
  virtual bool NumLockIsEnabled() OVERRIDE;
  virtual bool CapsLockIsEnabled() OVERRIDE;
  virtual unsigned int GetNumLockMask() OVERRIDE;
  virtual void GetLockedModifiers(bool* out_caps_lock_enabled,
                                  bool* out_num_lock_enabled) OVERRIDE;
  virtual std::string CreateFullXkbLayoutName(
      const std::string& layout_name,
      const ModifierMap& modifire_map) OVERRIDE;

 private:
  // This function is used by SetLayout() and RemapModifierKeys(). Calls
  // setxkbmap command if needed, and updates the last_full_layout_name_ cache.
  bool SetLayoutInternal(const std::string& layout_name,
                         const ModifierMap& modifier_map,
                         bool force);

  // Executes 'setxkbmap -layout ...' command asynchronously using a layout name
  // in the |execute_queue_|. Do nothing if the queue is empty.
  // TODO(yusukes): Use libxkbfile.so instead of the command (crosbug.com/13105)
  void MaybeExecuteSetLayoutCommand();

  // Returns true if the XKB layout uses the right Alt key for special purposes
  // like AltGr.
  bool KeepRightAlt(const std::string& xkb_layout_name) const;

  // Returns true if the XKB layout uses the CapsLock key for special purposes.
  // For example, since US Colemak layout uses the key as back space,
  // KeepCapsLock("us(colemak)") would return true.
  bool KeepCapsLock(const std::string& xkb_layout_name) const;

  // Returns true if the current thread is the UI thread, or the process is
  // running on Linux.
  bool CurrentlyOnUIThread() const;

  // Converts |key| to a modifier key name which is used in
  // /usr/share/X11/xkb/symbols/chromeos.
  static std::string ModifierKeyToString(ModifierKey key);

  // Called when execve'd setxkbmap process exits.
  static void OnSetLayoutFinish(pid_t pid, int status, XKeyboardImpl* self);

  const bool is_running_on_chrome_os_;
  unsigned int num_lock_mask_;

  // The current Num Lock and Caps Lock status. If true, enabled.
  bool current_num_lock_status_;
  bool current_caps_lock_status_;
  // The XKB layout name which we set last time like "us" and "us(dvorak)".
  std::string current_layout_name_;
  // The mapping of modifier keys we set last time.
  ModifierMap current_modifier_map_;

  // A queue for executing setxkbmap one by one.
  std::queue<std::string> execute_queue_;

  std::set<std::string> keep_right_alt_xkb_layout_names_;
  std::set<std::string> caps_lock_remapped_xkb_layout_names_;

  DISALLOW_COPY_AND_ASSIGN(XKeyboardImpl);
};

XKeyboardImpl::XKeyboardImpl(const InputMethodUtil& util)
    : is_running_on_chrome_os_(base::chromeos::IsRunningOnChromeOS()) {
  num_lock_mask_ = GetNumLockMask();

  // web_input_event_aurax11.cc seems to assume that Mod2Mask is always assigned
  // to Num Lock.
  // TODO(yusukes): Check the assumption is really okay. If not, modify the Aura
  // code, and then remove the CHECK below.
  CHECK(!is_running_on_chrome_os_ || (num_lock_mask_ == Mod2Mask));
  GetLockedModifiers(&current_caps_lock_status_, &current_num_lock_status_);

  for (size_t i = 0; i < arraysize(kCustomizableKeys); ++i) {
    ModifierKey key = kCustomizableKeys[i];
    current_modifier_map_.push_back(ModifierKeyPair(key, key));
  }

  std::string layout;
  for (size_t i = 0; i < arraysize(kKeepRightAltInputMethods); ++i) {
    layout = util.GetKeyboardLayoutName(kKeepRightAltInputMethods[i]);
    keep_right_alt_xkb_layout_names_.insert(layout);
  }
  for (size_t i = 0; i < arraysize(kCapsLockRemapped); ++i) {
    layout = util.GetKeyboardLayoutName(kCapsLockRemapped[i]);
    caps_lock_remapped_xkb_layout_names_.insert(layout);
  }
}

bool XKeyboardImpl::SetLayoutInternal(const std::string& layout_name,
                                      const ModifierMap& modifier_map,
                                      bool force) {
  if (!is_running_on_chrome_os_) {
    // We should not try to change a layout on Linux or inside ui_tests. Just
    // return true.
    return true;
  }

  const std::string layout_to_set = CreateFullXkbLayoutName(
      layout_name, modifier_map);
  if (layout_to_set.empty()) {
    return false;
  }

  if (!current_layout_name_.empty()) {
    const std::string current_layout = CreateFullXkbLayoutName(
        current_layout_name_, current_modifier_map_);
    if (!force && (current_layout == layout_to_set)) {
      DVLOG(1) << "The requested layout is already set: " << layout_to_set;
      return true;
    }
  }

  // Turn off caps lock if there is no kCapsLockKey in the remapped keys.
  if (!ContainsModifierKeyAsReplacement(modifier_map, kCapsLockKey)) {
    SetCapsLockEnabled(false);
  }

  DVLOG(1) << (force ? "Reapply" : "Set") << " layout: " << layout_to_set;

  const bool start_execution = execute_queue_.empty();
  // If no setxkbmap command is in flight (i.e. start_execution is true),
  // start the first one by explicitly calling MaybeExecuteSetLayoutCommand().
  // If one or more setxkbmap commands are already in flight, just push the
  // layout name to the queue. setxkbmap command for the layout will be called
  // via OnSetLayoutFinish() callback later.
  execute_queue_.push(layout_to_set);
  if (start_execution) {
    MaybeExecuteSetLayoutCommand();
  }
  return true;
}

// Executes 'setxkbmap -layout ...' command asynchronously using a layout name
// in the |execute_queue_|. Do nothing if the queue is empty.
// TODO(yusukes): Use libxkbfile.so instead of the command (crosbug.com/13105)
void XKeyboardImpl::MaybeExecuteSetLayoutCommand() {
  if (execute_queue_.empty()) {
    return;
  }
  const std::string layout_to_set = execute_queue_.front();

  std::vector<std::string> argv;
  base::ProcessHandle handle = base::kNullProcessHandle;

  argv.push_back(kSetxkbmapCommand);
  argv.push_back("-layout");
  argv.push_back(layout_to_set);
  argv.push_back("-synch");

  if (!base::LaunchProcess(argv, base::LaunchOptions(), &handle)) {
    DVLOG(1) << "Failed to execute setxkbmap: " << layout_to_set;
    execute_queue_ = std::queue<std::string>();  // clear the queue.
    return;
  }

  // g_child_watch_add is necessary to prevent the process from becoming a
  // zombie.
  const base::ProcessId pid = base::GetProcId(handle);
  g_child_watch_add(pid,
                    reinterpret_cast<GChildWatchFunc>(OnSetLayoutFinish),
                    this);
  DVLOG(1) << "ExecuteSetLayoutCommand: " << layout_to_set << ": pid=" << pid;
}

bool XKeyboardImpl::NumLockIsEnabled() {
  bool num_lock_enabled = false;
  GetLockedModifiers(NULL /* Caps Lock */, &num_lock_enabled);
  return num_lock_enabled;
}

bool XKeyboardImpl::CapsLockIsEnabled() {
  bool caps_lock_enabled = false;
  GetLockedModifiers(&caps_lock_enabled, NULL /* Num Lock */);
  return caps_lock_enabled;
}

unsigned int XKeyboardImpl::GetNumLockMask() {
  CHECK(CurrentlyOnUIThread());
  static const unsigned int kBadMask = 0;

  unsigned int real_mask = kBadMask;
  XkbDescPtr xkb_desc =
      XkbGetKeyboard(ui::GetXDisplay(), XkbAllComponentsMask, XkbUseCoreKbd);
  if (!xkb_desc) {
    return kBadMask;
  }

  if (xkb_desc->dpy && xkb_desc->names && xkb_desc->names->vmods) {
    const std::string string_to_find(kNumLockVirtualModifierString);
    for (size_t i = 0; i < XkbNumVirtualMods; ++i) {
      const unsigned int virtual_mod_mask = 1U << i;
      ui::XScopedString virtual_mod_str(
          XGetAtomName(xkb_desc->dpy, xkb_desc->names->vmods[i]));
      if (!virtual_mod_str.string())
        continue;
      if (string_to_find == virtual_mod_str.string()) {
        if (!XkbVirtualModsToReal(xkb_desc, virtual_mod_mask, &real_mask)) {
          DVLOG(1) << "XkbVirtualModsToReal failed";
          real_mask = kBadMask;  // reset the return value, just in case.
        }
        break;
      }
    }
  }
  XkbFreeKeyboard(xkb_desc, 0, True /* free all components */);
  return real_mask;
}

void XKeyboardImpl::GetLockedModifiers(bool* out_caps_lock_enabled,
                                       bool* out_num_lock_enabled) {
  CHECK(CurrentlyOnUIThread());

  if (out_num_lock_enabled && !num_lock_mask_) {
    DVLOG(1) << "Cannot get locked modifiers. Num Lock mask unknown.";
    if (out_caps_lock_enabled) {
      *out_caps_lock_enabled = false;
    }
    if (out_num_lock_enabled) {
      *out_num_lock_enabled = false;
    }
    return;
  }

  XkbStateRec status;
  XkbGetState(ui::GetXDisplay(), XkbUseCoreKbd, &status);
  if (out_caps_lock_enabled) {
    *out_caps_lock_enabled = status.locked_mods & LockMask;
  }
  if (out_num_lock_enabled) {
    *out_num_lock_enabled = status.locked_mods & num_lock_mask_;
  }
}

std::string XKeyboardImpl::CreateFullXkbLayoutName(
    const std::string& layout_name,
    const ModifierMap& modifier_map) {
  static const char kValidLayoutNameCharacters[] =
      "abcdefghijklmnopqrstuvwxyz0123456789()-_";

  if (layout_name.empty()) {
    DVLOG(1) << "Invalid layout_name: " << layout_name;
    return "";
  }

  if (layout_name.find_first_not_of(kValidLayoutNameCharacters) !=
      std::string::npos) {
    DVLOG(1) << "Invalid layout_name: " << layout_name;
    return "";
  }

  std::string use_search_key_as_str;
  std::string use_left_control_key_as_str;
  std::string use_left_alt_key_as_str;

  for (size_t i = 0; i < modifier_map.size(); ++i) {
    std::string* target = NULL;
    switch (modifier_map[i].original) {
      case kSearchKey:
        target = &use_search_key_as_str;
        break;
      case kLeftControlKey:
        target = &use_left_control_key_as_str;
        break;
      case kLeftAltKey:
        target = &use_left_alt_key_as_str;
        break;
      default:
        break;
    }
    if (!target) {
      DVLOG(1) << "We don't support remaping "
               << ModifierKeyToString(modifier_map[i].original);
      return "";
    }
    if (!(target->empty())) {
      DVLOG(1) << ModifierKeyToString(modifier_map[i].original)
               << " appeared twice";
      return "";
    }
    *target = ModifierKeyToString(modifier_map[i].replacement);
  }

  if (use_search_key_as_str.empty() ||
      use_left_control_key_as_str.empty() ||
      use_left_alt_key_as_str.empty()) {
    DVLOG(1) << "Incomplete ModifierMap: size=" << modifier_map.size();
    return "";
  }

  if (KeepCapsLock(layout_name)) {
    use_search_key_as_str = ModifierKeyToString(kSearchKey);
  }

  std::string full_xkb_layout_name =
      base::StringPrintf("%s+chromeos(%s_%s_%s%s)",
                         layout_name.c_str(),
                         use_search_key_as_str.c_str(),
                         use_left_control_key_as_str.c_str(),
                         use_left_alt_key_as_str.c_str(),
                         (KeepRightAlt(layout_name) ? "_keepralt" : ""));

  return full_xkb_layout_name;
}

void XKeyboardImpl::SetLockedModifiers(ModifierLockStatus new_caps_lock_status,
                                       ModifierLockStatus new_num_lock_status) {
  CHECK(CurrentlyOnUIThread());
  if (!num_lock_mask_) {
    DVLOG(1) << "Cannot set locked modifiers. Num Lock mask unknown.";
    return;
  }

  unsigned int affect_mask = 0;
  unsigned int value_mask = 0;
  if (new_caps_lock_status != kDontChange) {
    affect_mask |= LockMask;
    value_mask |= ((new_caps_lock_status == kEnableLock) ? LockMask : 0);
    current_caps_lock_status_ = (new_caps_lock_status == kEnableLock);
  }
  if (new_num_lock_status != kDontChange) {
    affect_mask |= num_lock_mask_;
    value_mask |= ((new_num_lock_status == kEnableLock) ? num_lock_mask_ : 0);
    current_num_lock_status_ = (new_num_lock_status == kEnableLock);
  }

  if (affect_mask) {
    XkbLockModifiers(ui::GetXDisplay(), XkbUseCoreKbd, affect_mask, value_mask);
  }
}

void XKeyboardImpl::SetNumLockEnabled(bool enable_num_lock) {
  SetLockedModifiers(
      kDontChange, enable_num_lock ? kEnableLock : kDisableLock);
}

void XKeyboardImpl::SetCapsLockEnabled(bool enable_caps_lock) {
  SetLockedModifiers(
      enable_caps_lock ? kEnableLock : kDisableLock, kDontChange);
}

bool XKeyboardImpl::SetCurrentKeyboardLayoutByName(
    const std::string& layout_name) {
  if (SetLayoutInternal(layout_name, current_modifier_map_, false)) {
    current_layout_name_ = layout_name;
    return true;
  }
  return false;
}

bool XKeyboardImpl::ReapplyCurrentKeyboardLayout() {
  if (current_layout_name_.empty()) {
    DVLOG(1) << "Can't reapply XKB layout: layout unknown";
    return false;
  }
  return SetLayoutInternal(
      current_layout_name_, current_modifier_map_, true /* force */);
}

void XKeyboardImpl::ReapplyCurrentModifierLockStatus() {
  SetLockedModifiers(current_caps_lock_status_ ? kEnableLock : kDisableLock,
                     current_num_lock_status_ ? kEnableLock : kDisableLock);
}

bool XKeyboardImpl::RemapModifierKeys(const ModifierMap& modifier_map) {
  const std::string layout_name = current_layout_name_.empty() ?
      kDefaultLayoutName : current_layout_name_;
  if (SetLayoutInternal(layout_name, modifier_map, false)) {
    current_layout_name_ = layout_name;
    current_modifier_map_ = modifier_map;
    return true;
  }
  return false;
}

bool XKeyboardImpl::KeepRightAlt(const std::string& xkb_layout_name) const {
  return keep_right_alt_xkb_layout_names_.count(xkb_layout_name) > 0;
}

bool XKeyboardImpl::KeepCapsLock(const std::string& xkb_layout_name) const {
  return caps_lock_remapped_xkb_layout_names_.count(xkb_layout_name) > 0;
}

bool XKeyboardImpl::CurrentlyOnUIThread() const {
  // It seems that the tot Chrome (as of Mar 7 2012) does not allow browser
  // tests to call BrowserThread::CurrentlyOn(). It ends up a CHECK failure:
  //   FATAL:sequenced_worker_pool.cc
  //   Check failed: constructor_message_loop_.get().
  // For now, just allow unit/browser tests to call any functions in this class.
  // TODO(yusukes): Stop special-casing browser_tests and remove this function.
  if (!is_running_on_chrome_os_) {
    return true;
  }
  return BrowserThread::CurrentlyOn(BrowserThread::UI);
}

// static
std::string XKeyboardImpl::ModifierKeyToString(ModifierKey key) {
  switch (key) {
    case kSearchKey:
      return "search";
    case kLeftControlKey:
      return "leftcontrol";
    case kLeftAltKey:
      return "leftalt";
    case kVoidKey:
      return "disabled";
    case kCapsLockKey:
      return "capslock";
    case kNumModifierKeys:
      break;
  }
  return "";
}

// static
void XKeyboardImpl::OnSetLayoutFinish(pid_t pid,
                                      int status,
                                      XKeyboardImpl* self) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DVLOG(1) << "OnSetLayoutFinish: pid=" << pid;
  if (self->execute_queue_.empty()) {
    DVLOG(1) << "OnSetLayoutFinish: execute_queue_ is empty. "
             << "base::LaunchProcess failed? pid=" << pid;
    return;
  }
  self->execute_queue_.pop();
  self->MaybeExecuteSetLayoutCommand();
}

}  // namespace

// static
bool XKeyboard::SetAutoRepeatEnabled(bool enabled) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (enabled) {
    XAutoRepeatOn(ui::GetXDisplay());
  } else {
    XAutoRepeatOff(ui::GetXDisplay());
  }
  DVLOG(1) << "Set auto-repeat mode to: " << (enabled ? "on" : "off");
  return true;
}

// static
bool XKeyboard::SetAutoRepeatRate(const AutoRepeatRate& rate) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DVLOG(1) << "Set auto-repeat rate to: "
           << rate.initial_delay_in_ms << " ms delay, "
           << rate.repeat_interval_in_ms << " ms interval";
  if (XkbSetAutoRepeatRate(ui::GetXDisplay(), XkbUseCoreKbd,
                           rate.initial_delay_in_ms,
                           rate.repeat_interval_in_ms) != True) {
    DVLOG(1) << "Failed to set auto-repeat rate";
    return false;
  }
  return true;
}

// static
bool XKeyboard::GetAutoRepeatEnabledForTesting() {
  XKeyboardState state = {};
  XGetKeyboardControl(ui::GetXDisplay(), &state);
  return state.global_auto_repeat != AutoRepeatModeOff;
}

// static
bool XKeyboard::GetAutoRepeatRateForTesting(AutoRepeatRate* out_rate) {
  return XkbGetAutoRepeatRate(ui::GetXDisplay(), XkbUseCoreKbd,
                              &(out_rate->initial_delay_in_ms),
                              &(out_rate->repeat_interval_in_ms)) == True;
}

// static
bool XKeyboard::ContainsModifierKeyAsReplacement(
    const ModifierMap& modifier_map,
    ModifierKey key) {
  for (size_t i = 0; i < modifier_map.size(); ++i) {
    if (modifier_map[i].replacement == key) {
      return true;
    }
  }
  return false;
}

// static
XKeyboard* XKeyboard::Create(const InputMethodUtil& util) {
  return new XKeyboardImpl(util);
}

}  // namespace input_method
}  // namespace chromeos
