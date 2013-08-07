// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DIAGNOSTICS_RECON_DIAGNOSTICS_H_
#define CHROME_BROWSER_DIAGNOSTICS_RECON_DIAGNOSTICS_H_

#include "chrome/browser/diagnostics/diagnostics_test.h"

namespace diagnostics {

enum OutcomeCodes {
  DIAG_RECON_SUCCESS,

  // OperatingSystemTest
  DIAG_RECON_PRE_WINDOW_XP_SP2,

  // ConflictingDllsTest
  DIAG_RECON_DICTIONARY_LOOKUP_FAILED,
  DIAG_RECON_NO_STATUS_FIELD,
  DIAG_RECON_NO_NAME_FIELD,
  DIAG_RECON_NO_LOCATION_FIELD,
  DIAG_RECON_CONFLICTING_MODULES,
  DIAG_RECON_NOT_IMPLEMENTED,

  // InstallTypeTest
  DIAG_RECON_INSTALL_PATH_PROVIDER,

  // VersionTest
  DIAG_RECON_NO_VERSION,
  DIAG_RECON_EMPTY_VERSION,

  // PathTest
  DIAG_RECON_DEPENDENCY,
  DIAG_RECON_PATH_PROVIDER,
  DIAG_RECON_PATH_NOT_FOUND,
  DIAG_RECON_CANNOT_OBTAIN_SIZE,
  DIAG_RECON_FILE_TOO_LARGE,
  DIAG_RECON_NOT_WRITABLE,

  // DiskSpaceTest
  DIAG_RECON_UNABLE_TO_QUERY,
  DIAG_RECON_LOW_DISK_SPACE,

  // JSONTest
  DIAG_RECON_FILE_NOT_FOUND,
  DIAG_RECON_FILE_NOT_FOUND_OK,
  DIAG_RECON_CANNOT_OBTAIN_FILE_SIZE,
  DIAG_RECON_FILE_TOO_BIG,
  DIAG_RECON_UNABLE_TO_OPEN_FILE,
  DIAG_RECON_PARSE_ERROR,
};

// Identifiers for the tests.
extern const char kConflictingDllsTest[];
extern const char kDiskSpaceTest[];
extern const char kInstallTypeTest[];
extern const char kJSONBookmarksTest[];
extern const char kJSONLocalStateTest[];
extern const char kJSONProfileTest[];
extern const char kOperatingSystemTest[];
extern const char kPathDictionariesTest[];
extern const char kPathLocalStateTest[];
extern const char kPathResourcesTest[];
extern const char kPathUserDataTest[];
extern const char kVersionTest[];

DiagnosticsTest* MakeOperatingSystemTest();
DiagnosticsTest* MakeConflictingDllsTest();
DiagnosticsTest* MakeInstallTypeTest();
DiagnosticsTest* MakeVersionTest();
DiagnosticsTest* MakeUserDirTest();
DiagnosticsTest* MakeLocalStateFileTest();
DiagnosticsTest* MakeDictonaryDirTest();
DiagnosticsTest* MakeResourcesFileTest();
DiagnosticsTest* MakeDiskSpaceTest();
DiagnosticsTest* MakePreferencesTest();
DiagnosticsTest* MakeBookMarksTest();
DiagnosticsTest* MakeLocalStateTest();

}  // namespace diagnostics

#endif  // CHROME_BROWSER_DIAGNOSTICS_RECON_DIAGNOSTICS_H_
