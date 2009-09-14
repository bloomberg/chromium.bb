// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file contains the specification of setup main functions.

#ifndef CHROME_INSTALLER_SETUP_INSTALL_H_
#define CHROME_INSTALLER_SETUP_INSTALL_H_

#include <base/values.h>

#include "chrome/installer/util/util_constants.h"
#include "chrome/installer/util/version.h"

namespace installer {
// Get path to the installer under Chrome version folder
// (for example <path>\Google\Chrome\<Version>\installer)
std::wstring GetInstallerPathUnderChrome(const std::wstring& install_path,
                                         const std::wstring& new_version);

// This function installs or updates a new version of Chrome. It returns
// install status (failed, new_install, updated etc).
//
// exe_path: Path to the executable (setup.exe) as it will be copied
//           to Chrome install folder after install is complete
// archive_path: Path to the archive (chrome.7z) as it will be copied
//               to Chrome install folder after install is complete
// install_temp_path: working directory used during install/update. It should
//                    also has a sub dir source that contains a complete
//                    and unpacked Chrome package.
// prefs: master preferences. See chrome/installer/util/master_preferences.h.
// new_version: new Chrome version that needs to be installed
// installed_version: currently installed version of Chrome, if any, or
//                    NULL otherwise
//
// Note: since caller unpacks Chrome to install_temp_path\source, the caller
// is responsible for cleaning up install_temp_path.
installer_util::InstallStatus InstallOrUpdateChrome(
    const std::wstring& exe_path, const std::wstring& archive_path,
    const std::wstring& install_temp_path, const std::wstring& prefs_path,
    const DictionaryValue* prefs, const Version& new_version,
    const Version* installed_version);
}

#endif  // CHROME_INSTALLER_SETUP_INSTALL_H_
