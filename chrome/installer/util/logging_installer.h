// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_INSTALLER_UTIL_LOGGING_INSTALLER_H_
#define CHROME_INSTALLER_UTIL_LOGGING_INSTALLER_H_
#pragma once

class CommandLine;
class FilePath;

namespace installer {

// Call to initialize logging for Chrome installer.
void InitInstallerLogging(const CommandLine& command_line);

// Call when done using logging for Chrome installer.
void EndInstallerLogging();

// Returns the full path of the log file.
FilePath GetLogFilePath(const CommandLine& command_line);

}  // namespace installer

#endif  // CHROME_INSTALLER_UTIL_LOGGING_INSTALLER_H_
