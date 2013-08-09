// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/variations/variations_seed_processor.h"

#include <vector>

#include "base/command_line.h"
#include "base/strings/string_split.h"
#include "chrome/common/metrics/variations/variations_associated_data.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chrome_variations {

namespace {

// Converts |time| to Study proto format.
int64 TimeToProtoTime(const base::Time& time) {
  return (time - base::Time::UnixEpoch()).InSeconds();
}

// Constants for testing associating command line flags with trial groups.
const char kFlagStudyName[] = "flag_test_trial";
const char kFlagGroup1Name[] = "flag_group1";
const char kFlagGroup2Name[] = "flag_group2";
const char kNonFlagGroupName[] = "non_flag_group";
const char kForcingFlag1[] = "flag_test1";
const char kForcingFlag2[] = "flag_test2";

// Adds an experiment to |study| with the specified |name| and |probability|.
Study_Experiment* AddExperiment(const std::string& name, int probability,
                                Study* study) {
  Study_Experiment* experiment = study->add_experiment();
  experiment->set_name(name);
  experiment->set_probability_weight(probability);
  return experiment;
}

// Populates |study| with test data used for testing associating command line
// flags with trials groups. The study will contain three groups, a default
// group that isn't associated with a flag, and two other groups, both
// associated with different flags.
Study CreateStudyWithFlagGroups(int default_group_probability,
                                int flag_group1_probability,
                                int flag_group2_probability) {
  DCHECK_GE(default_group_probability, 0);
  DCHECK_GE(flag_group1_probability, 0);
  DCHECK_GE(flag_group2_probability, 0);
  Study study;
  study.set_name(kFlagStudyName);
  study.set_default_experiment_name(kNonFlagGroupName);

  AddExperiment(kNonFlagGroupName, default_group_probability, &study);
  AddExperiment(kFlagGroup1Name, flag_group1_probability, &study)
      ->set_forcing_flag(kForcingFlag1);
  AddExperiment(kFlagGroup2Name, flag_group2_probability, &study)
      ->set_forcing_flag(kForcingFlag2);

  return study;
}

}  // namespace

TEST(VariationsSeedProcessorTest, CheckStudyChannel) {
  VariationsSeedProcessor seed_processor;

  const Study_Channel channels[] = {
    Study_Channel_CANARY,
    Study_Channel_DEV,
    Study_Channel_BETA,
    Study_Channel_STABLE,
  };
  bool channel_added[arraysize(channels)] = { 0 };

  Study_Filter filter;

  // Check in the forwarded order. The loop cond is <= arraysize(channels)
  // instead of < so that the result of adding the last channel gets checked.
  for (size_t i = 0; i <= arraysize(channels); ++i) {
    for (size_t j = 0; j < arraysize(channels); ++j) {
      const bool expected = channel_added[j] || filter.channel_size() == 0;
      const bool result = seed_processor.CheckStudyChannel(filter, channels[j]);
      EXPECT_EQ(expected, result) << "Case " << i << "," << j << " failed!";
    }

    if (i < arraysize(channels)) {
      filter.add_channel(channels[i]);
      channel_added[i] = true;
    }
  }

  // Do the same check in the reverse order.
  filter.clear_channel();
  memset(&channel_added, 0, sizeof(channel_added));
  for (size_t i = 0; i <= arraysize(channels); ++i) {
    for (size_t j = 0; j < arraysize(channels); ++j) {
      const bool expected = channel_added[j] || filter.channel_size() == 0;
      const bool result = seed_processor.CheckStudyChannel(filter, channels[j]);
      EXPECT_EQ(expected, result) << "Case " << i << "," << j << " failed!";
    }

    if (i < arraysize(channels)) {
      const int index = arraysize(channels) - i - 1;
      filter.add_channel(channels[index]);
      channel_added[index] = true;
    }
  }
}

