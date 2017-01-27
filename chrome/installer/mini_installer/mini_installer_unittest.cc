// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/mini_installer/mini_installer.h"

#include "chrome/installer/mini_installer/configuration.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mini_installer {

TEST(MiniInstallerTest, AppendCommandLineFlags) {
  static constexpr struct {
    const wchar_t* command_line;
    const wchar_t* args;
  } kData[] = {
      {L"", L"foo.exe"},
      {L"mini_installer.exe", L"foo.exe"},
      {L"mini_installer.exe --verbose-logging", L"foo.exe --verbose-logging"},
      {L"C:\\Temp\\mini_installer.exe --verbose-logging",
       L"foo.exe --verbose-logging"},
      {L"C:\\Temp\\mini_installer --verbose-logging",
       L"foo.exe --verbose-logging"},
      {L"\"C:\\Temp\\mini_installer (1).exe\" --verbose-logging",
       L"foo.exe --verbose-logging"},
      {L"\"mini_installer.exe\"--verbose-logging",
       L"foo.exe --verbose-logging"},
  };

  CommandString buffer;

  for (const auto& data : kData) {
    buffer.assign(L"foo.exe");
    AppendCommandLineFlags(data.command_line, &buffer);
    EXPECT_STREQ(data.args, buffer.get()) << data.command_line;
  }
}

}  // namespace mini_installer
