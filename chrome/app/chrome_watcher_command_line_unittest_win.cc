// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/app/chrome_watcher_command_line_win.h"

#include <windows.h>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/process/process.h"
#include "base/win/scoped_handle.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(ChromeWatcherCommandLineTest, BasicTest) {
  base::Process current = base::Process::Open(base::GetCurrentProcId());
  ASSERT_TRUE(current.IsValid());
  HANDLE event = ::CreateEvent(nullptr, FALSE, FALSE, nullptr);

  base::CommandLine cmd_line = GenerateChromeWatcherCommandLine(
      base::FilePath(L"example.exe"), current.Handle(), event);

  base::win::ScopedHandle current_result;
  base::win::ScopedHandle event_result;
  ASSERT_TRUE(InterpretChromeWatcherCommandLine(cmd_line, &current_result,
                                                &event_result));
  ASSERT_EQ(current.Handle(), current_result.Get());
  ASSERT_EQ(event, event_result.Get());
}
