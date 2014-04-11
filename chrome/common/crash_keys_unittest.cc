// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/crash_keys.h"

#include <map>
#include <set>
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
    std::map<std::string, std::string>::const_iterator it = keys_.find(key);
    if (it == keys_.end())
      return std::string();
    return it->second;
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
    for (int i = 1; i <= 5; ++i)
      command_line.AppendSwitch(base::StringPrintf("--fewer-%d", i));
    crash_keys::SetSwitchesFromCommandLine(&command_line);
    EXPECT_EQ("--fewer-1", GetKeyValue("switch-1"));
    EXPECT_EQ("--fewer-2", GetKeyValue("switch-2"));
    EXPECT_EQ("--fewer-3", GetKeyValue("switch-3"));
    EXPECT_EQ("--fewer-4", GetKeyValue("switch-4"));
    EXPECT_EQ("--fewer-5", GetKeyValue("switch-5"));
    for (int i = 6; i < 20; ++i)
      EXPECT_FALSE(HasCrashKey(base::StringPrintf(crash_keys::kSwitch, i)));
  }
}

TEST_F(CrashKeysTest, Extensions) {
  // Set three extensions.
  {
    std::set<std::string> extensions;
    extensions.insert("ext.1");
    extensions.insert("ext.2");
    extensions.insert("ext.3");

    crash_keys::SetActiveExtensions(extensions);

    extensions.erase(GetKeyValue("extension-1"));
    extensions.erase(GetKeyValue("extension-2"));
    extensions.erase(GetKeyValue("extension-3"));
    EXPECT_EQ(0u, extensions.size());

    EXPECT_EQ("3", GetKeyValue("num-extensions"));
    EXPECT_FALSE(HasCrashKey("extension-4"));
  }

  // Set more than the max switches.
  {
    std::set<std::string> extensions;
    const int kMax = crash_keys::kExtensionIDMaxCount + 2;
    EXPECT_GT(kMax, 10);
    for (int i = 1; i <= kMax; ++i)
      extensions.insert(base::StringPrintf("ext.%d", i));
    crash_keys::SetActiveExtensions(extensions);

    for (int i = 1; i <= kMax; ++i) {
      extensions.erase(
          GetKeyValue(base::StringPrintf(crash_keys::kExtensionID, i)));
    }
    EXPECT_EQ(2u, extensions.size());

    EXPECT_EQ("12", GetKeyValue("num-extensions"));
    EXPECT_FALSE(HasCrashKey("extension-13"));
    EXPECT_FALSE(HasCrashKey("extension-14"));
  }

  // Set fewer to ensure that old ones are erased.
  {
    std::set<std::string> extensions;
    for (int i = 1; i <= 5; ++i)
      extensions.insert(base::StringPrintf("ext.%d", i));
    crash_keys::SetActiveExtensions(extensions);

    extensions.erase(GetKeyValue("extension-1"));
    extensions.erase(GetKeyValue("extension-2"));
    extensions.erase(GetKeyValue("extension-3"));
    extensions.erase(GetKeyValue("extension-4"));
    extensions.erase(GetKeyValue("extension-5"));
    EXPECT_EQ(0u, extensions.size());

    EXPECT_EQ("5", GetKeyValue("num-extensions"));
    for (int i = 6; i < 20; ++i) {
      std::string key = base::StringPrintf(crash_keys::kExtensionID, i);
      EXPECT_FALSE(HasCrashKey(key)) << key;
    }
  }
}

#if defined(OS_CHROMEOS)
TEST_F(CrashKeysTest, IgnoreBoringFlags) {
  CommandLine command_line(CommandLine::NO_PROGRAM);
  command_line.AppendSwitch("--enable-logging");
  command_line.AppendSwitch("--user-data-dir=/tmp");
  command_line.AppendSwitch("--v=1");
  command_line.AppendSwitch("--ash-default-wallpaper-small=test.png");

  command_line.AppendSwitch("--vv=1");
  command_line.AppendSwitch("--vvv");
  command_line.AppendSwitch("--enable-multi-profiles");
  command_line.AppendSwitch("--device-management-url=https://foo/bar");

  crash_keys::SetSwitchesFromCommandLine(&command_line);

  EXPECT_EQ("--vv=1", GetKeyValue("switch-1"));
  EXPECT_EQ("--vvv", GetKeyValue("switch-2"));
  EXPECT_EQ("--enable-multi-profiles", GetKeyValue("switch-3"));
  EXPECT_EQ("--device-management-url=https://foo/bar", GetKeyValue("switch-4"));
  EXPECT_FALSE(HasCrashKey("switch-5"));
}
#endif
