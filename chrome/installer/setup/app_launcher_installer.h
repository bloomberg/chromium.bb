// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The App Launcher is an adjunct product of Google Chrome, but it has a
// distinct registry entry. The functions in this file tap into various points
// in installer flow to update the App Launcher's registry, including the
// removal of deprecated app commands. This concentrates ugly code to to one
// place to facilitate future refactoring.

#ifndef CHROME_INSTALLER_SETUP_APP_LAUNCHER_INSTALLER_H_
#define CHROME_INSTALLER_SETUP_APP_LAUNCHER_INSTALLER_H_

#if defined(GOOGLE_CHROME_BUILD)

#include <windows.h>

namespace base {
class FilePath;
class Version;
}  // namespace base

class WorkItemList;

namespace installer {

class InstallerState;

// Create Version key for a App Launcher (if not already present) and sets the
// new product version as the last step.  We need this for App Launcher in
// order for shadow registry key to work.
void AddAppLauncherVersionKeyWorkItems(
    HKEY root,
    const base::Version& new_version,
    bool add_language_identifier,
    WorkItemList* list);

// Remove App Launcher's registry key, so it is in sync with Google Chrome's.
// Note: The key is added by App Launcher in SetDidRunForNDayActiveStats().
void RemoveAppLauncherVersionKey(HKEY reg_root);

// Adds work item to unconditionally remove legacy executables.
void AddRemoveLegacyAppHostExeWorkItems(const base::FilePath& target_path,
                                        const base::FilePath& temp_path,
                                        WorkItemList* list);

// Adds work item to unconditionally remove legacy app commands like
// "install-application", "install-extension", and
// "quick-enable-application-host".
void AddRemoveLegacyAppCommandsWorkItems(
    const InstallerState& installer_state,
    WorkItemList* work_item_list);

}  // namespace installer

#endif  // defined(GOOGLE_CHROME_BUILD)

#endif  // CHROME_INSTALLER_SETUP_APP_LAUNCHER_INSTALLER_H_
