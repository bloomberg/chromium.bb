// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/command_line_helper.h"

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "content/public/common/content_switches.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::Test;
using base::CommandLine;

const base::Feature kSomeSpecialFeature{"SomeSpecialFeature",
                                        base::FEATURE_DISABLED_BY_DEFAULT};

class CommandLineHelperTest : public Test {
 public:
  CommandLineHelperTest() {}

  void AddFeatureAndVerify(CommandLine& command_line,
                           const std::string& enabled_expected,
                           const std::string& disabled_expected) {
    CommandLineHelper::AddEnabledFeature(command_line,
                                         kSomeSpecialFeature.name);
    EXPECT_EQ(enabled_expected,
              command_line.GetSwitchValueASCII(switches::kEnableFeatures));
    EXPECT_EQ(disabled_expected,
              command_line.GetSwitchValueASCII(switches::kDisableFeatures));
  }
};

TEST_F(CommandLineHelperTest, AddWithEmptyCommandLine) {
  CommandLine command_line(CommandLine::NO_PROGRAM);
  AddFeatureAndVerify(command_line, "SomeSpecialFeature", "");
}

TEST_F(CommandLineHelperTest, AddWithNoEnabledFeatures) {
  const CommandLine::CharType* argv[] = {FILE_PATH_LITERAL("program")};
  CommandLine command_line(arraysize(argv), argv);
  AddFeatureAndVerify(command_line, "SomeSpecialFeature", "");
}

TEST_F(CommandLineHelperTest, AddWithEnabledTestFeature) {
  const CommandLine::CharType* argv[] = {
      FILE_PATH_LITERAL("program"),
      FILE_PATH_LITERAL("--enable-features=TestFeature")};
  CommandLine command_line(arraysize(argv), argv);
  AddFeatureAndVerify(command_line, "TestFeature,SomeSpecialFeature", "");
}

TEST_F(CommandLineHelperTest, AddWithEnabledSomeSpecialFeature) {
  const CommandLine::CharType* argv[] = {
      FILE_PATH_LITERAL("program"),
      FILE_PATH_LITERAL("--enable-features=SomeSpecialFeature,TestFeature")};
  CommandLine command_line(arraysize(argv), argv);
  AddFeatureAndVerify(command_line, "SomeSpecialFeature,TestFeature", "");
}

TEST_F(CommandLineHelperTest, AddWithDisabledSomeSpecialFeature) {
  const CommandLine::CharType* argv[] = {
      FILE_PATH_LITERAL("program"),
      FILE_PATH_LITERAL("--disable-features=SomeSpecialFeature")};
  CommandLine command_line(arraysize(argv), argv);
  AddFeatureAndVerify(command_line, "", "SomeSpecialFeature");
}

TEST_F(CommandLineHelperTest, AddWithDisabledTestFeature) {
  const CommandLine::CharType* argv[] = {
      FILE_PATH_LITERAL("program"),
      FILE_PATH_LITERAL("--disable-features=TestFeature")};
  CommandLine command_line(arraysize(argv), argv);
  AddFeatureAndVerify(command_line, "SomeSpecialFeature", "TestFeature");
}
