// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROFILES_PROFILE_WINDOW_H_
#define CHROME_BROWSER_PROFILES_PROFILE_WINDOW_H_

#include "chrome/browser/ui/host_desktop.h"
#include "chrome/browser/ui/startup/startup_types.h"

class Profile;
namespace base { class FilePath; }

namespace profiles {

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
// even if a window for that profile already exists.
void SwitchToProfile(
    const base::FilePath& path,
    chrome::HostDesktopType desktop_type,
    bool always_create);

// Closes all browser windows that belong to the guest profile.
void CloseGuestProfileWindows();

}  // namespace profiles

#endif  // CHROME_BROWSER_PROFILES_PROFILE_WINDOW_H_
