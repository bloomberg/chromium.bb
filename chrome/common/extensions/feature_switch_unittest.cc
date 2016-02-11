// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/feature_switch.h"

#include "base/command_line.h"
#include "base/memory/scoped_ptr.h"
#include "base/metrics/field_trial.h"
#include "testing/gtest/include/gtest/gtest.h"

using extensions::FeatureSwitch;

namespace {

const char kSwitchName[] = "test-switch";
const char kFieldTrialName[] = "field-trial";

// Create and register a field trial named |field_trial_name| that will always
// return the given |group_name|.
scoped_refptr<base::FieldTrial> CreateFieldTrialWithName(
    const std::string& field_trial_name,
    const std::string& group_name) {
  const int kTotalProbability = 10;
  // Note: This code will probably fail in the year 5000. But all the cycles we
  // save in the next 3000 years by not determining the current year will be
  // worth it.
  scoped_refptr<base::FieldTrial> trial =
      base::FieldTrialList::FactoryGetFieldTrial(
          field_trial_name, kTotalProbability, "default", 5000, 1, 1,
          base::FieldTrial::SESSION_RANDOMIZED, nullptr);
  trial->AppendGroup(group_name, kTotalProbability);
  return trial;
}

// Create and register a field trial that will always return the given
// |group_name|.
scoped_refptr<base::FieldTrial> CreateFieldTrial(
    const std::string& group_name) {
  return CreateFieldTrialWithName(kFieldTrialName, group_name);
}

template<FeatureSwitch::DefaultValue T>
class FeatureSwitchTest : public testing::Test {
 public:
  FeatureSwitchTest()
      : command_line_(base::CommandLine::NO_PROGRAM),
        feature_(&command_line_, kSwitchName, T) {}
 protected:
  base::CommandLine command_line_;
  FeatureSwitch feature_;
};

typedef FeatureSwitchTest<FeatureSwitch::DEFAULT_DISABLED>
    FeatureSwitchDisabledTest;
typedef FeatureSwitchTest<FeatureSwitch::DEFAULT_ENABLED>
    FeatureSwitchEnabledTest;

}  // namespace

TEST_F(FeatureSwitchDisabledTest, NoSwitchValue) {
  EXPECT_FALSE(feature_.IsEnabled());
}

TEST_F(FeatureSwitchDisabledTest, FalseSwitchValue) {
  command_line_.AppendSwitchASCII(kSwitchName, "0");
  EXPECT_FALSE(feature_.IsEnabled());
}

TEST_F(FeatureSwitchDisabledTest, GibberishSwitchValue) {
  command_line_.AppendSwitchASCII(kSwitchName, "monkey");
  EXPECT_FALSE(feature_.IsEnabled());
}

TEST_F(FeatureSwitchDisabledTest, Override) {
  {
    FeatureSwitch::ScopedOverride override(&feature_, false);
    EXPECT_FALSE(feature_.IsEnabled());
  }
  EXPECT_FALSE(feature_.IsEnabled());

  {
    FeatureSwitch::ScopedOverride override(&feature_, true);
    EXPECT_TRUE(feature_.IsEnabled());
  }
  EXPECT_FALSE(feature_.IsEnabled());
}

TEST_F(FeatureSwitchDisabledTest, TrueSwitchValue) {
  command_line_.AppendSwitchASCII(kSwitchName, "1");
  EXPECT_TRUE(feature_.IsEnabled());

  {
    FeatureSwitch::ScopedOverride override(&feature_, false);
    EXPECT_FALSE(feature_.IsEnabled());
  }
  EXPECT_TRUE(feature_.IsEnabled());

  {
    FeatureSwitch::ScopedOverride override(&feature_, true);
    EXPECT_TRUE(feature_.IsEnabled());
  }
  EXPECT_TRUE(feature_.IsEnabled());
}

TEST_F(FeatureSwitchDisabledTest, TrimSwitchValue) {
  command_line_.AppendSwitchASCII(kSwitchName, " \t  1\n  ");
  EXPECT_TRUE(feature_.IsEnabled());
}

TEST_F(FeatureSwitchEnabledTest, NoSwitchValue) {
  EXPECT_TRUE(feature_.IsEnabled());
}

TEST_F(FeatureSwitchEnabledTest, TrueSwitchValue) {
  command_line_.AppendSwitchASCII(kSwitchName, "1");
  EXPECT_TRUE(feature_.IsEnabled());
}

TEST_F(FeatureSwitchEnabledTest, GibberishSwitchValue) {
  command_line_.AppendSwitchASCII(kSwitchName, "monkey");
  EXPECT_TRUE(feature_.IsEnabled());
}

TEST_F(FeatureSwitchEnabledTest, Override) {
  {
    FeatureSwitch::ScopedOverride override(&feature_, true);
    EXPECT_TRUE(feature_.IsEnabled());
  }
  EXPECT_TRUE(feature_.IsEnabled());

  {
    FeatureSwitch::ScopedOverride override(&feature_, false);
    EXPECT_FALSE(feature_.IsEnabled());
  }
  EXPECT_TRUE(feature_.IsEnabled());
}

