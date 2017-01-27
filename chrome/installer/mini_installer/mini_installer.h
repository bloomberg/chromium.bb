// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_INSTALLER_MINI_INSTALLER_MINI_INSTALLER_H_
#define CHROME_INSTALLER_MINI_INSTALLER_MINI_INSTALLER_H_

#include <windows.h>

#include "chrome/installer/mini_installer/exit_code.h"

namespace mini_installer {

// A container of a process exit code (eventually passed to ExitProcess) and
// a Windows error code for cases where the exit code is non-zero.
struct ProcessExitResult {
  DWORD exit_code;
  DWORD windows_error;

  explicit ProcessExitResult(DWORD exit) : exit_code(exit), windows_error(0) {}
  ProcessExitResult(DWORD exit, DWORD win)
      : exit_code(exit), windows_error(win) {}

  bool IsSuccess() const { return exit_code == SUCCESS_EXIT_CODE; }
};

// Main function for Chrome's mini_installer. First gets a working dir, unpacks
// the resources, and finally executes setup.exe to do the install/update. Also
// handles invoking a previous version's setup.exe to patch itself in the case
// of differential updates.
ProcessExitResult WMain(HMODULE module);

}  // namespace mini_installer

#endif  // CHROME_INSTALLER_MINI_INSTALLER_MINI_INSTALLER_H_
