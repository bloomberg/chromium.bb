// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TOOLS_DISABLE_OUTDATED_BUILD_DETECTOR_CONSTANTS_H_
#define CHROME_TOOLS_DISABLE_OUTDATED_BUILD_DETECTOR_CONSTANTS_H_

#include <stdint.h>

namespace switches {

extern const wchar_t kMultiInstall[];
extern const wchar_t kSystemLevel[];

}  // namespace switches

namespace env {

extern const wchar_t kGoogleUpdateIsMachine[];

}  // namespace env

extern const wchar_t kAOHY[];
extern const wchar_t kBrand[];
extern const wchar_t kInstallerResult[];
extern const wchar_t kInstallerError[];
extern const wchar_t kInstallerExtraCode1[];
extern const wchar_t kUninstallArguments[];

// Values for the process exit code and for Google Update's InstallerError,
// both of which are of type DWORD (a 32-bit unsigned int). The codes start at
// 1000 so as to not collide with existing setup.exe and mini_installer.exe
// exit codes (found in chrome/installer/util/util_constants.h and
// chrome/installer/mini_installer/exit_code.h, respectively).
enum class ExitCode : uint32_t {
  // Success exit codes.
  BOTH_BRANDS_UPDATED = 1000,   // Both Chrome and the Binaries were updated.
  CHROME_BRAND_UPDATED = 1001,  // Only Chrome's brand was updated.
  NO_CHROME = 1002,             // Chrome's ClientState key was not found.
  NON_ORGANIC_BRAND = 1003,     // A non-organic brand was found, no update.

  // Failure exit codes.
  UNKNOWN_FAILURE = 1010,  // Never reported.
  UNSUPPORTED_OS = 1011,   // Client OS is Windows 7 or newer.

  // For these failure modes, Google Update's ExtraCode1 field is populated with
  // the Windows error code corresponding to the failed operation.
  FAILED_OPENING_KEY = 1012,    // RegOpenKeyEx failed.
  FAILED_READING_BRAND = 1013,  // RegQueryValueEx failed.
  FAILED_WRITING_BRAND = 1014,  // RegSetValueEx failed.
};

// Values defined by Google Update.
enum class InstallerResult : uint32_t {
  FAILED_CUSTOM_ERROR = 1,
};

#endif  // CHROME_TOOLS_DISABLE_OUTDATED_BUILD_DETECTOR_CONSTANTS_H_
