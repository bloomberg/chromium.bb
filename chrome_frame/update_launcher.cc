// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome_frame/update_launcher.h"

#include <windows.h>
#include <Shellapi.h>

#include "base/win/scoped_com_initializer.h"
#include "base/win/scoped_comptr.h"
#include "base/win/scoped_handle.h"
#include "google_update/google_update_idl.h"

namespace {

const wchar_t kChromeFrameGuid[] = L"{8BA986DA-5100-405E-AA35-86F34A02ACBF}";

const DWORD kLaunchFailureExitCode = 0xFF;

const wchar_t kUpdateCommandFlag[] = L"--update-cmd";

// Waits indefinitely for the provided process to exit. Returns the process's
// exit code, or kLaunchFailureExitCode if an error occurs in the waiting.
DWORD WaitForProcessExitCode(HANDLE handle) {
  DWORD exit_code = 0;
  return ((::WaitForSingleObject(handle, INFINITE) == WAIT_OBJECT_0) &&
          ::GetExitCodeProcess(handle, &exit_code)) ?
      exit_code : kLaunchFailureExitCode;
}

}  // namespace

namespace update_launcher {

std::wstring GetUpdateCommandFromArguments(const wchar_t* command_line) {
  std::wstring command;

  if (command_line != NULL) {
    int num_args = 0;
    wchar_t** args = NULL;
    args = ::CommandLineToArgvW(command_line, &num_args);

    if (args) {
      if (num_args == 3 && _wcsicmp(args[1], kUpdateCommandFlag) == 0)
        command = args[2];
      ::LocalFree(args);
    }
  }

  return command;
}

DWORD LaunchUpdateCommand(const std::wstring& command) {
  base::win::ScopedCOMInitializer com_initializer;
  if (!com_initializer.succeeded())
    return kLaunchFailureExitCode;

  base::win::ScopedComPtr<IProcessLauncher> ipl;
  if (FAILED(ipl.CreateInstance(__uuidof(ProcessLauncherClass))))
    return kLaunchFailureExitCode;

  ULONG_PTR phandle = NULL;
  if (FAILED(ipl->LaunchCmdElevated(kChromeFrameGuid, command.c_str(),
                                    ::GetCurrentProcessId(), &phandle)))
    return kLaunchFailureExitCode;

  base::win::ScopedHandle process(reinterpret_cast<HANDLE>(phandle));
  return WaitForProcessExitCode(process);
}

}  // namespace process_launcher