TEST_F(FeatureSwitchEnabledTest, FalseSwitchValue) {
  command_line_.AppendSwitchASCII(kSwitchName, "0");
  EXPECT_FALSE(feature_.IsEnabled());

  {
    FeatureSwitch::ScopedOverride override(&feature_, true);
    EXPECT_TRUE(feature_.IsEnabled());
  }
  EXPECT_FALSE(feature_.IsEnabled());

  {
    FeatureSwitch::ScopedOverride override(&feature_, false);
    EXPECT_FALSE(feature_.IsEnabled());
  }
  EXPECT_FALSE(feature_.IsEnabled());
}

TEST_F(FeatureSwitchEnabledTest, TrimSwitchValue) {
  command_line_.AppendSwitchASCII(kSwitchName, "\t\t 0 \n");
  EXPECT_FALSE(feature_.IsEnabled());
}

TEST_F(FeatureSwitchEnabledTest, TrueFieldTrialValue) {
  // Construct a fake field trial that defaults to the group "Enabled".
  base::FieldTrialList field_trials(nullptr);
  scoped_refptr<base::FieldTrial> enabled_trial = CreateFieldTrial("Enabled");
  {
    // A default-enabled switch should be enabled (naturally).
    FeatureSwitch default_enabled_switch(
        &command_line_, kSwitchName,
        std::vector<std::string>(1, kFieldTrialName),
        FeatureSwitch::DEFAULT_ENABLED);
    EXPECT_TRUE(default_enabled_switch.IsEnabled());
    // Scoped overrides override everything.
    FeatureSwitch::ScopedOverride scoped_override(&default_enabled_switch,
                                                  false);
    EXPECT_FALSE(default_enabled_switch.IsEnabled());
  }

  {
    // A default-disabled switch should be enabled because of the field trial.
    FeatureSwitch default_disabled_switch(
        &command_line_, kSwitchName,
        std::vector<std::string>(1, kFieldTrialName),
        FeatureSwitch::DEFAULT_DISABLED);
    EXPECT_TRUE(default_disabled_switch.IsEnabled());
    // Scoped overrides override everything.
    FeatureSwitch::ScopedOverride scoped_override(&default_disabled_switch,
                                                  false);
    EXPECT_FALSE(default_disabled_switch.IsEnabled());
  }
}

TEST_F(FeatureSwitchEnabledTest, FalseFieldTrialValue) {
  // Construct a fake field trial that defaults to the group "Disabled".
  base::FieldTrialList field_trials(nullptr);
  scoped_refptr<base::FieldTrial> disabled_trial = CreateFieldTrial("Disabled");
  {
    // A default-enabled switch should be disabled because of the field trial.
    FeatureSwitch default_enabled_switch(
        &command_line_, kSwitchName,
        std::vector<std::string>(1, kFieldTrialName),
        FeatureSwitch::DEFAULT_ENABLED);
    EXPECT_FALSE(default_enabled_switch.IsEnabled());
    // Scoped overrides override everything.
    FeatureSwitch::ScopedOverride scoped_override(&default_enabled_switch,
                                                  true);
    EXPECT_TRUE(default_enabled_switch.IsEnabled());
  }

  {
    // A default-disabled switch should remain disabled.
    FeatureSwitch default_disabled_switch(
        &command_line_, kSwitchName,
        std::vector<std::string>(1, kFieldTrialName),
        FeatureSwitch::DEFAULT_DISABLED);
    EXPECT_FALSE(default_disabled_switch.IsEnabled());
    // Scoped overrides override everything.
    FeatureSwitch::ScopedOverride scoped_override(&default_disabled_switch,
                                                  true);
    EXPECT_TRUE(default_disabled_switch.IsEnabled());
  }
}

TEST_F(FeatureSwitchEnabledTest,
       TrueFieldTrialValueAndTrueRequiredFieldTrialValue) {
  std::vector<std::string> required_trials;
  base::FieldTrialList field_trials(nullptr);
  scoped_refptr<base::FieldTrial> enabled_trial = CreateFieldTrial("Enabled");
  required_trials.push_back(kFieldTrialName);
  const char* required_trial_name = "required-trial";
  scoped_refptr<base::FieldTrial> enabled_required_trial =
      CreateFieldTrialWithName(required_trial_name, "Enabled");
  required_trials.push_back(required_trial_name);
  FeatureSwitch trial_enabled_switch(&command_line_, kSwitchName,
                                     required_trials,
                                     FeatureSwitch::DEFAULT_DISABLED);
  EXPECT_TRUE(trial_enabled_switch.IsEnabled());
}

TEST_F(FeatureSwitchEnabledTest,
       TrueFieldTrialValueAndFalseRequiredFieldTrialValue) {
  std::vector<std::string> required_trials;
  base::FieldTrialList field_trials(nullptr);
  scoped_refptr<base::FieldTrial> enabled_trial = CreateFieldTrial("Enabled");
  required_trials.push_back(kFieldTrialName);
  const char* required_trial_name = "required-trial";
  scoped_refptr<base::FieldTrial> enabled_required_trial =
      CreateFieldTrialWithName(required_trial_name, "Disabled");
  required_trials.push_back(required_trial_name);
  FeatureSwitch trial_enabled_switch(&command_line_, kSwitchName,
                                     required_trials,
                                     FeatureSwitch::DEFAULT_DISABLED);
  EXPECT_FALSE(trial_enabled_switch.IsEnabled());
}
