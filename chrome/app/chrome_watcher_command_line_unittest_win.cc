// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/app/chrome_watcher_command_line_win.h"

#include <windows.h>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/process/process_handle.h"
#include "base/win/scoped_handle.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(ChromeWatcherCommandLineTest, BasicTest) {
  // Ownership of these handles is passed to the ScopedHandles below via
  // InterpretChromeWatcherCommandLine().
  base::ProcessHandle current =
      ::OpenProcess(PROCESS_QUERY_INFORMATION | SYNCHRONIZE,
                    TRUE,  // Inheritable
                    ::GetCurrentProcessId());
  ASSERT_NE(nullptr, current);

  HANDLE event = ::CreateEvent(nullptr, FALSE, FALSE, nullptr);
  ASSERT_NE(nullptr, event);

  base::CommandLine cmd_line = GenerateChromeWatcherCommandLine(
      base::FilePath(L"example.exe"), current, event);

  base::win::ScopedHandle current_result;
  base::win::ScopedHandle event_result;
  ASSERT_TRUE(InterpretChromeWatcherCommandLine(cmd_line, &current_result,
                                                &event_result));
  ASSERT_EQ(current, current_result.Get());
  ASSERT_EQ(event, event_result.Get());
}
