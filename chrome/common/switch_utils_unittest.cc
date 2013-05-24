// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/switch_utils.h"

#include "base/basictypes.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(SwitchUtilsTest, RemoveSwitches) {
  const CommandLine::CharType* argv[] = {
    FILE_PATH_LITERAL("program"),
    FILE_PATH_LITERAL("--app=http://www.google.com/"),
    FILE_PATH_LITERAL("--force-first-run"),
    FILE_PATH_LITERAL("--make-default-browser"),
    FILE_PATH_LITERAL("--foo"),
    FILE_PATH_LITERAL("--bar")};
  CommandLine cmd_line(arraysize(argv), argv);
  EXPECT_FALSE(cmd_line.GetCommandLineString().empty());

  std::map<std::string, CommandLine::StringType> switches =
      cmd_line.GetSwitches();
  EXPECT_EQ(5U, switches.size());

  switches::RemoveSwitchesForAutostart(&switches);
  EXPECT_EQ(2U, switches.size());
  EXPECT_TRUE(cmd_line.HasSwitch("foo"));
  EXPECT_TRUE(cmd_line.HasSwitch("bar"));
}

#if defined(OS_WIN)
TEST(SwitchUtilsTest, RemoveSwitchesFromString) {
  // All these command line args (except foo and bar) will
  // be removed after RemoveSwitchesForAutostart:
  CommandLine cmd_line = CommandLine::FromString(
      L"program"
      L" --app=http://www.google.com/"
      L" --force-first-run"
      L" --make-default-browser"
      L" --foo"
      L" --bar");
  EXPECT_FALSE(cmd_line.GetCommandLineString().empty());

  std::map<std::string, CommandLine::StringType> switches =
      cmd_line.GetSwitches();
  EXPECT_EQ(5U, switches.size());

  switches::RemoveSwitchesForAutostart(&switches);
  EXPECT_EQ(2U, switches.size());
  EXPECT_TRUE(cmd_line.HasSwitch("foo"));
  EXPECT_TRUE(cmd_line.HasSwitch("bar"));
}
#endif
