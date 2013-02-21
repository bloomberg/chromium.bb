// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/values.h"
#include "chrome/test/chromedriver/chrome_desktop_impl.h"
#include "chrome/test/chromedriver/status.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(ProcessCommandLineArgs, NoArgs) {
  CommandLine command(CommandLine::NO_PROGRAM);
  base::ListValue switches;
  ASSERT_TRUE(switches.empty());
  Status status = internal::ProcessCommandLineArgs(&switches, &command);
  ASSERT_TRUE(status.IsOk());
  ASSERT_TRUE(command.GetSwitches().empty());
}

TEST(ProcessCommandLineArgs, SingleArgWithoutValue) {
  CommandLine command(CommandLine::NO_PROGRAM);
  base::ListValue switches;
  switches.AppendString("enable-nacl");
  ASSERT_EQ(1u, switches.GetSize());
  Status status = internal::ProcessCommandLineArgs(&switches, &command);
  ASSERT_TRUE(status.IsOk());
  ASSERT_EQ(1u, command.GetSwitches().size());
  ASSERT_TRUE(command.HasSwitch("enable-nacl"));
}

TEST(ProcessCommandLineArgs, SingleArgWithValue) {
  CommandLine command(CommandLine::NO_PROGRAM);
  base::ListValue switches;
  switches.AppendString("load-extension=/test/extension");
  ASSERT_EQ(1u, switches.GetSize());
  Status status = internal::ProcessCommandLineArgs(&switches, &command);
  ASSERT_TRUE(status.IsOk());
  ASSERT_EQ(1u, command.GetSwitches().size());
  ASSERT_TRUE(command.HasSwitch("load-extension"));
  ASSERT_EQ("/test/extension", command.GetSwitchValueASCII("load-extension"));
}

TEST(ProcessCommandLineArgs, MultipleArgs) {
  CommandLine command(CommandLine::NO_PROGRAM);
  base::ListValue switches;
  switches.AppendString("disable-sync");
  switches.AppendString("user-data-dir=/test/user/data");
  ASSERT_EQ(2u, switches.GetSize());
  Status status = internal::ProcessCommandLineArgs(&switches, &command);
  ASSERT_TRUE(status.IsOk());
  ASSERT_EQ(2u, command.GetSwitches().size());
  ASSERT_TRUE(command.HasSwitch("disable-sync"));
  ASSERT_TRUE(command.HasSwitch("user-data-dir"));
  ASSERT_EQ("/test/user/data", command.GetSwitchValueASCII("user-data-dir"));
}
