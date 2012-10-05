// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file contains the specification of setup main functions.

#ifndef CHROME_INSTALLER_SETUP_INSTALL_H_
#define CHROME_INSTALLER_SETUP_INSTALL_H_

#include <vector>

#include "base/string16.h"
#include "base/version.h"
#include "chrome/installer/util/installation_state.h"
#include "chrome/installer/util/installer_state.h"
#include "chrome/installer/util/master_preferences.h"
#include "chrome/installer/util/product.h"
#include "chrome/installer/util/util_constants.h"

class FilePath;

namespace installer {

class InstallationState;
class InstallerState;
class MasterPreferences;

enum InstallShortcutOperation {
  // Create mandatory shortcuts (Start menu, taskbar, and uninstall).
  INSTALL_SHORTCUT_CREATE_MANDATORY = 0,
  // Create mandatory shortcuts and desktop + quick launch shortcuts.
  INSTALL_SHORTCUT_CREATE_ALL,
  // Replace all shortcuts that still exist with the most recent version of
  // each individual shortcut.
  INSTALL_SHORTCUT_REPLACE_EXISTING,
};

// Escape |att_value| as per the XML AttValue production
// (http://www.w3.org/TR/2008/REC-xml-20081126/#NT-AttValue) for a value in
// single quotes.
void EscapeXmlAttributeValueInSingleQuotes(string16* att_value);

// Creates VisualElementsManifest.xml beside chrome.exe in |src_path| if
// |src_path|\VisualElements exists.
// Returns true unless the manifest is supposed to be created, but fails to be.
bool CreateVisualElementsManifest(const FilePath& src_path,
                                  const Version& version);

// Overwrites shortcuts (desktop, quick launch, start menu, and uninstall link)
// if they are present on the system.
// If |install_operation| is a creation command, appropriate shortcuts will be
// created even if they don't exist.
// The uninstall link is only created if this is not an MSI install.
// If creating the Start menu shortcut is successful, it is also pinned to the
// taskbar.
void CreateOrUpdateShortcuts(const InstallerState& installer_state,
                             const FilePath& setup_exe,
                             const Product& product,
                             InstallShortcutOperation install_operation,
                             bool alternate_desktop_shortcut);

// Registers Chrome on this machine.
// If |make_chrome_default|, also attempts to make Chrome default (potentially
// popping a UAC if the user is not an admin and HKLM registrations are required
// to register Chrome's capabilities on this version of Windows (i.e.
// pre-Win8)).
void RegisterChromeOnMachine(const InstallerState& installer_state,
                             const Product& product,
                             bool make_chrome_default);

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
InstallStatus InstallOrUpdateProduct(
    const InstallationState& original_state,
    const InstallerState& installer_state,
    const FilePath& setup_path,
    const FilePath& archive_path,
    const FilePath& install_temp_path,
    const FilePath& prefs_path,
    const installer::MasterPreferences& prefs,
    const Version& new_version);

// Performs installation-related tasks following an OS upgrade.
void HandleOsUpgradeForBrowser(const InstallerState& installer_state,
                               const Product& chrome,
                               const FilePath& setup_exe);

}  // namespace installer

#endif  // CHROME_INSTALLER_SETUP_INSTALL_H_
