// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_INSTALLER_SETUP_CF_MIGRATION_H_
#define CHROME_INSTALLER_SETUP_CF_MIGRATION_H_

#include "chrome/installer/util/util_constants.h"

class CommandLine;

namespace base {
class FilePath;
}

namespace installer {

class InstallationState;
class InstallerState;
class ProductState;

// Invokes a child helper instance of the setup.exe in |installer_directory| to
// run MigrateChromeFrame (see comments below) using the archive in
// |installer_directory| for the given installation level. Returns true if the
// process is launched.
bool LaunchChromeFrameMigrationProcess(
    const ProductState& chrome_frame_product,
    const CommandLine& command_line,
    const base::FilePath& installer_directory,
    bool system_level);

// Migrates multi-install Chrome Frame to single-install at the current
// level. Does not remove the multi-install binaries if no other products are
// using them. --uncompressed-archive=chrome.7z is expected to be given on the
// command line to point this setup.exe at the (possibly patched) archive from
// the calling instance.
// Note about process model: this is called in a child setup.exe that is
// invoked from the setup.exe instance run as part of an update to a
// multi-install Chrome Frame.
InstallStatus MigrateChromeFrame(const InstallationState& original_state,
                                 InstallerState* installer_state);

}  // namespace installer

#endif  // CHROME_INSTALLER_SETUP_CF_MIGRATION_H_