TEST(VariationsSeedProcessorTest, CheckStudyLocale) {
  VariationsSeedProcessor seed_processor;

  struct {
    const char* filter_locales;
    bool en_us_result;
    bool en_ca_result;
    bool fr_result;
  } test_cases[] = {
    {"en-US", true, false, false},
    {"en-US,en-CA,fr", true, true, true},
    {"en-US,en-CA,en-GB", true, true, false},
    {"en-GB,en-CA,en-US", true, true, false},
    {"ja,kr,vi", false, false, false},
    {"fr-CA", false, false, false},
    {"", true, true, true},
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(test_cases); ++i) {
    std::vector<std::string> filter_locales;
    Study_Filter filter;
    base::SplitString(test_cases[i].filter_locales, ',', &filter_locales);
    for (size_t j = 0; j < filter_locales.size(); ++j)
      filter.add_locale(filter_locales[j]);
    EXPECT_EQ(test_cases[i].en_us_result,
              seed_processor.CheckStudyLocale(filter, "en-US"));
    EXPECT_EQ(test_cases[i].en_ca_result,
              seed_processor.CheckStudyLocale(filter, "en-CA"));
    EXPECT_EQ(test_cases[i].fr_result,
              seed_processor.CheckStudyLocale(filter, "fr"));
  }
}

TEST(VariationsSeedProcessorTest, CheckStudyPlatform) {
  VariationsSeedProcessor seed_processor;

  const Study_Platform platforms[] = {
    Study_Platform_PLATFORM_WINDOWS,
    Study_Platform_PLATFORM_MAC,
    Study_Platform_PLATFORM_LINUX,
    Study_Platform_PLATFORM_CHROMEOS,
    Study_Platform_PLATFORM_ANDROID,
    Study_Platform_PLATFORM_IOS,
  };
  ASSERT_EQ(Study_Platform_Platform_ARRAYSIZE,
            static_cast<int>(arraysize(platforms)));
  bool platform_added[arraysize(platforms)] = { 0 };

  Study_Filter filter;

  // Check in the forwarded order. The loop cond is <= arraysize(platforms)
  // instead of < so that the result of adding the last channel gets checked.
  for (size_t i = 0; i <= arraysize(platforms); ++i) {
    for (size_t j = 0; j < arraysize(platforms); ++j) {
      const bool expected = platform_added[j] || filter.platform_size() == 0;
      const bool result = seed_processor.CheckStudyPlatform(filter,
                                                            platforms[j]);
      EXPECT_EQ(expected, result) << "Case " << i << "," << j << " failed!";
    }

    if (i < arraysize(platforms)) {
      filter.add_platform(platforms[i]);
      platform_added[i] = true;
    }
  }

  // Do the same check in the reverse order.
  filter.clear_platform();
  memset(&platform_added, 0, sizeof(platform_added));
  for (size_t i = 0; i <= arraysize(platforms); ++i) {
    for (size_t j = 0; j < arraysize(platforms); ++j) {
      const bool expected = platform_added[j] || filter.platform_size() == 0;
      const bool result = seed_processor.CheckStudyPlatform(filter,
                                                            platforms[j]);
      EXPECT_EQ(expected, result) << "Case " << i << "," << j << " failed!";
    }

    if (i < arraysize(platforms)) {
      const int index = arraysize(platforms) - i - 1;
      filter.add_platform(platforms[index]);
      platform_added[index] = true;
    }
  }
}

TEST(VariationsSeedProcessorTest, CheckStudyStartDate) {
  VariationsSeedProcessor seed_processor;

  const base::Time now = base::Time::Now();
  const base::TimeDelta delta = base::TimeDelta::FromHours(1);
  const struct {
    const base::Time start_date;
    bool expected_result;
  } start_test_cases[] = {
    { now - delta, true },
    { now, true },
    { now + delta, false },
  };

  Study_Filter filter;

  // Start date not set should result in true.
  EXPECT_TRUE(seed_processor.CheckStudyStartDate(filter, now));

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(start_test_cases); ++i) {
    filter.set_start_date(TimeToProtoTime(start_test_cases[i].start_date));
    const bool result = seed_processor.CheckStudyStartDate(filter, now);
    EXPECT_EQ(start_test_cases[i].expected_result, result)
        << "Case " << i << " failed!";
  }
}

