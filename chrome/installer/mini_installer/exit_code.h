// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_INSTALLER_MINI_INSTALLER_EXIT_CODE_H_
#define CHROME_INSTALLER_MINI_INSTALLER_EXIT_CODE_H_

namespace mini_installer {

// mini_installer process exit codes (the underlying type is uint32_t).
enum ExitCode {
  SUCCESS_EXIT_CODE = 0,
  GENERIC_ERROR = 1,
  // The next three generic values are here for historic reasons. New additions
  // should have values strictly greater than them. This is to prevent
  // collisions with setup.exe's installer::InstallStatus enum since the two are
  // surfaced similarly by Google Update.
  GENERIC_INITIALIZATION_FAILURE = 101,
  GENERIC_UNPACKING_FAILURE = 102,
  GENERIC_SETUP_FAILURE = 103,
};

}  // namespace mini_installer

#endif  // CHROME_INSTALLER_MINI_INSTALLER_EXIT_CODE_H_
