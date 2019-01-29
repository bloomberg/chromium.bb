// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/process/launch.h"
#include "base/process/process.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

// Tests the updater process returns 0 when run with no arguments.
TEST(UpdaterTest, UpdaterExitCode) {
  base::FilePath::CharType buffer[MAX_PATH] = {0};
  ASSERT_NE(0u, GetModuleFileName(0, buffer, base::size(buffer)));
  const base::FilePath updater =
      base::FilePath(buffer).DirName().Append(FILE_PATH_LITERAL("updater.exe"));
  base::LaunchOptions options;
  options.start_hidden = true;
  auto process = base::LaunchProcess(base::CommandLine(updater), options);
  ASSERT_TRUE(process.IsValid());
  int exit_code = -1;
  EXPECT_TRUE(process.WaitForExitWithTimeout(base::TimeDelta::FromSeconds(60),
                                             &exit_code));
  EXPECT_EQ(0, exit_code);
}
