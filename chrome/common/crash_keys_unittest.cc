// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/crash_keys.h"

#include <map>
#include <string>

#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/debug/crash_logging.h"
#include "base/strings/string_piece.h"
#include "base/strings/stringprintf.h"
#include "testing/gtest/include/gtest/gtest.h"

class CrashKeysTest : public testing::Test {
 public:
  virtual void SetUp() OVERRIDE {
    self_ = this;
    base::debug::SetCrashKeyReportingFunctions(
        &SetCrashKeyValue, &ClearCrashKey);
    crash_keys::RegisterChromeCrashKeys();
  }

  virtual void TearDown() OVERRIDE {
    base::debug::ResetCrashLoggingForTesting();
    self_ = NULL;
  }

  bool HasCrashKey(const std::string& key) {
    return keys_.find(key) != keys_.end();
  }

  std::string GetKeyValue(const std::string& key) {
    return keys_[key];
  }

 private:
  static void SetCrashKeyValue(const base::StringPiece& key,
                               const base::StringPiece& value) {
    self_->keys_[key.as_string()] = value.as_string();
  }

  static void ClearCrashKey(const base::StringPiece& key) {
    self_->keys_.erase(key.as_string());
  }

  static CrashKeysTest* self_;

  std::map<std::string, std::string> keys_;
};

CrashKeysTest* CrashKeysTest::self_ = NULL;

TEST_F(CrashKeysTest, Switches) {
  // Set three switches.
  {
    CommandLine command_line(CommandLine::NO_PROGRAM);
    for (int i = 1; i <= 3; ++i)
      command_line.AppendSwitch(base::StringPrintf("--flag-%d", i));
    crash_keys::SetSwitchesFromCommandLine(&command_line);
    EXPECT_EQ("--flag-1", GetKeyValue("switch-1"));
    EXPECT_EQ("--flag-2", GetKeyValue("switch-2"));
    EXPECT_EQ("--flag-3", GetKeyValue("switch-3"));
    EXPECT_FALSE(HasCrashKey("switch-4"));
  }

  // Set more than the max switches.
  {
    CommandLine command_line(CommandLine::NO_PROGRAM);
    const int kMax = crash_keys::kSwitchesMaxCount + 2;
    EXPECT_GT(kMax, 15);
    for (int i = 1; i <= kMax; ++i)
      command_line.AppendSwitch(base::StringPrintf("--many-%d", i));
    crash_keys::SetSwitchesFromCommandLine(&command_line);
    EXPECT_EQ("--many-1", GetKeyValue("switch-1"));
    EXPECT_EQ("--many-9", GetKeyValue("switch-9"));
    EXPECT_EQ("--many-15", GetKeyValue("switch-15"));
    EXPECT_FALSE(HasCrashKey("switch-16"));
    EXPECT_FALSE(HasCrashKey("switch-17"));
  }

  // Set fewer to ensure that old ones are erased.
  {
    CommandLine command_line(CommandLine::NO_PROGRAM);
    const char kFormat[] = "--fewer-%d";
    for (int i = 1; i <= 5; ++i)
      command_line.AppendSwitch(base::StringPrintf(kFormat, i));
    crash_keys::SetSwitchesFromCommandLine(&command_line);
    EXPECT_EQ("--fewer-1", GetKeyValue("switch-1"));
    EXPECT_EQ("--fewer-2", GetKeyValue("switch-2"));
    EXPECT_EQ("--fewer-3", GetKeyValue("switch-3"));
    EXPECT_EQ("--fewer-4", GetKeyValue("switch-4"));
    EXPECT_EQ("--fewer-5", GetKeyValue("switch-5"));
    for (int i = 6; i < 20; ++i)
      EXPECT_FALSE(HasCrashKey(base::StringPrintf(kFormat, i)));
  }
}
