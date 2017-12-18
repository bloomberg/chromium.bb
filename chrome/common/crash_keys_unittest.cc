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
#include "build/build_config.h"
#include "components/crash/core/common/crash_key.h"
#include "testing/gtest/include/gtest/gtest.h"

using crash_reporter::GetCrashKeyValue;

class CrashKeysTest : public testing::Test {
 public:
  void SetUp() override {
    crash_reporter::ResetCrashKeysForTesting();
    crash_reporter::InitializeCrashKeys();
    self_ = this;
    base::debug::SetCrashKeyReportingFunctions(
        &SetCrashKeyValue, &ClearCrashKey);
    crash_keys::RegisterChromeCrashKeys();
  }

  void TearDown() override {
    base::debug::ResetCrashLoggingForTesting();
    crash_reporter::ResetCrashKeysForTesting();
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

TEST_F(CrashKeysTest, IgnoreBoringFlags) {
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitch("--enable-logging");
  command_line.AppendSwitch("--v=1");

  command_line.AppendSwitch("--vv=1");
  command_line.AppendSwitch("--vvv");
  command_line.AppendSwitch("--enable-multi-profiles");
  command_line.AppendSwitch("--device-management-url=https://foo/bar");
#if defined(OS_CHROMEOS)
  command_line.AppendSwitch("--user-data-dir=/tmp");
  command_line.AppendSwitch("--default-wallpaper-small=test.png");
#endif

  crash_keys::SetCrashKeysFromCommandLine(command_line);

  EXPECT_EQ("--vv=1", GetCrashKeyValue("switch-1"));
  EXPECT_EQ("--vvv", GetCrashKeyValue("switch-2"));
  EXPECT_EQ("--enable-multi-profiles", GetCrashKeyValue("switch-3"));
  EXPECT_EQ("--device-management-url=https://foo/bar",
            GetCrashKeyValue("switch-4"));
  EXPECT_TRUE(GetCrashKeyValue("switch-5").empty());
}
