// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome_frame/ready_mode/internal/registry_ready_mode_state.h"

#include <windows.h>

#include "base/logging.h"
#include "base/process_util.h"
#include "base/string_util.h"
#include "base/time.h"
#include "base/task.h"
#include "base/win/registry.h"
#include "base/win/scoped_comptr.h"
#include "base/win/scoped_handle.h"
#include "base/win/windows_version.h"
#include "chrome/installer/util/browser_distribution.h"
#include "chrome/installer/util/google_update_constants.h"
#include "chrome/installer/util/master_preferences.h"
#include "chrome/installer/util/util_constants.h"
#include "chrome_frame/chrome_launcher_utils.h"
#include "chrome_frame/ready_mode/ready_mode.h"

namespace {

// Looks up a command entry in the registry and attempts to execute it directly.
// Returns the new process handle, which the caller is responsible for closing,
// or NULL upon failure.
HANDLE LaunchCommandDirectly(const std::wstring& command_field) {
  BrowserDistribution* dist = BrowserDistribution::GetDistribution();
  std::wstring version_key_name(dist->GetVersionKey());

  HKEY roots[] = {HKEY_CURRENT_USER, HKEY_LOCAL_MACHINE};

  for (int i = 0; i < arraysize(roots); i++) {
    base::win::RegKey version_key;
    if (version_key.Open(roots[i], version_key_name.c_str(),
                         KEY_QUERY_VALUE) == ERROR_SUCCESS) {
      std::wstring command_line;
      if (version_key.ReadValue(command_field.c_str(),
                                &command_line) == ERROR_SUCCESS) {
        HANDLE launched_process = NULL;
        if (base::LaunchApp(command_line, false, true, &launched_process)) {
          return launched_process;
        }
      }
    }
  }
  return NULL;
}

// Attempts to launch a command using the ProcessLauncher. Returns a handle to
// the launched process, which the caller is responsible for closing, or NULL
// upon failure.
HANDLE LaunchCommandViaProcessLauncher(const std::wstring& command_field) {
  HANDLE launched_process = NULL;

  scoped_ptr<CommandLine> command_line(
      chrome_launcher::CreateUpdateCommandLine(command_field));

  if (command_line != NULL)
    base::LaunchApp(*command_line, false, true, &launched_process);

  return launched_process;
}

// Waits for the provided process to exit, and verifies that its exit code
// corresponds to one of the known "success" codes for the installer. If the
// exit code cannot be retrieved, or if it signals failure, returns false.
bool CheckProcessExitCode(HANDLE handle) {
  // TODO(erikwright): Use RegisterWaitForSingleObject to wait
  // asynchronously.
  DWORD wait_result = WaitForSingleObject(handle, 5000);  // (ms)

  if (wait_result == WAIT_OBJECT_0) {
    DWORD exit_code = 0;
    if (!::GetExitCodeProcess(handle, &exit_code)) {
      DPLOG(ERROR) << "GetExitCodeProcess failed.";
      return false;
    }

    // These are the only two success codes returned by the installer.
    // All other codes are errors.
    if (exit_code != 0 && exit_code != installer::UNINSTALL_REQUIRES_REBOOT) {
      DLOG(ERROR) << "Process failed with exit code " << exit_code << ".";
      return false;
    }

    return true;
  }

  if (wait_result == WAIT_FAILED)
    DPLOG(ERROR) << "Error while waiting for elevated child process.";

  if (wait_result == WAIT_ABANDONED)
    DLOG(ERROR) << "Unexpeced WAIT_ABANDONED while waiting on child process.";

  if (wait_result == WAIT_TIMEOUT)
    DLOG(ERROR) << "Timeout while waiting on child process.";

  return false;
}

// If we are running on XP (no protected mode) or in a high-integrity process,
// we can invoke the installer directly. If not, we will have to go via the
// ProcessLauncher.
bool CanLaunchDirectly() {
  if (base::win::GetVersion() < base::win::VERSION_VISTA)
    return true;

  base::IntegrityLevel integrity_level = base::INTEGRITY_UNKNOWN;
  if (!base::GetProcessIntegrityLevel(base::GetCurrentProcessHandle(),
                                      &integrity_level)) {
    DLOG(ERROR) << "Failed to determine process integrity level.";
    return false;
  }

  return integrity_level == base::HIGH_INTEGRITY;
}

// Attempts to launch the specified command either directly or via the
// ProcessLauncher. Returns true if the command is launched and returns a
// success code.
bool LaunchAndCheckCommand(const std::wstring& command_field) {
  base::win::ScopedHandle handle;

  if (CanLaunchDirectly())
    handle.Set(LaunchCommandDirectly(command_field));
  else
    handle.Set(LaunchCommandViaProcessLauncher(command_field));

  if (handle.IsValid() && CheckProcessExitCode(handle))
    return true;

  DLOG(ERROR) << "Command " << command_field << " could not be launched.";
  return false;
}

}  // namespace

