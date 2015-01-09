// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/app/chrome_watcher_command_line_win.h"

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/process/process_handle.h"
#include "base/win/scoped_handle.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(ChromeWatcherCommandLineTest, BasicTest) {
  base::ProcessHandle current = nullptr;
  ASSERT_TRUE(base::OpenProcessHandle(base::GetCurrentProcId(), &current));
  base::CommandLine cmd_line =
      GenerateChromeWatcherCommandLine(base::FilePath(L"example.exe"), current);
  base::win::ScopedHandle result = InterpretChromeWatcherCommandLine(cmd_line);
  ASSERT_EQ(current, result.Get());
}