TEST(VariationsSeedProcessorTest, CheckStudyVersion) {
  VariationsSeedProcessor seed_processor;

  const struct {
    const char* min_version;
    const char* version;
    bool expected_result;
  } min_test_cases[] = {
    { "1.2.2", "1.2.3", true },
    { "1.2.3", "1.2.3", true },
    { "1.2.4", "1.2.3", false },
    { "1.3.2", "1.2.3", false },
    { "2.1.2", "1.2.3", false },
    { "0.3.4", "1.2.3", true },
    // Wildcards.
    { "1.*", "1.2.3", true },
    { "1.2.*", "1.2.3", true },
    { "1.2.3.*", "1.2.3", true },
    { "1.2.4.*", "1.2.3", false },
    { "2.*", "1.2.3", false },
    { "0.3.*", "1.2.3", true },
  };

  const struct {
    const char* max_version;
    const char* version;
    bool expected_result;
  } max_test_cases[] = {
    { "1.2.2", "1.2.3", false },
    { "1.2.3", "1.2.3", true },
    { "1.2.4", "1.2.3", true },
    { "2.1.1", "1.2.3", true },
    { "2.1.1", "2.3.4", false },
    // Wildcards
    { "2.1.*", "2.3.4", false },
    { "2.*", "2.3.4", true },
    { "2.3.*", "2.3.4", true },
    { "2.3.4.*", "2.3.4", true },
    { "2.3.4.0.*", "2.3.4", true },
    { "2.4.*", "2.3.4", true },
    { "1.3.*", "2.3.4", false },
    { "1.*", "2.3.4", false },
  };

  Study_Filter filter;

  // Min/max version not set should result in true.
  EXPECT_TRUE(seed_processor.CheckStudyVersion(filter, base::Version("1.2.3")));

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(min_test_cases); ++i) {
    filter.set_min_version(min_test_cases[i].min_version);
    const bool result =
        seed_processor.CheckStudyVersion(filter,
                                         Version(min_test_cases[i].version));
    EXPECT_EQ(min_test_cases[i].expected_result, result) <<
        "Min. version case " << i << " failed!";
  }
  filter.clear_min_version();

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(max_test_cases); ++i) {
    filter.set_max_version(max_test_cases[i].max_version);
    const bool result =
        seed_processor.CheckStudyVersion(filter,
                                         Version(max_test_cases[i].version));
    EXPECT_EQ(max_test_cases[i].expected_result, result) <<
        "Max version case " << i << " failed!";
  }

  // Check intersection semantics.
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(min_test_cases); ++i) {
    for (size_t j = 0; j < ARRAYSIZE_UNSAFE(max_test_cases); ++j) {
      filter.set_min_version(min_test_cases[i].min_version);
      filter.set_max_version(max_test_cases[j].max_version);

      if (!min_test_cases[i].expected_result) {
        const bool result =
            seed_processor.CheckStudyVersion(
                filter, Version(min_test_cases[i].version));
        EXPECT_FALSE(result) << "Case " << i << "," << j << " failed!";
      }

      if (!max_test_cases[j].expected_result) {
        const bool result =
            seed_processor.CheckStudyVersion(
                filter, Version(max_test_cases[j].version));
        EXPECT_FALSE(result) << "Case " << i << "," << j << " failed!";
      }
    }
  }
}

// Test that the group for kForcingFlag1 is forced.
TEST(VariationsSeedProcessorTest, ForceGroupWithFlag1) {
  CommandLine::ForCurrentProcess()->AppendSwitch(kForcingFlag1);

  base::FieldTrialList field_trial_list(NULL);

  Study study = CreateStudyWithFlagGroups(100, 0, 0);
  VariationsSeedProcessor().CreateTrialFromStudy(study, false);

  EXPECT_EQ(kFlagGroup1Name,
            base::FieldTrialList::FindFullName(kFlagStudyName));
}

// Test that the group for kForcingFlag2 is forced.
TEST(VariationsSeedProcessorTest, ForceGroupWithFlag2) {
  CommandLine::ForCurrentProcess()->AppendSwitch(kForcingFlag2);

  base::FieldTrialList field_trial_list(NULL);

  Study study = CreateStudyWithFlagGroups(100, 0, 0);
  VariationsSeedProcessor().CreateTrialFromStudy(study, false);

  EXPECT_EQ(kFlagGroup2Name,
            base::FieldTrialList::FindFullName(kFlagStudyName));
}

