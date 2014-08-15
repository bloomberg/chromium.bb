// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROFILES_PROFILE_WINDOW_H_
#define CHROME_BROWSER_PROFILES_PROFILE_WINDOW_H_

#include "base/callback_forward.h"
#include "chrome/browser/profiles/profile_metrics.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/host_desktop.h"
#include "chrome/browser/ui/profile_chooser_constants.h"
#include "chrome/browser/ui/startup/startup_types.h"

class Profile;
namespace base { class FilePath; }

namespace profiles {

// Callback to be used when switching to a new profile is completed.
typedef base::Callback<void()> ProfileSwitchingDoneCallback;

// Different tutorials that can be displayed in the user manager.
enum UserManagerTutorialMode {
  USER_MANAGER_NO_TUTORIAL,        // Does not display a tutorial.
  USER_MANAGER_TUTORIAL_OVERVIEW,  // Basic overview of new features.
  USER_MANAGER_TUTORIAL_LOCK,      // TODO(noms): To be implemented.
};

// Activates a window for |profile| on the desktop specified by
// |desktop_type|. If no such window yet exists, or if |always_create| is
// true, this first creates a new window, then activates
// that. If activating an exiting window and multiple windows exists then the
// window that was most recently active is activated. This is used for
// creation of a window from the multi-profile dropdown menu.
void FindOrCreateNewWindowForProfile(
    Profile* profile,
    chrome::startup::IsProcessStartup process_startup,
    chrome::startup::IsFirstRun is_first_run,
    chrome::HostDesktopType desktop_type,
    bool always_create);

// Opens a Browser with the specified profile given by |path|.
// If |always_create| is true then a new window is created
// even if a window for that profile already exists. When the browser is
// opened, |callback| will be run if it isn't null.

void SwitchToProfile(const base::FilePath& path,
                     chrome::HostDesktopType desktop_type,
                     bool always_create,
                     ProfileSwitchingDoneCallback callback,
                     ProfileMetrics::ProfileOpen metric);

// Opens a Browser for the guest profile and runs |callback| if it isn't null.
void SwitchToGuestProfile(chrome::HostDesktopType desktop_type,
                          ProfileSwitchingDoneCallback callback);

// Creates a new profile from the next available profile directory, and
// opens a new browser window for the profile once it is ready. When the browser
// is opened, |callback| will be run if it isn't null.
void CreateAndSwitchToNewProfile(chrome::HostDesktopType desktop_type,
                                 ProfileSwitchingDoneCallback callback,
                                 ProfileMetrics::ProfileAdd metric);

// Closes all browser windows that belong to the guest profile.
void CloseGuestProfileWindows();

// Closes all the browser windows for |profile| and opens the user manager.
void LockProfile(Profile* profile);

// Creates or reuses the guest profile needed by the user manager. Based on
// the value of |tutorial_mode|, the user manager can show a specific
// tutorial, or no tutorial at all. If a tutorial is not shown, then
// |profile_path_to_focus| could be used to specify which user should be
// focused. |callback| is run with the custom url to be displayed, as well as
// a pointer to the guest profile.
void CreateGuestProfileForUserManager(
    const base::FilePath& profile_path_to_focus,
    profiles::UserManagerTutorialMode tutorial_mode,
    const base::Callback<void(Profile*, const std::string&)>& callback);

// Based on the |profile| preferences, determines whether a user manager
// tutorial needs to be shown, and displays the user manager with or without
// the tutorial.
void ShowUserManagerMaybeWithTutorial(Profile* profile);

// Enables new profile management preview and shows the user manager tutorial.
void EnableNewProfileManagementPreview(Profile* profile);

// Disables new profile management preview and attempts to relaunch Chrome.
void DisableNewProfileManagementPreview(Profile* profile);

// Converts from modes in the avatar menu to modes understood by
// ProfileChooserView.
void BubbleViewModeFromAvatarBubbleMode(
    BrowserWindow::AvatarBubbleMode mode,
    BubbleViewMode* bubble_view_mode,
    TutorialMode* tutorial_mode);

}  // namespace profiles

#endif  // CHROME_BROWSER_PROFILES_PROFILE_WINDOW_H_
