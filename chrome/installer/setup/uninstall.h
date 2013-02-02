// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file declares Chrome uninstall related functions.

#ifndef CHROME_INSTALLER_SETUP_UNINSTALL_H_
#define CHROME_INSTALLER_SETUP_UNINSTALL_H_

#include <shlobj.h>

#include "base/string16.h"
#include "chrome/installer/util/util_constants.h"

class BrowserDistribution;
class CommandLine;

namespace base {
class FilePath;
}

namespace installer {

class InstallationState;
class InstallerState;
class Product;

// This function removes all Chrome registration related keys. It returns true
// if successful, otherwise false. The error code is set in |exit_code|.
// |root| is the registry root (HKLM|HKCU) and |browser_entry_suffix| is the
// suffix for default browser entry name in the registry (optional).
bool DeleteChromeRegistrationKeys(const InstallerState& installer_state,
                                  BrowserDistribution* dist,
                                  HKEY root,
                                  const string16& browser_entry_suffix,
                                  InstallStatus* exit_code);

// Removes any legacy registry keys from earlier versions of Chrome that are no
// longer needed. This is used during autoupdate since we don't do full
// uninstalls/reinstalls to update.
void RemoveChromeLegacyRegistryKeys(BrowserDistribution* dist,
                                    const string16& chrome_exe);

// This function uninstalls a product.  Hence we came up with this awesome
// name for it.
//
// original_state: The installation state of all products on the system.
// installer_state: State associated with this operation.
// setup_path: Path to the executable (setup.exe) as it will be copied
//           to temp folder before deleting Chrome folder.
// dist: Represents the distribution to be uninstalled.
// remove_all: Remove all shared files, registry entries as well.
// force_uninstall: Uninstall without prompting for user confirmation or
//                  any checks for Chrome running.
// cmd_line: CommandLine that contains information about the command that
//           was used to launch current uninstaller.
installer::InstallStatus UninstallProduct(
    const InstallationState& original_state,
    const InstallerState& installer_state,
    const base::FilePath& setup_path,
    const Product& dist,
    bool remove_all,
    bool force_uninstall,
    const CommandLine& cmd_line);

// Cleans up the installation directory after all uninstall operations have
// completed. Depending on what products are remaining, setup.exe and the
// installer archive may be deleted. Empty directories will be pruned (or
// scheduled for pruning after reboot, if necessary).
//
// original_state: The installation state of all products on the system.
// installer_state: State associated with this operation.
// cmd_line: CommandLine that contains information about the command that
//           was used to launch current uninstaller.
// uninstall_status: the uninstall status so far (may change during invocation).
void CleanUpInstallationDirectoryAfterUninstall(
    const InstallationState& original_state,
    const InstallerState& installer_state,
    const CommandLine& cmd_line,
    installer::InstallStatus* uninstall_status);

}  // namespace installer

#endif  // CHROME_INSTALLER_SETUP_UNINSTALL_H_
