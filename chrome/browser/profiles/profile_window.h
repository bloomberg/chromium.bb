// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROFILES_PROFILE_WINDOW_H_
#define CHROME_BROWSER_PROFILES_PROFILE_WINDOW_H_

#include "base/callback_forward.h"
#include "chrome/browser/ui/host_desktop.h"
#include "chrome/browser/ui/startup/startup_types.h"

class Profile;
namespace base { class FilePath; }

namespace profiles {

// Callback to be used when switching to a new profile is completed.
typedef base::Callback<void()> ProfileSwitchingDoneCallback;

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

void SwitchToProfile(
    const base::FilePath& path,
    chrome::HostDesktopType desktop_type,
    bool always_create,
    ProfileSwitchingDoneCallback callback);

// Opens a Browser for the guest profile and runs |callback| if it isn't null.
void SwitchToGuestProfile(chrome::HostDesktopType desktop_type,
                          ProfileSwitchingDoneCallback callback);

// Creates a new profile from the next available profile directory, and
// opens a new browser window for the profile once it is ready. When the browser
// is opened, |callback| will be run if it isn't null.
void CreateAndSwitchToNewProfile(chrome::HostDesktopType desktop_type,
                                 ProfileSwitchingDoneCallback callback);

// Closes all browser windows that belong to the guest profile.
void CloseGuestProfileWindows();

}  // namespace profiles

#endif  // CHROME_BROWSER_PROFILES_PROFILE_WINDOW_H_
