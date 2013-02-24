// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "chrome/common/chrome_switches.h"
#include "chrome_frame/chrome_launcher.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(ChromeLauncher, IsValidCommandLine) {
  CommandLine bad(base::FilePath(L"dummy.exe"));
  bad.AppendSwitch(switches::kNoFirstRun);  // in whitelist
  bad.AppendSwitch("no-such-switch");  // does not exist
  bad.AppendSwitch(switches::kHomePage);  // exists but not in whitelist

  EXPECT_FALSE(chrome_launcher::IsValidCommandLine(
      bad.GetCommandLineString().c_str()));

  CommandLine good(base::FilePath(L"dummy.exe"));
  good.AppendSwitch(switches::kNoFirstRun);  // in whitelist
  good.AppendSwitch(switches::kDisableBackgroundMode);  // in whitelist
  good.AppendSwitchASCII(switches::kUserDataDir, "foo");  // in whitelist

  EXPECT_TRUE(chrome_launcher::IsValidCommandLine(
      good.GetCommandLineString().c_str()));

  CommandLine no_params(base::FilePath(L"dummy.exe"));
  EXPECT_TRUE(chrome_launcher::IsValidCommandLine(
      no_params.GetCommandLineString().c_str()));

  CommandLine empty(base::FilePath(L""));
  EXPECT_TRUE(chrome_launcher::IsValidCommandLine(
      empty.GetCommandLineString().c_str()));
}

TEST(ChromeLauncher, TrimWhiteSpace) {
  std::wstring trimmed(chrome_launcher::TrimWhiteSpace(L" \t  some text \n\t"));
  EXPECT_STREQ(L"some text", trimmed.c_str());

  std::wstring now_empty(chrome_launcher::TrimWhiteSpace(L"\t\t     \n\t"));
  EXPECT_STREQ(L"", now_empty.c_str());

  std::wstring empty(chrome_launcher::TrimWhiteSpace(L""));
  EXPECT_STREQ(L"", empty.c_str());

  std::wstring not_trimmed(chrome_launcher::TrimWhiteSpace(L"foo bar"));
  EXPECT_STREQ(L"foo bar", not_trimmed.c_str());

  std::wstring trimmed_right(chrome_launcher::TrimWhiteSpace(L"foo bar\t"));
  EXPECT_STREQ(L"foo bar", trimmed_right.c_str());

  std::wstring trimmed_left(chrome_launcher::TrimWhiteSpace(L"\nfoo bar"));
  EXPECT_STREQ(L"foo bar", trimmed_right.c_str());
}

TEST(ChromeLauncher, IsValidArgument) {
  EXPECT_TRUE(chrome_launcher::IsValidArgument(L"--chrome-frame"));
  EXPECT_TRUE(chrome_launcher::IsValidArgument(L"--disable-background-mode"));
  EXPECT_FALSE(chrome_launcher::IsValidArgument(L"--invalid-arg"));

  EXPECT_TRUE(chrome_launcher::IsValidArgument(L"--chrome-frame="));
  EXPECT_TRUE(chrome_launcher::IsValidArgument(L"--chrome-frame=foo"));
  EXPECT_TRUE(chrome_launcher::IsValidArgument(L"--chrome-frame=foo=foo"));

  EXPECT_FALSE(chrome_launcher::IsValidArgument(L"chrome-frame"));
  EXPECT_FALSE(chrome_launcher::IsValidArgument(L"-chrome-frame"));
  EXPECT_FALSE(chrome_launcher::IsValidArgument(L"---chrome-frame"));
  EXPECT_FALSE(chrome_launcher::IsValidArgument(L" --chrome-frame"));
  EXPECT_FALSE(chrome_launcher::IsValidArgument(L"--chrome-framefoobar"));
  EXPECT_FALSE(chrome_launcher::IsValidArgument(L"foobar--chrome-frame"));
  EXPECT_FALSE(chrome_launcher::IsValidArgument(L"--chrome-frames"));
  EXPECT_FALSE(chrome_launcher::IsValidArgument(L"--Chrome-frame"));
  EXPECT_FALSE(chrome_launcher::IsValidArgument(L""));
}
