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

// A string for obtaining a mask value for Num Lock.
const char kNumLockVirtualModifierString[] = "NumLock";

// Returns false if |layout_name| contains a bad character.
bool CheckLayoutName(const std::string& layout_name) {
  static const char kValidLayoutNameCharacters[] =
      "abcdefghijklmnopqrstuvwxyz0123456789()-_";

  if (layout_name.empty()) {
    DVLOG(1) << "Invalid layout_name: " << layout_name;
    return false;
  }

  if (layout_name.find_first_not_of(kValidLayoutNameCharacters) !=
      std::string::npos) {
    DVLOG(1) << "Invalid layout_name: " << layout_name;
    return false;
  }

  return true;
}

class XKeyboardImpl : public XKeyboard {
 public:
  explicit XKeyboardImpl(const InputMethodUtil& util);
  virtual ~XKeyboardImpl() {}

  // Overridden from XKeyboard:
  virtual bool SetCurrentKeyboardLayoutByName(
      const std::string& layout_name) OVERRIDE;
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

 private:
  // This function is used by SetLayout() and RemapModifierKeys(). Calls
  // setxkbmap command if needed, and updates the last_full_layout_name_ cache.
  bool SetLayoutInternal(const std::string& layout_name, bool force);

  // Executes 'setxkbmap -layout ...' command asynchronously using a layout name
  // in the |execute_queue_|. Do nothing if the queue is empty.
  // TODO(yusukes): Use libxkbfile.so instead of the command (crosbug.com/13105)
  void MaybeExecuteSetLayoutCommand();

  // Returns true if the current thread is the UI thread, or the process is
  // running on Linux.
  bool CurrentlyOnUIThread() const;

  // Called when execve'd setxkbmap process exits.
  static void OnSetLayoutFinish(pid_t pid, int status, XKeyboardImpl* self);

  const bool is_running_on_chrome_os_;
  unsigned int num_lock_mask_;

  // The current Num Lock and Caps Lock status. If true, enabled.
  bool current_num_lock_status_;
  bool current_caps_lock_status_;
  // The XKB layout name which we set last time like "us" and "us(dvorak)".
  std::string current_layout_name_;

  // A queue for executing setxkbmap one by one.
  std::queue<std::string> execute_queue_;

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
}

bool XKeyboardImpl::SetLayoutInternal(const std::string& layout_name,
                                      bool force) {
  if (!is_running_on_chrome_os_) {
    // We should not try to change a layout on Linux or inside ui_tests. Just
    // return true.
    return true;
  }

  if (!CheckLayoutName(layout_name))
    return false;

  if (!force && (current_layout_name_ == layout_name)) {
    DVLOG(1) << "The requested layout is already set: " << layout_name;
    return true;
  }

  DVLOG(1) << (force ? "Reapply" : "Set") << " layout: " << layout_name;

  const bool start_execution = execute_queue_.empty();
  // If no setxkbmap command is in flight (i.e. start_execution is true),
  // start the first one by explicitly calling MaybeExecuteSetLayoutCommand().
  // If one or more setxkbmap commands are already in flight, just push the
  // layout name to the queue. setxkbmap command for the layout will be called
  // via OnSetLayoutFinish() callback later.
  execute_queue_.push(layout_name);
  if (start_execution)
    MaybeExecuteSetLayoutCommand();

  return true;
}

// Executes 'setxkbmap -layout ...' command asynchronously using a layout name
// in the |execute_queue_|. Do nothing if the queue is empty.
// TODO(yusukes): Use libxkbfile.so instead of the command (crosbug.com/13105)
void XKeyboardImpl::MaybeExecuteSetLayoutCommand() {
  if (execute_queue_.empty())
    return;
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
  if (!xkb_desc)
    return kBadMask;

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
    if (out_caps_lock_enabled)
      *out_caps_lock_enabled = false;
    if (out_num_lock_enabled)
      *out_num_lock_enabled = false;
    return;
  }

  XkbStateRec status;
  XkbGetState(ui::GetXDisplay(), XkbUseCoreKbd, &status);
  if (out_caps_lock_enabled)
    *out_caps_lock_enabled = status.locked_mods & LockMask;
  if (out_num_lock_enabled)
    *out_num_lock_enabled = status.locked_mods & num_lock_mask_;
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

  if (affect_mask)
    XkbLockModifiers(ui::GetXDisplay(), XkbUseCoreKbd, affect_mask, value_mask);
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
  if (SetLayoutInternal(layout_name, false)) {
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
  return SetLayoutInternal(current_layout_name_, true /* force */);
}

void XKeyboardImpl::ReapplyCurrentModifierLockStatus() {
  SetLockedModifiers(current_caps_lock_status_ ? kEnableLock : kDisableLock,
                     current_num_lock_status_ ? kEnableLock : kDisableLock);
}

bool XKeyboardImpl::CurrentlyOnUIThread() const {
  // It seems that the tot Chrome (as of Mar 7 2012) does not allow browser
  // tests to call BrowserThread::CurrentlyOn(). It ends up a CHECK failure:
  //   FATAL:sequenced_worker_pool.cc
  //   Check failed: constructor_message_loop_.get().
  // For now, just allow unit/browser tests to call any functions in this class.
  // TODO(yusukes): Stop special-casing browser_tests and remove this function.
  if (!is_running_on_chrome_os_)
    return true;
  return BrowserThread::CurrentlyOn(BrowserThread::UI);
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
  if (enabled)
    XAutoRepeatOn(ui::GetXDisplay());
  else
    XAutoRepeatOff(ui::GetXDisplay());
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
bool XKeyboard::CheckLayoutNameForTesting(const std::string& layout_name) {
  return CheckLayoutName(layout_name);
}

// static
XKeyboard* XKeyboard::Create(const InputMethodUtil& util) {
  return new XKeyboardImpl(util);
}

}  // namespace input_method
}  // namespace chromeos