TEST(VariationsSeedProcessorTest, ForceGroup_ChooseFirstGroupWithFlag) {
  // Add the flag to the command line arguments so the flag group is forced.
  CommandLine::ForCurrentProcess()->AppendSwitch(kForcingFlag1);
  CommandLine::ForCurrentProcess()->AppendSwitch(kForcingFlag2);

  base::FieldTrialList field_trial_list(NULL);

  Study study = CreateStudyWithFlagGroups(100, 0, 0);
  VariationsSeedProcessor().CreateTrialFromStudy(study, false);

  EXPECT_EQ(kFlagGroup1Name,
            base::FieldTrialList::FindFullName(kFlagStudyName));
}

TEST(VariationsSeedProcessorTest, ForceGroup_DontChooseGroupWithFlag) {
  base::FieldTrialList field_trial_list(NULL);

  // The two flag groups are given high probability, which would normally make
  // them very likely to be chosen. They won't be chosen since flag groups are
  // never chosen when their flag isn't present.
  Study study = CreateStudyWithFlagGroups(1, 999, 999);
  VariationsSeedProcessor().CreateTrialFromStudy(study, false);
  EXPECT_EQ(kNonFlagGroupName,
            base::FieldTrialList::FindFullName(kFlagStudyName));
}

TEST(VariationsSeedProcessorTest, IsStudyExpired) {
  VariationsSeedProcessor seed_processor;

  const base::Time now = base::Time::Now();
  const base::TimeDelta delta = base::TimeDelta::FromHours(1);
  const struct {
    const base::Time expiry_date;
    bool expected_result;
  } expiry_test_cases[] = {
    { now - delta, true },
    { now, true },
    { now + delta, false },
  };

  Study study;

  // Expiry date not set should result in false.
  EXPECT_FALSE(seed_processor.IsStudyExpired(study, now));

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(expiry_test_cases); ++i) {
    study.set_expiry_date(TimeToProtoTime(expiry_test_cases[i].expiry_date));
    const bool result = seed_processor.IsStudyExpired(study, now);
    EXPECT_EQ(expiry_test_cases[i].expected_result, result)
        << "Case " << i << " failed!";
  }
}

TEST(VariationsSeedProcessorTest, NonExpiredStudyPrioritizedOverExpiredStudy) {
  VariationsSeedProcessor seed_processor;

  const std::string kTrialName = "A";
  const std::string kGroup1Name = "Group1";

  TrialsSeed seed;
  Study* study1 = seed.add_study();
  study1->set_name(kTrialName);
  study1->set_default_experiment_name("Default");
  AddExperiment(kGroup1Name, 100, study1);
  AddExperiment("Default", 0, study1);
  Study* study2 = seed.add_study();
  *study2 = *study1;
  ASSERT_EQ(seed.study(0).name(), seed.study(1).name());

  const base::Time year_ago =
      base::Time::Now() - base::TimeDelta::FromDays(365);

  const base::Version version("20.0.0.0");

  // Check that adding [expired, non-expired] activates the non-expired one.
  ASSERT_EQ(std::string(), base::FieldTrialList::FindFullName(kTrialName));
  {
    base::FieldTrialList field_trial_list(NULL);
    study1->set_expiry_date(TimeToProtoTime(year_ago));
    seed_processor.CreateTrialsFromSeed(seed, "en-CA", base::Time::Now(),
                                        version, Study_Channel_STABLE);
    EXPECT_EQ(kGroup1Name, base::FieldTrialList::FindFullName(kTrialName));
  }

  // Check that adding [non-expired, expired] activates the non-expired one.
  ASSERT_EQ(std::string(), base::FieldTrialList::FindFullName(kTrialName));
  {
    base::FieldTrialList field_trial_list(NULL);
    study1->clear_expiry_date();
    study2->set_expiry_date(TimeToProtoTime(year_ago));
    seed_processor.CreateTrialsFromSeed(seed, "en-CA", base::Time::Now(),
                                        version, Study_Channel_STABLE);
    EXPECT_EQ(kGroup1Name, base::FieldTrialList::FindFullName(kTrialName));
  }
}