RegistryReadyModeState::RegistryReadyModeState(
    const std::wstring& key_name, base::TimeDelta temporary_decline_duration,
    Observer* observer)
    : key_name_(key_name),
      temporary_decline_duration_(temporary_decline_duration),
      observer_(observer) {
}

RegistryReadyModeState::~RegistryReadyModeState() {
}

base::Time RegistryReadyModeState::GetNow() {
  return base::Time::Now();
}

ReadyModeStatus RegistryReadyModeState::GetStatus() {
  bool exists = false;
  int64 value = 0;

  if (!GetValue(&value, &exists))
    return READY_MODE_ACTIVE;

  if (!exists)
    return READY_MODE_ACCEPTED;

  if (value == 0)
    return READY_MODE_PERMANENTLY_DECLINED;

  if (value == 1)
    return READY_MODE_ACTIVE;

  base::Time when_declined(base::Time::FromInternalValue(value));
  base::Time now(GetNow());

  // If the decline duration has passed, or is further in the future than
  // the total timeout, consider it expired.
  bool expired = (now - when_declined) > temporary_decline_duration_ ||
      (when_declined - now) > temporary_decline_duration_;

  if (expired)
      return READY_MODE_TEMPORARY_DECLINE_EXPIRED;
  else
      return READY_MODE_TEMPORARILY_DECLINED;
}

void RegistryReadyModeState::NotifyObserver() {
  if (observer_ != NULL)
    observer_->OnStateChange(GetStatus());
}

bool RegistryReadyModeState::GetValue(int64* value, bool* exists) {
  *exists = false;
  *value = 0;

  HKEY roots[] = { HKEY_CURRENT_USER, HKEY_LOCAL_MACHINE };
  LONG result = ERROR_SUCCESS;
  for (int i = 0; i < arraysize(roots); i++) {
    base::win::RegKey config_key;
    result = config_key.Open(roots[i], key_name_.c_str(), KEY_QUERY_VALUE);
    if (result == ERROR_SUCCESS) {
      result = config_key.ReadInt64(installer::kChromeFrameReadyModeField,
                                    value);
      if (result == ERROR_SUCCESS) {
        *exists = true;
        return true;
      }
      if (result != ERROR_FILE_NOT_FOUND) {
        DLOG(ERROR) << "Failed to read from registry key " << key_name_
                    << " and value " << installer::kChromeFrameReadyModeField
                    << " error: " << result;
        return false;
      }
    }
  }

  return true;
}

void RegistryReadyModeState::RefreshStateAndNotify() {
  HRESULT hr = UrlMkSetSessionOption(URLMON_OPTION_USERAGENT_REFRESH,
                                     NULL, 0, 0);
  if (FAILED(hr)) {
    DLOG(ERROR) << "Failed to refresh user agent string from registry. "
                << "UrlMkSetSessionOption returned "
                << base::StringPrintf("0x%08x", hr);
  } else {
    NotifyObserver();
  }
}

void RegistryReadyModeState::ExpireTemporaryDecline() {
  if (LaunchAndCheckCommand(google_update::kRegCFEndTempOptOutCmdField))
    RefreshStateAndNotify();
}

void RegistryReadyModeState::TemporarilyDeclineChromeFrame() {
  if (LaunchAndCheckCommand(google_update::kRegCFTempOptOutCmdField))
    RefreshStateAndNotify();
}

void RegistryReadyModeState::PermanentlyDeclineChromeFrame() {
  if (LaunchAndCheckCommand(google_update::kRegCFOptOutCmdField))
    RefreshStateAndNotify();
}

void RegistryReadyModeState::AcceptChromeFrame() {
  if (LaunchAndCheckCommand(google_update::kRegCFOptInCmdField))
    NotifyObserver();
}
