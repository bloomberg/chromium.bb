// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/settings/settings.h"

#include "base/command_line.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/chrome_cleaner/constants/chrome_cleaner_switches.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chrome_cleaner {

// Number high enough that it will never be a valid engine value.
constexpr int kInvalidEngineValue = 3141592;
static_assert(kInvalidEngineValue < Engine::Name_MIN ||
                  kInvalidEngineValue > Engine::Name_MAX,
              "kInvalidEngineValue is not an invalid value");

constexpr char kNonNumericValue[] = "foo";

class SettingsTest : public testing::Test {
 protected:
  Settings* ReinitializeSettings(const base::CommandLine& command_line) {
    Settings* settings = Settings::GetInstance();
    settings->Initialize(command_line, GetTargetBinary());
    return settings;
  }

  void TestExecutionMode(const std::string& switch_value,
                         ExecutionMode expected_execution_mode) {
    base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
    if (!switch_value.empty())
      command_line.AppendSwitchASCII(kExecutionModeSwitch, switch_value);

    Settings* settings = ReinitializeSettings(command_line);
    EXPECT_EQ(expected_execution_mode, settings->execution_mode());
  }
};

TEST_F(SettingsTest, EngineDefaultValue) {
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  Settings* settings = ReinitializeSettings(command_line);
  EXPECT_EQ(Engine::ESET, settings->engine());
}

TEST_F(SettingsTest, ValidEngines) {
  for (int index = Engine::Name_MIN; index < Engine::Name_MAX; ++index) {
    base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
    command_line.AppendSwitchASCII(chrome_cleaner::kEngineSwitch,
                                   base::NumberToString(index));
    Settings* settings = ReinitializeSettings(command_line);
    if (index != Engine::UNKNOWN && index != Engine::DEPRECATED_URZA)
      EXPECT_EQ(static_cast<Engine::Name>(index), settings->engine());
    else
      EXPECT_EQ(Engine::ESET, settings->engine());  // Fallback to default.
  }
}

TEST_F(SettingsTest, EngineInvalidNumericValue) {
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitchASCII(chrome_cleaner::kEngineSwitch,
                                 base::NumberToString(kInvalidEngineValue));
  Settings* settings = ReinitializeSettings(command_line);
  EXPECT_EQ(Engine::ESET, settings->engine());
}

TEST_F(SettingsTest, EngineNonNumericValue) {
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitchASCII(chrome_cleaner::kEngineSwitch,
                                 kNonNumericValue);
  Settings* settings = ReinitializeSettings(command_line);
  EXPECT_EQ(Engine::ESET, settings->engine());
}

TEST_F(SettingsTest, CleanerRunId_Generated) {
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  Settings* settings = ReinitializeSettings(command_line);
  // A non-empty cleanup id should be generated if switch --cleanup-id is not
  // present.
  EXPECT_FALSE(settings->cleanup_id().empty());

  command_line.AppendSwitchASCII(chrome_cleaner::kCleanupIdSwitch,
                                 std::string());
  settings = ReinitializeSettings(command_line);
  // A non-empty cleanup id should be generated if switch --cleanup-id is
  // present, but the corresponding value is empty.
  EXPECT_FALSE(settings->cleanup_id().empty());
}

TEST_F(SettingsTest, CleanerRunId_Propagated) {
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  const std::string propagated_cleanup_id = "propagated-run-id";
  command_line.AppendSwitchASCII(chrome_cleaner::kCleanupIdSwitch,
                                 propagated_cleanup_id);
  Settings* settings = ReinitializeSettings(command_line);
  EXPECT_EQ(propagated_cleanup_id, settings->cleanup_id());
}

TEST_F(SettingsTest, SBEREnabled) {
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  EXPECT_FALSE(ReinitializeSettings(command_line)->sber_enabled());

  command_line.AppendSwitch(chrome_cleaner::kExtendedSafeBrowsingEnabledSwitch);
  EXPECT_TRUE(ReinitializeSettings(command_line)->sber_enabled());
}