TEST(VariationsSeedProcessorTest, ValidateStudy) {
  VariationsSeedProcessor seed_processor;

  Study study;
  study.set_default_experiment_name("def");
  AddExperiment("abc", 100, &study);
  Study_Experiment* default_group = AddExperiment("def", 200, &study);

  base::FieldTrial::Probability total_probability = 0;
  bool valid = seed_processor.ValidateStudyAndComputeTotalProbability(
      study, &total_probability);
  EXPECT_TRUE(valid);
  EXPECT_EQ(300, total_probability);

  // Min version checks.
  study.mutable_filter()->set_min_version("1.2.3.*");
  valid = seed_processor.ValidateStudyAndComputeTotalProbability(
      study, &total_probability);
  EXPECT_TRUE(valid);
  study.mutable_filter()->set_min_version("1.*.3");
  valid = seed_processor.ValidateStudyAndComputeTotalProbability(
      study, &total_probability);
  EXPECT_FALSE(valid);
  study.mutable_filter()->set_min_version("1.2.3");
  valid = seed_processor.ValidateStudyAndComputeTotalProbability(
      study, &total_probability);
  EXPECT_TRUE(valid);

  // Max version checks.
  study.mutable_filter()->set_max_version("2.3.4.*");
  valid = seed_processor.ValidateStudyAndComputeTotalProbability(
      study, &total_probability);
  EXPECT_TRUE(valid);
  study.mutable_filter()->set_max_version("*.3");
  valid = seed_processor.ValidateStudyAndComputeTotalProbability(
      study, &total_probability);
  EXPECT_FALSE(valid);
  study.mutable_filter()->set_max_version("2.3.4");
  valid = seed_processor.ValidateStudyAndComputeTotalProbability(
      study, &total_probability);
  EXPECT_TRUE(valid);

  study.clear_default_experiment_name();
  valid = seed_processor.ValidateStudyAndComputeTotalProbability(study,
      &total_probability);
  EXPECT_FALSE(valid);

  study.set_default_experiment_name("xyz");
  valid = seed_processor.ValidateStudyAndComputeTotalProbability(study,
      &total_probability);
  EXPECT_FALSE(valid);

  study.set_default_experiment_name("def");
  default_group->clear_name();
  valid = seed_processor.ValidateStudyAndComputeTotalProbability(study,
      &total_probability);
  EXPECT_FALSE(valid);

  default_group->set_name("def");
  valid = seed_processor.ValidateStudyAndComputeTotalProbability(study,
      &total_probability);
  ASSERT_TRUE(valid);
  Study_Experiment* repeated_group = study.add_experiment();
  repeated_group->set_name("abc");
  repeated_group->set_probability_weight(1);
  valid = seed_processor.ValidateStudyAndComputeTotalProbability(study,
      &total_probability);
  EXPECT_FALSE(valid);
}

TEST(VariationsSeedProcessorTest, VariationParams) {
  base::FieldTrialList field_trial_list(NULL);
  VariationsSeedProcessor seed_processor;

  Study study;
  study.set_name("Study1");
  study.set_default_experiment_name("B");

  Study_Experiment* experiment1 = AddExperiment("A", 1, &study);
  Study_Experiment_Param* param = experiment1->add_param();
  param->set_name("x");
  param->set_value("y");

  Study_Experiment* experiment2 = AddExperiment("B", 0, &study);

  seed_processor.CreateTrialFromStudy(study, false);
  EXPECT_EQ("y", GetVariationParamValue("Study1", "x"));

  study.set_name("Study2");
  experiment1->set_probability_weight(0);
  experiment2->set_probability_weight(1);
  seed_processor.CreateTrialFromStudy(study, false);
  EXPECT_EQ(std::string(), GetVariationParamValue("Study2", "x"));
}

}  // namespace chrome_variations
