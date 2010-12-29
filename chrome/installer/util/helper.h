// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file contains helper functions used by setup.

#ifndef CHROME_INSTALLER_UTIL_HELPER_H_
#define CHROME_INSTALLER_UTIL_HELPER_H_
#pragma once

class BrowserDistribution;
class CommandLine;
class FilePath;

namespace installer {

// Checks if a distribution is currently installed as part of a multi-install.
bool IsInstalledAsMulti(bool system_install, BrowserDistribution* dist);

// Retrieves the command line switches for uninstalling the distribution.
// Note that the returned CommandLine object does not include a "program".
// Only the switches should be used.
// Returns true if the product is installed and the uninstall switches
// were successfully retrieved, otherwise false.
bool GetUninstallSwitches(bool system_install, BrowserDistribution* dist,
                          CommandLine* cmd_line_switches);

// This function returns the install path for Chrome depending on whether its
// system wide install or user specific install.
// system_install: if true, the function returns system wide location
//                 (ProgramFiles\Google). Otherwise it returns user specific
//                 location (Document And Settings\<user>\Local Settings...)
FilePath GetChromeInstallPath(bool system_install, BrowserDistribution* dist);

// This function returns the path to the directory that holds the user data,
// this is always inside "Document And Settings\<user>\Local Settings.". Note
// that this is the default user data directory and does not take into account
// that it can be overriden with a command line parameter.
FilePath GetChromeUserDataPath(BrowserDistribution* dist);

// This is a workaround while we unify Chrome and Chrome Frame installation
// folders.  Right now, Chrome Frame can be installed into two different
// folders: 1) A special "Chrome Frame" folder next to Chrome's folder
// 2) The same folder as Chrome is installed into.
// Right now this function will only return Chrome's installation folder
// if Chrome Frame is not already installed or if Chrome Frame is installed
// in multi_install mode.
// If multi_install is false or if CF is installed in single mode, then the
// returned path will be the "Chrome Frame" subfolder of either the user or
// system default installation folders.
FilePath GetChromeFrameInstallPath(bool multi_install, bool system_install,
                                   BrowserDistribution* dist);

}  // namespace installer

#endif  // CHROME_INSTALLER_UTIL_HELPER_H_
