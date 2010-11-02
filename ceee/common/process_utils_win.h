// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Utilities for process/threads in Windows.

#ifndef CEEE_COMMON_PROCESS_UTILS_WIN_H_
#define CEEE_COMMON_PROCESS_UTILS_WIN_H_

#include <windows.h>
#include <wtypes.h>

#include <string>

#include "base/basictypes.h"
#include "base/singleton.h"
#include "base/process_util.h"

namespace process_utils_win {

// Sets the integrity level of the specified thread, or the current thread
// if thread is NULL. The level is a string as #defined in sddl.h, e.g.,
// SDDL_ML_LOW or any string of the proper SID Component format as described
// here: http://msdn.microsoft.com/en-us/library/aa379597(v=VS.85).aspx
// Will return HRESULT_FROM_WIN32(ERROR_PRIVILEGE_NOT_HELD) if the provided
// thread (or its process) have lower priviliges then the ones requested.
HRESULT SetThreadIntegrityLevel(HANDLE* thread, const std::wstring& level);

// Reset the thread integrity level to the process level. Again, if thread
// is NULL, set the integrity level of the current thread.
HRESULT ResetThreadIntegrityLevel(HANDLE* thread);

// The bool pointer will be set to true if the current process has been started
// in administrator's mode ('Run As Adminstrator') while UAC was active.
// Programs started that way run at different integrity level than these
// invoked by the user in a regular way.
// More detailed analysis is with the implementation.
HRESULT IsCurrentProcessUacElevated(bool* running_as_admin);

// Checks if a given process is compatible with the current process.
// Two processes are compatible if they run on the same platform-compatible and
// are security-compatible.
// The former makes sense on a 64-bit platform, where 32-bit processes inhabit
// a separate subsystem (WOW64). Thus, two processes are platform-compatible
// when both are 32-bit or both are 64-bit. It is noop on 32-bit systems.
// Two processes are security-compatible when their integrity levels permit COM
// communication between them. High integrity processes are compatible only with
// other high integrity processes. A low or a medium integrity process is
// compatible with any process that is either low or medium integrity.
// Some considerations of security-compatibility are expanded upon in comments
// with implementation.
class ProcessCompatibilityCheck {
 public:
  // Is the process associated with the given window compatible with the
  // current process. If the call returns an error code, the value of
  // *is_compatible is undefined.
  static HRESULT IsCompatible(HWND process_window, bool* is_compatible);

  // Is the process of given ID compatible with the current process. If the
  // call returns an error code, the value of *is_compatible is undefined.
  static HRESULT IsCompatible(DWORD process_id, bool* is_compatible);

 protected:
  typedef HANDLE (WINAPI *OpenProcessFuncType)(DWORD, BOOL, DWORD);
  typedef BOOL (WINAPI *CloseHandleFuncType)(HANDLE);
  typedef BOOL (WINAPI *IsWOW64ProcessFuncType)(HANDLE, PBOOL);

  // Non-trivial constructor will initialize default state. Possible error code
  // will be stored for reference.
  ProcessCompatibilityCheck();

  // Exposed for unittesting only.
  static void PatchState(WORD system_type, bool current_process_wow64,
                         bool check_integrity,
                         base::IntegrityLevel current_process_integrity,
                         OpenProcessFuncType open_process_func,
                         CloseHandleFuncType close_handle_func,
                         IsWOW64ProcessFuncType is_wow64_process_func);
  static void PatchState(OpenProcessFuncType open_process_func,
                         CloseHandleFuncType close_handle_func,
                         IsWOW64ProcessFuncType is_wow64_process_func);
  // Reset to normal state.
  static void ResetState();

  // Is the process of given ID compatible with the current process?
  HRESULT IsProcessCompatible(DWORD process_id, bool* is_compatible);

 private:
  // Initialization function taking true system state.
  void StandardInitialize();

  // Initialize call assigning given values to objects.
  void KnownStateInitialize(WORD system_type, bool current_process_wow64,
                            bool check_integrity,
                            base::IntegrityLevel current_process_integrity);

  // Functions checking aspects of 'compatibility' of processes with the
  // current process.
  HRESULT IsProcessPlatformCompatible(HANDLE process_handle,
                                      bool* is_compatible);
  HRESULT IsProcessIntegrityCompatible(HANDLE process_handle,
                                       bool* is_compatible);

  HRESULT initialization_result_;
  bool running_on_amd64_platform_;
  bool running_as_wow_64_;
  bool integrity_checks_on_;
  base::IntegrityLevel running_integrity_;

  // Function pointers held to allow substituting with a test harness.
  static OpenProcessFuncType open_process_func_;
  static CloseHandleFuncType close_handle_func_;
  static IsWOW64ProcessFuncType is_wow64_process_func_;

  friend struct DefaultSingletonTraits<ProcessCompatibilityCheck>;

  DISALLOW_COPY_AND_ASSIGN(ProcessCompatibilityCheck);
};

}  // namespace process_utils_win

#endif  // CEEE_COMMON_PROCESS_UTILS_WIN_H_