TEST_F(SettingsTest, ExecutionMode) {
  const struct TestCase {
    std::string switch_value;
    ExecutionMode expected_execution_mode;
  } test_cases[] = {
      // Switch not present.
      {"", ExecutionMode::kNone},
      // Valid values for execution mode.
      {base::NumberToString(static_cast<int>(ExecutionMode::kScanning)),
       ExecutionMode::kScanning},
      {base::NumberToString(static_cast<int>(ExecutionMode::kCleanup)),
       ExecutionMode::kCleanup},
      // Unknown values for execution mode.
      {base::NumberToString(static_cast<int>(ExecutionMode::kNumValues)),
       ExecutionMode::kNone},
      {base::NumberToString(static_cast<int>(ExecutionMode::kNumValues) + 1),
       ExecutionMode::kNone},
      {base::NumberToString(static_cast<int>(ExecutionMode::kNone) - 1),
       ExecutionMode::kNone},
      {"invalid-value", ExecutionMode::kNone},
  };

  for (auto t : test_cases) {
    TestExecutionMode(t.switch_value, t.expected_execution_mode);
  }
}

TEST_F(SettingsTest, Timeouts) {
  using OverrideAccessor = bool (Settings::*)() const;
  using TimeoutAccessor = base::TimeDelta (Settings::*)() const;

  const struct TestCase {
    std::string switch_value;
    bool expected_timeout_overridden;
    base::TimeDelta expected_timeout;
  } test_cases[] = {
      // Switch not present.
      {"", false, base::TimeDelta()},
      // Valid number of minutes.
      {"100", true, base::TimeDelta::FromMinutes(100)},
      // Disabled.
      {"0", true, base::TimeDelta()},
      // Invalid values.
      {"ten", false, base::TimeDelta()},
      {"1.5", false, base::TimeDelta()},
      {"-3", false, base::TimeDelta()},
  };

  const struct SwitchesToTest {
    std::string switch_name;
    OverrideAccessor override_accessor;
    TimeoutAccessor timeout_accessor;
  } switches_to_test[] = {
      {kCleaningTimeoutMinutesSwitch, &Settings::cleaning_timeout_overridden,
       &Settings::cleaning_timeout},
      {kScanningTimeoutMinutesSwitch, &Settings::scanning_timeout_overridden,
       &Settings::scanning_timeout},
  };

  for (auto s : switches_to_test) {
    SCOPED_TRACE(s.switch_name);
    for (auto t : test_cases) {
      SCOPED_TRACE(t.switch_value);

      base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
      if (!t.switch_value.empty())
        command_line.AppendSwitchASCII(s.switch_name, t.switch_value);

      Settings* settings = ReinitializeSettings(command_line);

      bool timeout_overridden = (settings->*(s.override_accessor))();
      base::TimeDelta timeout = (settings->*(s.timeout_accessor))();

      EXPECT_EQ(t.expected_timeout_overridden, timeout_overridden);
      EXPECT_EQ(t.expected_timeout, timeout);
    }
  }
}

TEST_F(SettingsTest, NoLocationsToScanSpecified) {
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  Settings* settings = ReinitializeSettings(command_line);
  // All valid scan locations should be specified, but let's not check the exact
  // contents and size. It's bigger than 1 location for sure.
  EXPECT_LT(1u, settings->locations_to_scan().size());
  EXPECT_TRUE(settings->scan_switches_correct());
}

TEST_F(SettingsTest, LimitsToSpecifiedLocations) {
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitchASCII(kScanLocationsSwitch, "1,2,3");
  std::vector<UwS::TraceLocation> expected_locations = {
      UwS::FOUND_IN_STARTUP, UwS::FOUND_IN_MEMORY,
      UwS::FOUND_IN_UNINSTALLSTRING,
  };

  Settings* settings = ReinitializeSettings(command_line);
  EXPECT_EQ(expected_locations, settings->locations_to_scan());
  EXPECT_TRUE(settings->scan_switches_correct());
}

TEST_F(SettingsTest, UsesOnlyValidSpecifiedLocations) {
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitchASCII(kScanLocationsSwitch, "1,100,yada,-3");
  std::vector<UwS::TraceLocation> expected_locations = {UwS::FOUND_IN_STARTUP};

  Settings* settings = ReinitializeSettings(command_line);
  EXPECT_EQ(expected_locations, settings->locations_to_scan());
  EXPECT_FALSE(settings->scan_switches_correct());
}

TEST_F(SettingsTest, OnlyInvalidLocationsSpecified) {
  const std::string test_cases[] = {
      "",            // empty flag value
      "100,102,-3",  // only invalid values specified
  };
  for (const std::string& switch_value : test_cases) {
    base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
    command_line.AppendSwitchASCII(kScanLocationsSwitch, switch_value);
    Settings* settings = ReinitializeSettings(command_line);
    // All valid scan locations should be specified.
    EXPECT_LT(1u, settings->locations_to_scan().size());
    EXPECT_FALSE(settings->scan_switches_correct());
  }
}

}  // namespace chrome_cleaner
