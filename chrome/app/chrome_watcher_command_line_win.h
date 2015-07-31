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
// to observe |parent_process|, whose main thread is identified by
// |main_thread_id|. The watcher process will signal |on_initialized_event| when
// its initialization is complete.
base::CommandLine GenerateChromeWatcherCommandLine(
    const base::FilePath& chrome_exe,
    HANDLE parent_process,
    DWORD main_thread_id,
    HANDLE on_initialized_event);

// Interprets the Command Line used to launch a Chrome Watcher process and
// extracts the parent process and initialization event HANDLEs and the parent
// process main thread ID. Verifies that the handles are usable in this process
// before returning them. Returns true if all parameters are successfully parsed
// and false otherwise. In case of partial failure, any successfully parsed
// HANDLEs will be closed.
bool InterpretChromeWatcherCommandLine(
    const base::CommandLine& command_line,
    base::win::ScopedHandle* parent_process,
    DWORD* main_thread_id,
    base::win::ScopedHandle* on_initialized_event);

#endif  // CHROME_APP_CHROME_WATCHER_COMMAND_LINE_WIN_H_
