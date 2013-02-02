// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file contains helper functions used by setup.

#ifndef CHROME_INSTALLER_UTIL_HELPER_H_
#define CHROME_INSTALLER_UTIL_HELPER_H_

#include <string>
#include <vector>

class BrowserDistribution;

namespace base {
class FilePath;
}

namespace installer {

// This function returns the install path for Chrome depending on whether its
// system wide install or user specific install.
// system_install: if true, the function returns system wide location
//                 (ProgramFiles\Google). Otherwise it returns user specific
//                 location (Document And Settings\<user>\Local Settings...)
base::FilePath GetChromeInstallPath(bool system_install, BrowserDistribution* dist);

// Returns the path(s) to the directory that holds the user data (primary and,
// if applicable to |dist|, alternate).  This is always inside a user's local
// application data folder (e.g., "AppData\Local or "Local Settings\Application
// Data" in %USERPROFILE%). Note that these are the defaults and do not take
// into account that they can be overriden with a command line parameter.
// |paths| may be empty on return, but is guaranteed not to contain empty paths
// otherwise. If more than one path is returned, they are guaranteed to be
// siblings.
void GetChromeUserDataPaths(BrowserDistribution* dist,
                            std::vector<base::FilePath>* paths);

// Returns the distribution corresponding to the current process's binaries.
// In the case of a multi-install product, this will be the CHROME_BINARIES
// distribution.
BrowserDistribution* GetBinariesDistribution(bool system_install);

// Returns the app guid under which the current process receives updates from
// Google Update.
std::wstring GetAppGuidForUpdates(bool system_install);

}  // namespace installer

#endif  // CHROME_INSTALLER_UTIL_HELPER_H_
