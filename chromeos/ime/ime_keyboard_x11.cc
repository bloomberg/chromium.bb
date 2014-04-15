// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ime/ime_keyboard.h"

#include <cstdlib>
#include <cstring>
#include <queue>
#include <set>
#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/process/kill.h"
#include "base/process/launch.h"
#include "base/process/process_handle.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/sys_info.h"
#include "base/threading/thread_checker.h"
#include "ui/gfx/x/x11_types.h"

// These includes conflict with base/tracked_objects.h so must come last.
#include <X11/XKBlib.h>
#include <X11/Xlib.h>

namespace chromeos {
namespace input_method {
namespace {

// The delay in milliseconds that we'll wait between checking if
// setxkbmap command finished.
const int kSetLayoutCommandCheckDelayMs = 100;

// The command we use to set the current XKB layout and modifier key mapping.
// TODO(yusukes): Use libxkbfile.so instead of the command (crosbug.com/13105)
const char kSetxkbmapCommand[] = "/usr/bin/setxkbmap";

// A string for obtaining a mask value for Num Lock.
const char kNumLockVirtualModifierString[] = "NumLock";

const char *kISOLevel5ShiftLayoutIds[] = {
  "ca(multix)",
  "de(neo)",
};

const char *kAltGrLayoutIds[] = {
  "be",
  "be",
  "be",
  "bg",
  "bg(phonetic)",
  "br",
  "ca",
  "ca(eng)",
  "ca(multix)",
  "ch",
  "ch(fr)",
  "cz",
  "de",
  "de(neo)",
  "dk",
  "ee",
  "es",
  "es(cat)",
  "fi",
  "fr",
  "gb(dvorak)",
  "gb(extd)",
  "gr",
  "hr",
  "il",
  "it",
  "latam",
  "lt",
  "no",
  "pl",
  "pt",
  "ro",
  "se",
  "si",
  "sk",
  "tr",
  "ua",
  "us(altgr-intl)",
  "us(intl)",
};


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

class ImeKeyboardX11 : public ImeKeyboard {
 public:
  ImeKeyboardX11();
  virtual ~ImeKeyboardX11() {}

  // Adds/removes observer.
  virtual void AddObserver(Observer* observer) OVERRIDE;
  virtual void RemoveObserver(Observer* observer) OVERRIDE;

  // ImeKeyboard:
  virtual bool SetCurrentKeyboardLayoutByName(
      const std::string& layout_name) OVERRIDE;
  virtual bool ReapplyCurrentKeyboardLayout() OVERRIDE;
  virtual void ReapplyCurrentModifierLockStatus() OVERRIDE;
  virtual void DisableNumLock() OVERRIDE;
  virtual void SetCapsLockEnabled(bool enable_caps_lock) OVERRIDE;
  virtual bool CapsLockIsEnabled() OVERRIDE;
  virtual bool IsISOLevel5ShiftAvailable() const OVERRIDE;
  virtual bool IsAltGrAvailable() const OVERRIDE;
  virtual bool SetAutoRepeatEnabled(bool enabled) OVERRIDE;
  virtual bool SetAutoRepeatRate(const AutoRepeatRate& rate) OVERRIDE;

 private:
  // Returns a mask for Num Lock (e.g. 1U << 4). Returns 0 on error.
  unsigned int GetNumLockMask();

  // Sets the caps-lock status. Note that calling this function always disables
  // the num-lock.
  void SetLockedModifiers(bool caps_lock_enabled);

  // This function is used by SetLayout() and RemapModifierKeys(). Calls
  // setxkbmap command if needed, and updates the last_full_layout_name_ cache.
  bool SetLayoutInternal(const std::string& layout_name, bool force);

  // Executes 'setxkbmap -layout ...' command asynchronously using a layout name
  // in the |execute_queue_|. Do nothing if the queue is empty.
  // TODO(yusukes): Use libxkbfile.so instead of the command (crosbug.com/13105)
  void MaybeExecuteSetLayoutCommand();

  // Polls to see setxkbmap process exits.
  void PollUntilChildFinish(const base::ProcessHandle handle);

  // Called when execve'd setxkbmap process exits.
  void OnSetLayoutFinish();

  const bool is_running_on_chrome_os_;
  unsigned int num_lock_mask_;

  // The current Caps Lock status. If true, enabled.
  bool current_caps_lock_status_;

  // The XKB layout name which we set last time like "us" and "us(dvorak)".
  std::string current_layout_name_;

