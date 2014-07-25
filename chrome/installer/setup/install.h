// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file contains the specification of setup main functions.

#ifndef CHROME_INSTALLER_SETUP_INSTALL_H_
#define CHROME_INSTALLER_SETUP_INSTALL_H_

#include <vector>

#include "base/strings/string16.h"
#include "base/version.h"
#include "chrome/installer/util/installation_state.h"
#include "chrome/installer/util/installer_state.h"
#include "chrome/installer/util/master_preferences.h"
#include "chrome/installer/util/product.h"
#include "chrome/installer/util/util_constants.h"

namespace base {
class FilePath;
}

namespace installer {

class InstallationState;
class InstallerState;
class MasterPreferences;

enum InstallShortcutOperation {
  // Create all shortcuts (potentially skipping those explicitly stated not to
  // be installed in the InstallShortcutPreferences).
  INSTALL_SHORTCUT_CREATE_ALL,
  // Create each per-user shortcut (potentially skipping those explicitly stated
  // not to be installed in the InstallShortcutPreferences), but only if the
  // system-level equivalent of that shortcut is not present on the system.
  INSTALL_SHORTCUT_CREATE_EACH_IF_NO_SYSTEM_LEVEL,
  // Replace all shortcuts that still exist with the most recent version of
  // each individual shortcut.
  INSTALL_SHORTCUT_REPLACE_EXISTING,
};

enum InstallShortcutLevel {
  // Install shortcuts for the current user only.
  CURRENT_USER,
  // Install global shortcuts visible to all users. Note: the Quick Launch
  // and taskbar pin shortcuts are still installed per-user (as they have no
  // all-users version).
  ALL_USERS,
};

// Escape |att_value| as per the XML AttValue production
// (http://www.w3.org/TR/2008/REC-xml-20081126/#NT-AttValue) for a value in
// single quotes.
void EscapeXmlAttributeValueInSingleQuotes(base::string16* att_value);

// Creates VisualElementsManifest.xml beside chrome.exe in |src_path| if
// |src_path|\VisualElements exists.
// Returns true unless the manifest is supposed to be created, but fails to be.
bool CreateVisualElementsManifest(const base::FilePath& src_path,
                                  const Version& version);

// Overwrites shortcuts (desktop, quick launch, and start menu) if they are
// present on the system.
// |prefs| can affect the behavior of this method through
// kDoNotCreateDesktopShortcut, kDoNotCreateQuickLaunchShortcut, and
// kAltShortcutText.
// |install_level| specifies whether to install per-user shortcuts or shortcuts
// for all users on the system (this should only be used to update legacy
// system-level installs).
// If |install_operation| is a creation command, appropriate shortcuts will be
// created even if they don't exist.
// If creating the Start menu shortcut is successful, it is also pinned to the
// taskbar.
void CreateOrUpdateShortcuts(
    const base::FilePath& target,
    const Product& product,
    const MasterPreferences& prefs,
    InstallShortcutLevel install_level,
    InstallShortcutOperation install_operation);

// Registers Chrome on this machine.
// If |make_chrome_default|, also attempts to make Chrome default where doing so
// requires no more user interaction than a UAC prompt. In practice, this means
// on versions of Windows prior to Windows 8.
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
// src_path: the unpacked Chrome package (inside |install_temp_path|).
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
    const base::FilePath& setup_path,
    const base::FilePath& archive_path,
    const base::FilePath& install_temp_path,
    const base::FilePath& src_path,
    const base::FilePath& prefs_path,
    const installer::MasterPreferences& prefs,
    const Version& new_version);

// Performs installation-related tasks following an OS upgrade.
// |chrome| The installed product (must be a browser).
void HandleOsUpgradeForBrowser(const InstallerState& installer_state,
                               const Product& chrome);

// Performs per-user installation-related tasks on Active Setup (ran on first
// login for each user post system-level Chrome install).
// |installation_root|: The root of this install (i.e. the directory in which
// chrome.exe is installed).
// Shortcut creation is skipped if the First Run beacon is present (unless
// |force| is set to true).
// |chrome| The installed product (must be a browser).
void HandleActiveSetupForBrowser(const base::FilePath& installation_root,
                                 const Product& chrome,
                                 bool force);

}  // namespace installer

#endif  // CHROME_INSTALLER_SETUP_INSTALL_H_
