// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Utilities for windows process and threads stuff.

#include "ceee/common/process_utils_win.h"

#include <sddl.h>

#include "base/logging.h"
#include "base/scoped_handle.h"
#include "base/win/windows_version.h"
#include "ceee/common/com_utils.h"


namespace process_utils_win {

HRESULT SetThreadIntegrityLevel(HANDLE* thread, const std::wstring& level) {
  HANDLE temp_handle = NULL;
  BOOL success = ::OpenProcessToken(
      ::GetCurrentProcess(), MAXIMUM_ALLOWED, &temp_handle);
  ScopedHandle process_token(temp_handle);
  temp_handle = NULL;
  if (success) {
    success = ::DuplicateTokenEx(
        process_token, MAXIMUM_ALLOWED, NULL, SecurityImpersonation,
        TokenImpersonation, &temp_handle);
    ScopedHandle mic_token(temp_handle);
    temp_handle = NULL;
    if (success) {
      PSID mic_sid = NULL;
      success = ::ConvertStringSidToSid(level.c_str(), &mic_sid);
      if (success) {
        // Set Process IL to Low
        TOKEN_MANDATORY_LABEL tml = {0};
        tml.Label.Attributes = SE_GROUP_INTEGRITY | SE_GROUP_INTEGRITY_ENABLED;
        tml.Label.Sid = mic_sid;
        success = ::SetTokenInformation(
            mic_token, TokenIntegrityLevel, &tml,
            sizeof(tml) + ::GetLengthSid(mic_sid));
        if (success) {
          success = ::SetThreadToken(thread, mic_token);
          LOG_IF(WARNING, !success) << "Failed to SetThreadToken." <<
              com::LogWe();
        } else {
          LOG(WARNING) << "Failed to SetTokenInformation." << com::LogWe();
        }
        ::LocalFree(mic_sid);
      } else {
        LOG(WARNING) << "Failed to SIDConvert: " << level << com::LogWe();
      }
    } else {
      LOG(WARNING) << "Failed to Duplicate the process token." << com::LogWe();
    }
  } else {
    LOG(WARNING) << "Failed to Open the process token." << com::LogWe();
  }

  if (success)
    return S_OK;
  else
    return HRESULT_FROM_WIN32(GetLastError());
}

HRESULT ResetThreadIntegrityLevel(HANDLE* thread) {
  if (::SetThreadToken(thread, NULL))
    return S_OK;
  else
    return HRESULT_FROM_WIN32(GetLastError());
}

HRESULT IsCurrentProcessUacElevated(bool* running_as_admin) {
  DCHECK(NULL != running_as_admin);

  if (base::win::GetVersion() < base::win::VERSION_VISTA) {
    *running_as_admin = false;
    return S_OK;
  }

  HANDLE temp_handle = NULL;
  if (!::OpenProcessToken(::GetCurrentProcess(), TOKEN_QUERY, &temp_handle)) {
    DWORD error_code = ::GetLastError();
    LOG(WARNING) << "Failed to Open the process token." <<
        com::LogWe(error_code);
    return com::AlwaysErrorFromWin32(error_code);
  }

  // We check if the current process is running in a higher integrity mode than
  // it would be running if it was started 'normally' by checking if its token
  // has an associated full elevation token. This seems to do the trick and I
  // carefully checked it against the obvious alternative of checking the
  // integrity level of the current process. This is what I found out:
  // UAC off, normal start: token default, high integrity
  // UAC off, admin start: token default, high integrity
  // UAC on, normal start: token limited, medium integrity
  // UAC on, admin start: token full, medium integrity
  // All that for an admin-group member, who can run in elevated mode.
  // This logic applies to Vista/Win7. The case of earlier systems is handled
  // at the start.
  ScopedHandle process_token(temp_handle);
  TOKEN_ELEVATION_TYPE elevation_type = TokenElevationTypeDefault;
  DWORD variable_len_dummy = 0;
  if (!::GetTokenInformation(process_token, TokenElevationType, &elevation_type,
      sizeof(elevation_type), &variable_len_dummy)) {
    DWORD error_code = ::GetLastError();
    LOG(WARNING) << "Failed to retrieve token information." <<
        com::LogWe(error_code);
    return com::AlwaysErrorFromWin32(error_code);
  }

  *running_as_admin = elevation_type == TokenElevationTypeFull;

  return S_OK;
}

ProcessCompatibilityCheck::OpenProcessFuncType
    ProcessCompatibilityCheck::open_process_func_ = OpenProcess;
ProcessCompatibilityCheck::CloseHandleFuncType
    ProcessCompatibilityCheck::close_handle_func_ = CloseHandle;
ProcessCompatibilityCheck::IsWOW64ProcessFuncType
    ProcessCompatibilityCheck::is_wow64_process_func_ = IsWow64Process;

ProcessCompatibilityCheck::ProcessCompatibilityCheck()
    : initialization_result_(E_FAIL),
      running_on_amd64_platform_(false),
      running_as_wow_64_(false),
      integrity_checks_on_(false),
      running_integrity_(base::INTEGRITY_UNKNOWN) {
  StandardInitialize();
}

void ProcessCompatibilityCheck::StandardInitialize() {
  HRESULT hr = S_OK;

  SYSTEM_INFO system_info;
  ::GetNativeSystemInfo(&system_info);

  bool system_with_integrity_checks =
      base::win::GetVersion() >= base::win::VERSION_VISTA;

  base::IntegrityLevel integrity_level = base::INTEGRITY_UNKNOWN;
  if (system_with_integrity_checks &&
      !base::GetProcessIntegrityLevel(::GetCurrentProcess(),
                                      &integrity_level)) {
    hr = com::AlwaysErrorFromLastError();
  }

  BOOL is_wow_64 = FALSE;
  // If not running_on_amd64_platform_, all processes are OK and
  // we wouldn't need all these tools...
  if (system_info.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_AMD64 &&
      !is_wow64_process_func_(::GetCurrentProcess(), &is_wow_64)) {
    hr = com::AlwaysErrorFromLastError();
  }

  KnownStateInitialize(system_info.wProcessorArchitecture, is_wow_64,
                       system_with_integrity_checks, integrity_level);
  initialization_result_ = hr;
}

// Initialize with exactly this data with some checks, just in case.
void ProcessCompatibilityCheck::KnownStateInitialize(
    WORD system_type, bool current_process_wow64,
    bool check_integrity, base::IntegrityLevel current_process_integrity) {
  DCHECK(system_type == PROCESSOR_ARCHITECTURE_AMD64 ||
         system_type == PROCESSOR_ARCHITECTURE_INTEL);

  running_on_amd64_platform_ = system_type == PROCESSOR_ARCHITECTURE_AMD64;
  running_as_wow_64_ = current_process_wow64;

  // Can't be running_as_wow_64_ while not running_on_amd64_platform_.
  DCHECK((running_on_amd64_platform_ && running_as_wow_64_) ||
         !running_as_wow_64_);

  integrity_checks_on_ = check_integrity;
  running_integrity_ = current_process_integrity;

  // Assert that running_integrity_ is given whenever integrity_checks_on_:
  // integrity_checks_on_ => running_integrity_ != base::INTEGRITY_UNKNOWN AND
  // !integrity_checks_on_ => running_integrity_ == base::INTEGRITY_UNKNOWN.
  DCHECK(((integrity_checks_on_ &&
           running_integrity_ != base::INTEGRITY_UNKNOWN) ||
          !integrity_checks_on_) &&
         ((!integrity_checks_on_ &&
           running_integrity_ == base::INTEGRITY_UNKNOWN) ||
          integrity_checks_on_));
}

HRESULT ProcessCompatibilityCheck::IsCompatible(HWND process_window,
                                                bool* is_compatible) {
  DWORD process_id = 0;
  if (::GetWindowThreadProcessId(process_window, &process_id) != 0) {
    return IsCompatible(process_id, is_compatible);
  }

  return E_FAIL;
}

HRESULT ProcessCompatibilityCheck::IsCompatible(DWORD process_id,
                                                bool* is_compatible) {
  ProcessCompatibilityCheck* instance
      = Singleton<ProcessCompatibilityCheck>::get();

  DCHECK(instance != NULL);
  DCHECK(is_compatible != NULL);

  if (instance != NULL && SUCCEEDED(instance->initialization_result_)) {
    return instance->IsProcessCompatible(process_id, is_compatible);
  }

  return instance != NULL ? instance->initialization_result_ : E_FAIL;
}

HRESULT ProcessCompatibilityCheck::IsProcessCompatible(DWORD process_id,
                                                       bool* is_compatible) {
  DCHECK(is_compatible != NULL);

  if (!running_on_amd64_platform_ && !integrity_checks_on_) {
    *is_compatible = true;
    return S_OK;
  }

  HRESULT hr = E_FAIL;
  HANDLE other_process = open_process_func_(PROCESS_QUERY_INFORMATION,
                                            FALSE, process_id);

  if (other_process != NULL) {
    bool platform_compatible = false;
    bool integrity_compatible = false;
    hr = IsProcessPlatformCompatible(other_process, &platform_compatible);
    if (SUCCEEDED(hr))
      hr = IsProcessIntegrityCompatible(other_process, &integrity_compatible);

    if (SUCCEEDED(hr))
      *is_compatible = platform_compatible && integrity_compatible;

    if (!close_handle_func_(other_process))
      LOG(WARNING) << "CloseHandle failed: " << com::LogWe();
  }

  return hr;
}

// Is the process we are inquiring about of the same type
// (native 32, native 64 or WOW64) as the current process.
HRESULT ProcessCompatibilityCheck::IsProcessPlatformCompatible(
    HANDLE process_handle, bool* is_compatible) {
  if (!running_on_amd64_platform_) {
    *is_compatible = true;
    return S_OK;
  }

  BOOL is_other_wow_64 = FALSE;

  if (is_wow64_process_func_(process_handle, &is_other_wow_64)) {
    *is_compatible = (running_as_wow_64_ && is_other_wow_64) ||
                      !(running_as_wow_64_ || is_other_wow_64);
    return S_OK;
  }

  return com::AlwaysErrorFromLastError();
}

// Is the process we are inquiring about at the integrity level which should
// permit inserting an executor into that process?
// For the purpose of security-compatibility high integrity processes are
// considered a separate class from medium and low integrity processes because
// in interactive scenarios they will belong to different logon sessions.
// It appears we cannot programatically cross this boundary in COM and so we
// want to filter these processes out. On the other hand, medium and low
// integrity processes will exist within the same logon session and can
// attempt communication (in the sense discussed here).
// In practice this corresponds to running instances of IE as administrator and
// normally. The former group of processes will function at high integrity level
// while the latter at medium or low (IE's protected mode?). Note that each of
// these groups of processes will have a separate ceee_broker. Inquiring about
// process integrity appears to be a very practical way of properly routing
// requests to IE's windows from a broker instance.
HRESULT ProcessCompatibilityCheck::IsProcessIntegrityCompatible(
    HANDLE process_handle, bool* is_compatible) {
  if (!integrity_checks_on_) {
    *is_compatible = true;
    return S_OK;
  }

  base::IntegrityLevel integrity_level = base::INTEGRITY_UNKNOWN;
  if (base::GetProcessIntegrityLevel(process_handle, &integrity_level)) {
    *is_compatible = (integrity_level == base::HIGH_INTEGRITY &&
                      running_integrity_ == base::HIGH_INTEGRITY) ||
                     (integrity_level != base::HIGH_INTEGRITY &&
                      running_integrity_ != base::HIGH_INTEGRITY);
    return S_OK;
  }

  return com::AlwaysErrorFromLastError();
}

// Patching affects both static pointers and the state of the sigleton.
// To update the later, required variables are forwarded to its
// KnownStateInitialize.
void ProcessCompatibilityCheck::PatchState(
    WORD system_type, bool current_process_wow64,
    bool check_integrity, base::IntegrityLevel current_process_integrity,
    OpenProcessFuncType open_process_func,
    CloseHandleFuncType close_handle_func,
    IsWOW64ProcessFuncType is_wow64_process_func) {
  PatchState(open_process_func, close_handle_func, is_wow64_process_func);
  Singleton<ProcessCompatibilityCheck>::get()->KnownStateInitialize(
      system_type, current_process_wow64,
      check_integrity, current_process_integrity);
}

void ProcessCompatibilityCheck::PatchState(
    OpenProcessFuncType open_process_func,
    CloseHandleFuncType close_handle_func,
    IsWOW64ProcessFuncType is_wow64_process_func) {
  open_process_func_ = open_process_func;
  close_handle_func_ = close_handle_func;
  is_wow64_process_func_ = is_wow64_process_func;
  DCHECK(open_process_func_ != NULL && close_handle_func_ != NULL &&
         is_wow64_process_func_ != NULL);
}

void ProcessCompatibilityCheck::ResetState() {
  PatchState(OpenProcess, CloseHandle, IsWow64Process);
  Singleton<ProcessCompatibilityCheck>::get()->StandardInitialize();
}

}  // namespace com
