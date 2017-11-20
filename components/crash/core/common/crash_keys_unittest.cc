// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/crash/core/common/crash_keys.h"

#include <stddef.h>

#include <map>
#include <string>

#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/debug/crash_logging.h"
#include "base/format_macros.h"
#include "base/strings/string_piece.h"
#include "base/strings/stringprintf.h"
#include "components/crash/core/common/crash_key.h"
#include "testing/gtest/include/gtest/gtest.h"

class CrashKeysTest : public testing::Test {
 public:
  void SetUp() override {
    crash_reporter::InitializeCrashKeys();
    self_ = this;
    base::debug::SetCrashKeyReportingFunctions(
        &SetCrashKeyValue, &ClearCrashKey);
  }

  bool InitSwitchesCrashKeys() {
    std::vector<base::debug::CrashKey> keys;
    crash_keys::GetCrashKeysForCommandLineSwitches(&keys);
    return InitCrashKeys(keys);
  }

  bool InitVariationsCrashKeys() {
    std::vector<base::debug::CrashKey> keys = {
        {crash_keys::kNumVariations, crash_keys::kSmallSize},
        {crash_keys::kVariations, crash_keys::kHugeSize}};
    return InitCrashKeys(keys);
  }

  void TearDown() override {
    base::debug::ResetCrashLoggingForTesting();
    self_ = nullptr;
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
  bool InitCrashKeys(const std::vector<base::debug::CrashKey>& keys) {
    base::debug::InitCrashKeys(keys.data(), keys.size(),
                               crash_keys::kChunkMaxLength);
    return !keys.empty();
  }

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

CrashKeysTest* CrashKeysTest::self_ = nullptr;

TEST_F(CrashKeysTest, Switches) {
  ASSERT_TRUE(InitSwitchesCrashKeys());

  // Set three switches.
  {
    base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
    for (size_t i = 1; i <= 3; ++i)
      command_line.AppendSwitch(base::StringPrintf("--flag-%" PRIuS, i));
    crash_keys::SetSwitchesFromCommandLine(command_line, nullptr);
    EXPECT_EQ("--flag-1", GetKeyValue("switch-1"));
    EXPECT_EQ("--flag-2", GetKeyValue("switch-2"));
    EXPECT_EQ("--flag-3", GetKeyValue("switch-3"));
    EXPECT_FALSE(HasCrashKey("switch-4"));
  }

  // Set more than the max switches.
  {
    base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
    const size_t kMax = crash_keys::kSwitchesMaxCount + 2;
    EXPECT_GT(kMax, static_cast<size_t>(15));
    for (size_t i = 1; i <= kMax; ++i)
      command_line.AppendSwitch(base::StringPrintf("--many-%" PRIuS, i));
    crash_keys::SetSwitchesFromCommandLine(command_line, nullptr);
    EXPECT_EQ("--many-1", GetKeyValue("switch-1"));
    EXPECT_EQ("--many-9", GetKeyValue("switch-9"));
    EXPECT_EQ("--many-15", GetKeyValue("switch-15"));
    EXPECT_FALSE(HasCrashKey("switch-16"));
    EXPECT_FALSE(HasCrashKey("switch-17"));
  }

  // Set fewer to ensure that old ones are erased.
  {
    base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
    for (size_t i = 1; i <= 5; ++i)
      command_line.AppendSwitch(base::StringPrintf("--fewer-%" PRIuS, i));
    crash_keys::SetSwitchesFromCommandLine(command_line, nullptr);
    EXPECT_EQ("--fewer-1", GetKeyValue("switch-1"));
    EXPECT_EQ("--fewer-2", GetKeyValue("switch-2"));
    EXPECT_EQ("--fewer-3", GetKeyValue("switch-3"));
    EXPECT_EQ("--fewer-4", GetKeyValue("switch-4"));
    EXPECT_EQ("--fewer-5", GetKeyValue("switch-5"));
    for (size_t i = 6; i < 20; ++i)
      EXPECT_FALSE(HasCrashKey(base::StringPrintf(crash_keys::kSwitchFormat,
                                                  i)));
  }
}

namespace {

bool IsBoringFlag(const std::string& flag) {
  return flag.compare("--boring") == 0;
}

}  // namespace

TEST_F(CrashKeysTest, FilterFlags) {
  ASSERT_TRUE(InitSwitchesCrashKeys());

  using crash_keys::kSwitchesMaxCount;

  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitch("--not-boring-1");
  command_line.AppendSwitch("--boring");

  // Include the max number of non-boring switches, to make sure that only the
  // switches actually included in the crash keys are counted.
  for (size_t i = 2; i <= kSwitchesMaxCount; ++i)
    command_line.AppendSwitch(base::StringPrintf("--not-boring-%" PRIuS, i));

  crash_keys::SetSwitchesFromCommandLine(command_line, &IsBoringFlag);

  // If the boring keys are filtered out, every single key should now be
  // not-boring.
  for (size_t i = 1; i <= kSwitchesMaxCount; ++i) {
    std::string switch_name = base::StringPrintf(crash_keys::kSwitchFormat, i);
    std::string switch_value = base::StringPrintf("--not-boring-%" PRIuS, i);
    EXPECT_EQ(switch_value, GetKeyValue(switch_name)) << "switch_name is " <<
        switch_name;
  }
}

TEST_F(CrashKeysTest, VariationsCapacity) {
  ASSERT_TRUE(InitVariationsCrashKeys());

  // Variation encoding: two 32bit numbers encorded as hex with a '-' separator.
  const char kSampleVariation[] = "12345678-12345678";
  const size_t kVariationLen = std::strlen(kSampleVariation);
  const size_t kSeparatedVariationLen = kVariationLen + 1U;
  ASSERT_EQ(17U, kVariationLen);

  // The expected capacity factors in a separator (',').
  const size_t kExpectedCapacity = 112U;
  ASSERT_EQ(kExpectedCapacity,
            crash_keys::kHugeSize / (kSeparatedVariationLen));

  // Create some variations and set the crash keys.
  std::vector<std::string> variations;
  for (size_t i = 0; i < kExpectedCapacity + 2; ++i)
    variations.push_back(kSampleVariation);
  crash_keys::SetVariationsList(variations);

  // Validate crash keys.
  ASSERT_TRUE(HasCrashKey(crash_keys::kNumVariations));
  EXPECT_EQ("114", GetKeyValue(crash_keys::kNumVariations));

  const size_t kExpectedChunks = (kSeparatedVariationLen * kExpectedCapacity) /
                                 crash_keys::kChunkMaxLength;
  for (size_t i = 0; i < kExpectedChunks; ++i) {
    ASSERT_TRUE(HasCrashKey(
        base::StringPrintf("%s-%" PRIuS, crash_keys::kVariations, i + 1)));
  }
}
