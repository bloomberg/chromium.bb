// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file contains the specification of setup main functions.

#ifndef CHROME_INSTALLER_SETUP_INSTALL_H_
#define CHROME_INSTALLER_SETUP_INSTALL_H_
#pragma once

#include <string>
#include <vector>

#include "base/version.h"
#include "chrome/installer/util/product.h"
#include "chrome/installer/util/master_preferences.h"
#include "chrome/installer/util/util_constants.h"

class DictionaryValue;
class FilePath;

namespace installer {

class Package;

// This function installs or updates a new version of Chrome. It returns
// install status (failed, new_install, updated etc).
//
// setup_path: Path to the executable (setup.exe) as it will be copied
//           to Chrome install folder after install is complete
// archive_path: Path to the archive (chrome.7z) as it will be copied
//               to Chrome install folder after install is complete
// install_temp_path: working directory used during install/update. It should
//                    also has a sub dir source that contains a complete
//                    and unpacked Chrome package.
// prefs: master preferences. See chrome/installer/util/master_preferences.h.
// new_version: new Chrome version that needs to be installed
// package: Represents the target installation folder and all distributions
//          to be installed in that folder.
//
// Note: since caller unpacks Chrome to install_temp_path\source, the caller
// is responsible for cleaning up install_temp_path.
installer::InstallStatus InstallOrUpdateChrome(
    const FilePath& setup_path, const FilePath& archive_path,
    const FilePath& install_temp_path, const FilePath& prefs_path,
    const installer::MasterPreferences& prefs, const Version& new_version,
    const Package& package);

// Registers or unregisters the COM DLLs whose file names are given in
// |dll_files| and who reside in the path |dll_folder|.
// |system_level| specifies whether to call the system or user level DLL
// registration entry points.
// |do_register| says whether to register or unregister.
// |rollback_on_failure| says whether we should try to revert the registration
// or unregistration by calling the opposite entry point on the DLLs if
// something goes wrong.
bool RegisterComDllList(const FilePath& dll_folder,
                        const std::vector<FilePath>& dll_files,
                        bool system_level,
                        bool do_register,
                        bool rollback_on_failure);

}  // namespace installer

#endif  // CHROME_INSTALLER_SETUP_INSTALL_H_
