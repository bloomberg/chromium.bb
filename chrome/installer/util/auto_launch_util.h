// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_INSTALLER_UTIL_AUTO_LAUNCH_UTIL_H_
#define CHROME_INSTALLER_UTIL_AUTO_LAUNCH_UTIL_H_

#include "base/strings/string16.h"

namespace base {
class FilePath;
}

// A namespace containing the platform specific implementation of setting Chrome
// to launch at user login.
namespace auto_launch_util {

// Returns whether the Chrome executable in the directory |application_path| is
// set to auto-start at user login.
// |profile_directory| is the name of the directory (leaf, not the full path)
// that contains the profile that should be opened at user login.
// There are two flavors of auto-start, separated by whether a window is
// requested at startup or not (called background mode). |window_requested|
// specifies which flavor the caller is interested in.
// NOTE: This function returns true only if the flavor the caller specifies has
// been requested (or if both flavors have been requested). Therefore, it is
// possible that Chrome auto-starts even if this function returns false (since
// the other flavor might have been requested).
// NOTE: Chrome being set to launch in the background (without a window)
// does not necessarily mean that Chrome *will* launch without a window, since
// when both flavors of the auto-start feature have been requested. In other
// words, showing a window trumps not showing a window.
// ALSO NOTE: |application_path| is optional and should be blank in most cases
// (as it will default to the application path of the current executable).
bool AutoStartRequested(const base::string16& profile_directory,
                        bool window_requested,
                        const base::FilePath& application_path);

// Disables all auto-start features. |profile_directory| is the name of the
// directory (leaf, not the full path) that contains the profile that was set
// to be opened at user login.
void DisableAllAutoStartFeatures(const base::string16& profile_directory);

// Configures Chrome to auto-launch at user login and show a window. See also
// EnableBackgroundStartAtLogin, which does the same, except without a window.
// |profile_directory| is the name of the directory (leaf, not the full path)
// that contains the profile that should be opened at user login.
// |application_path| is needed when the caller is not the process being set to
// auto-launch, ie. the installer. This is because |application_path|, if left
// blank, defaults to the application path of the current executable.
void EnableForegroundStartAtLogin(const base::string16& profile_directory,
                                  const base::FilePath& application_path);

// Disables auto-starting Chrome in foreground mode at user login.
// |profile_directory| is the name of the directory (leaf, not the full path)
// that contains the profile that was set to be opened at user login.
// NOTE: Chrome may still launch if the other auto-start flavor is active
// (background mode).
void DisableForegroundStartAtLogin(const base::string16& profile_directory);

// Requests that Chrome start in Background Mode at user login (without a
// window being shown, except if EnableForegroundStartAtLogin has also been
// called).
// In short, EnableBackgroundStartAtLogin is the no-window version of calling
// EnableForegroundStartAtLogin). If both are called, a window will be shown on
// startup (foreground mode wins).
void EnableBackgroundStartAtLogin();

// Disables auto-starting Chrome in background mode at user login. Chrome may
// still launch if the other auto-start flavor is active (foreground mode).
void DisableBackgroundStartAtLogin();

}  // namespace auto_launch_util

#endif  // CHROME_INSTALLER_UTIL_AUTO_LAUNCH_UTIL_H_
