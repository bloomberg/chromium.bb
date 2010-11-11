// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_INSTALLER_UTIL_LOGGING_INSTALLER_H_
#define CHROME_INSTALLER_UTIL_LOGGING_INSTALLER_H_
#pragma once

namespace installer_util {
  class MasterPreferences;
}

class FilePath;

namespace installer {

// Call to initialize logging for Chrome installer.
void InitInstallerLogging(const installer_util::MasterPreferences& prefs);

// Call when done using logging for Chrome installer.
void EndInstallerLogging();

// Returns the full path of the log file.
FilePath GetLogFilePath(const installer_util::MasterPreferences& prefs);

}  // namespace installer

#endif  // CHROME_INSTALLER_UTIL_LOGGING_INSTALLER_H_