  // A queue for executing setxkbmap one by one.
  std::queue<std::string> execute_queue_;

  base::ThreadChecker thread_checker_;

  base::WeakPtrFactory<ImeKeyboardX11> weak_factory_;

  ObserverList<Observer> observers_;

  DISALLOW_COPY_AND_ASSIGN(ImeKeyboardX11);
};

ImeKeyboardX11::ImeKeyboardX11()
    : is_running_on_chrome_os_(base::SysInfo::IsRunningOnChromeOS()),
      weak_factory_(this) {
  // X must be already initialized.
  CHECK(gfx::GetXDisplay());

  num_lock_mask_ = GetNumLockMask();

  if (is_running_on_chrome_os_) {
    // Some code seems to assume that Mod2Mask is always assigned to
    // Num Lock.
    //
    // TODO(yusukes): Check the assumption is really okay. If not,
    // modify the Aura code, and then remove the CHECK below.
    LOG_IF(ERROR, num_lock_mask_ != Mod2Mask)
        << "NumLock is not assigned to Mod2Mask.  : " << num_lock_mask_;
  }

  current_caps_lock_status_ = CapsLockIsEnabled();
}

void ImeKeyboardX11::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void ImeKeyboardX11::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

unsigned int ImeKeyboardX11::GetNumLockMask() {
  DCHECK(thread_checker_.CalledOnValidThread());
  static const unsigned int kBadMask = 0;

  unsigned int real_mask = kBadMask;
  XkbDescPtr xkb_desc =
      XkbGetKeyboard(gfx::GetXDisplay(), XkbAllComponentsMask, XkbUseCoreKbd);
  if (!xkb_desc)
    return kBadMask;

  if (xkb_desc->dpy && xkb_desc->names) {
    const std::string string_to_find(kNumLockVirtualModifierString);
    for (size_t i = 0; i < XkbNumVirtualMods; ++i) {
      const unsigned int virtual_mod_mask = 1U << i;
      char* virtual_mod_str_raw_ptr =
          XGetAtomName(xkb_desc->dpy, xkb_desc->names->vmods[i]);
      if (!virtual_mod_str_raw_ptr)
        continue;
      const std::string virtual_mod_str = virtual_mod_str_raw_ptr;
      XFree(virtual_mod_str_raw_ptr);

      if (string_to_find == virtual_mod_str) {
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

void ImeKeyboardX11::SetLockedModifiers(bool caps_lock_enabled) {
  DCHECK(thread_checker_.CalledOnValidThread());

  // Always turn off num lock.
  unsigned int affect_mask = num_lock_mask_;
  unsigned int value_mask = 0;

  affect_mask |= LockMask;
  value_mask |= (caps_lock_enabled ? LockMask : 0);
  current_caps_lock_status_ = caps_lock_enabled;

  XkbLockModifiers(gfx::GetXDisplay(), XkbUseCoreKbd, affect_mask, value_mask);
}

bool ImeKeyboardX11::SetLayoutInternal(const std::string& layout_name,
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
void ImeKeyboardX11::MaybeExecuteSetLayoutCommand() {
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

  PollUntilChildFinish(handle);

  DVLOG(1) << "ExecuteSetLayoutCommand: " << layout_to_set
           << ": pid=" << base::GetProcId(handle);
}

// Delay and loop until child process finishes and call the callback.
void ImeKeyboardX11::PollUntilChildFinish(const base::ProcessHandle handle) {
  int exit_code;
  DVLOG(1) << "PollUntilChildFinish: poll for pid=" << base::GetProcId(handle);
  switch (base::GetTerminationStatus(handle, &exit_code)) {
    case base::TERMINATION_STATUS_STILL_RUNNING:
      DVLOG(1) << "PollUntilChildFinish: Try waiting again";
      base::MessageLoop::current()->PostDelayedTask(
          FROM_HERE,
          base::Bind(&ImeKeyboardX11::PollUntilChildFinish,
                     weak_factory_.GetWeakPtr(),
                     handle),
          base::TimeDelta::FromMilliseconds(kSetLayoutCommandCheckDelayMs));
      return;

    case base::TERMINATION_STATUS_NORMAL_TERMINATION:
      DVLOG(1) << "PollUntilChildFinish: Child process finished";
      OnSetLayoutFinish();
      return;

    case base::TERMINATION_STATUS_ABNORMAL_TERMINATION:
      DVLOG(1) << "PollUntilChildFinish: Abnormal exit code: " << exit_code;
      OnSetLayoutFinish();
      return;

    default:
      NOTIMPLEMENTED();
      OnSetLayoutFinish();
      return;
  }
}

bool ImeKeyboardX11::CapsLockIsEnabled() {
  DCHECK(thread_checker_.CalledOnValidThread());
  XkbStateRec status;
  XkbGetState(gfx::GetXDisplay(), XkbUseCoreKbd, &status);
  return (status.locked_mods & LockMask);
}

bool ImeKeyboardX11::IsISOLevel5ShiftAvailable() const {
  for (size_t i = 0; i < arraysize(kISOLevel5ShiftLayoutIds); ++i) {
    if (current_layout_name_ == kISOLevel5ShiftLayoutIds[i])
      return true;
  }
  return false;
}

bool ImeKeyboardX11::IsAltGrAvailable() const {
  for (size_t i = 0; i < arraysize(kAltGrLayoutIds); ++i) {
    if (current_layout_name_ == kAltGrLayoutIds[i])
      return true;
  }
  return false;
}

bool ImeKeyboardX11::SetAutoRepeatEnabled(bool enabled) {
  if (enabled)
    XAutoRepeatOn(gfx::GetXDisplay());
  else
    XAutoRepeatOff(gfx::GetXDisplay());
  DVLOG(1) << "Set auto-repeat mode to: " << (enabled ? "on" : "off");
  return true;
}

bool ImeKeyboardX11::SetAutoRepeatRate(const AutoRepeatRate& rate) {
  DVLOG(1) << "Set auto-repeat rate to: "
           << rate.initial_delay_in_ms << " ms delay, "
           << rate.repeat_interval_in_ms << " ms interval";
  if (XkbSetAutoRepeatRate(gfx::GetXDisplay(), XkbUseCoreKbd,
                           rate.initial_delay_in_ms,
                           rate.repeat_interval_in_ms) != True) {
    DVLOG(1) << "Failed to set auto-repeat rate";
    return false;
  }
  return true;
}

void ImeKeyboardX11::SetCapsLockEnabled(bool enable_caps_lock) {
  bool old_state = current_caps_lock_status_;
  SetLockedModifiers(enable_caps_lock);
  if (old_state != enable_caps_lock) {
    FOR_EACH_OBSERVER(ImeKeyboard::Observer, observers_,
                      OnCapsLockChanged(enable_caps_lock));
  }
}

bool ImeKeyboardX11::SetCurrentKeyboardLayoutByName(
    const std::string& layout_name) {
  if (SetLayoutInternal(layout_name, false)) {
    current_layout_name_ = layout_name;
    return true;
  }
  return false;
}

bool ImeKeyboardX11::ReapplyCurrentKeyboardLayout() {
  if (current_layout_name_.empty()) {
    DVLOG(1) << "Can't reapply XKB layout: layout unknown";
    return false;
  }
  return SetLayoutInternal(current_layout_name_, true /* force */);
}

void ImeKeyboardX11::ReapplyCurrentModifierLockStatus() {
  SetLockedModifiers(current_caps_lock_status_);
}

void ImeKeyboardX11::DisableNumLock() {
  SetCapsLockEnabled(current_caps_lock_status_);
}

void ImeKeyboardX11::OnSetLayoutFinish() {
  if (execute_queue_.empty()) {
    DVLOG(1) << "OnSetLayoutFinish: execute_queue_ is empty. "
             << "base::LaunchProcess failed?";
    return;
  }
  execute_queue_.pop();
  MaybeExecuteSetLayoutCommand();
}

}  // namespace

// static
bool ImeKeyboard::GetAutoRepeatEnabledForTesting() {
  XKeyboardState state = {};
  XGetKeyboardControl(gfx::GetXDisplay(), &state);
  return state.global_auto_repeat != AutoRepeatModeOff;
}

// static
bool ImeKeyboard::GetAutoRepeatRateForTesting(AutoRepeatRate* out_rate) {
  return XkbGetAutoRepeatRate(gfx::GetXDisplay(),
                              XkbUseCoreKbd,
                              &(out_rate->initial_delay_in_ms),
                              &(out_rate->repeat_interval_in_ms)) == True;
}

// static
bool ImeKeyboard::CheckLayoutNameForTesting(const std::string& layout_name) {
  return CheckLayoutName(layout_name);
}

// static
ImeKeyboard* ImeKeyboard::Create() { return new ImeKeyboardX11(); }

}  // namespace input_method
}  // namespace chromeos
