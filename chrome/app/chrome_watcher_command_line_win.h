// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_APP_CHROME_WATCHER_COMMAND_LINE_WIN_H_
#define CHROME_APP_CHROME_WATCHER_COMMAND_LINE_WIN_H_

#include <windows.h>

#include "base/win/scoped_handle.h"

namespace base {
class CommandLine;
class FilePath;
}  // namespace base

// Generates a CommandLine that will launch |chrome_exe| in Chrome Watcher mode
// to observe |parent_process|.
base::CommandLine GenerateChromeWatcherCommandLine(
    const base::FilePath& chrome_exe,
    HANDLE parent_process);

// Interprets the Command Line used to launch a Chrome Watcher process and
// extracts the parent process HANDLE. Verifies that the handle is usable in
// this process before returning it, and returns NULL in case of a failure.
base::win::ScopedHandle InterpretChromeWatcherCommandLine(
    const base::CommandLine& command_line);

#endif  // CHROME_APP_CHROME_WATCHER_COMMAND_LINE_WIN_H_
