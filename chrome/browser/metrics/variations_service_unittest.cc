// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/string_split.h"
#include "chrome/browser/metrics/proto/study.pb.h"
#include "chrome/browser/metrics/variations_service.h"
#include "chrome/common/chrome_version_info.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chrome_variations {

namespace {

// Converts |time| to Study proto format.
int64 TimeToProtoTime(const base::Time& time) {
  return (time - base::Time::UnixEpoch()).InSeconds();
}

}  // namespace

TEST(VariationsServiceTest, CheckStudyChannel) {
  const chrome::VersionInfo::Channel channels[] = {
    chrome::VersionInfo::CHANNEL_CANARY,
    chrome::VersionInfo::CHANNEL_DEV,
    chrome::VersionInfo::CHANNEL_BETA,
    chrome::VersionInfo::CHANNEL_STABLE,
  };
  const Study_Channel study_channels[] = {
    Study_Channel_CANARY,
    Study_Channel_DEV,
    Study_Channel_BETA,
    Study_Channel_STABLE,
  };
  ASSERT_EQ(arraysize(channels), arraysize(study_channels));
  bool channel_added[arraysize(channels)] = { 0 };

  Study_Filter filter;

  // Check in the forwarded order. The loop cond is <= arraysize(study_channels)
  // instead of < so that the result of adding the last channel gets checked.
  for (size_t i = 0; i <= arraysize(study_channels); ++i) {
    for (size_t j = 0; j < arraysize(channels); ++j) {
      const bool expected = channel_added[j] || filter.channel_size() == 0;
      const bool result = VariationsService::CheckStudyChannel(filter,
                                                               channels[j]);
      EXPECT_EQ(expected, result) << "Case " << i << "," << j << " failed!";
    }

    if (i < arraysize(study_channels)) {
      filter.add_channel(study_channels[i]);
      channel_added[i] = true;
    }
  }

  // Do the same check in the reverse order.
  filter.clear_channel();
  memset(&channel_added, 0, sizeof(channel_added));
  for (size_t i = 0; i <= arraysize(study_channels); ++i) {
    for (size_t j = 0; j < arraysize(channels); ++j) {
      const bool expected = channel_added[j] || filter.channel_size() == 0;
      const bool result = VariationsService::CheckStudyChannel(filter,
                                                               channels[j]);
      EXPECT_EQ(expected, result) << "Case " << i << "," << j << " failed!";
    }

    if (i < arraysize(study_channels)) {
      const int index = arraysize(study_channels) - i - 1;
      filter.add_channel(study_channels[index]);
      channel_added[index] = true;
    }
  }
}

TEST(VariationsServiceTest, CheckStudyLocale) {
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
              VariationsService::CheckStudyLocale(filter, "en-US"));
    EXPECT_EQ(test_cases[i].en_ca_result,
              VariationsService::CheckStudyLocale(filter, "en-CA"));
    EXPECT_EQ(test_cases[i].fr_result,
              VariationsService::CheckStudyLocale(filter, "fr"));
  }
}

TEST(VariationsServiceTest, CheckStudyPlatform) {
  const chrome_variations::Study_Platform platforms[] = {
    chrome_variations::Study_Platform_PLATFORM_WINDOWS,
    chrome_variations::Study_Platform_PLATFORM_MAC,
    chrome_variations::Study_Platform_PLATFORM_LINUX,
    chrome_variations::Study_Platform_PLATFORM_CHROMEOS,
    chrome_variations::Study_Platform_PLATFORM_ANDROID,
    chrome_variations::Study_Platform_PLATFORM_IOS,
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
      const bool result = VariationsService::CheckStudyPlatform(filter,
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
      const bool result = VariationsService::CheckStudyPlatform(filter,
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

TEST(VariationsServiceTest, CheckStudyVersion) {
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
  EXPECT_TRUE(VariationsService::CheckStudyVersion(filter, "1.2.3"));

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(min_test_cases); ++i) {
    filter.set_min_version(min_test_cases[i].min_version);
    const bool result =
        VariationsService::CheckStudyVersion(filter, min_test_cases[i].version);
    EXPECT_EQ(min_test_cases[i].expected_result, result) <<
        "Min. version case " << i << " failed!";
  }
  filter.clear_min_version();

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(max_test_cases); ++i) {
    filter.set_max_version(max_test_cases[i].max_version);
    const bool result =
        VariationsService::CheckStudyVersion(filter, max_test_cases[i].version);
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
            VariationsService::CheckStudyVersion(filter,
                                                 min_test_cases[i].version);
        EXPECT_FALSE(result) << "Case " << i << "," << j << " failed!";
      }

      if (!max_test_cases[j].expected_result) {
        const bool result =
            VariationsService::CheckStudyVersion(filter,
                                                 max_test_cases[j].version);
        EXPECT_FALSE(result) << "Case " << i << "," << j << " failed!";
      }
    }
  }
}

TEST(VariationsServiceTest, CheckStudyStartDate) {
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
  EXPECT_TRUE(VariationsService::CheckStudyStartDate(filter, now));

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(start_test_cases); ++i) {
    filter.set_start_date(TimeToProtoTime(start_test_cases[i].start_date));
    const bool result = VariationsService::CheckStudyStartDate(filter, now);
    EXPECT_EQ(start_test_cases[i].expected_result, result)
        << "Case " << i << " failed!";
  }
}

TEST(VariationsServiceTest, IsStudyExpired) {
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
  EXPECT_FALSE(VariationsService::IsStudyExpired(study, now));

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(expiry_test_cases); ++i) {
    study.set_expiry_date(TimeToProtoTime(expiry_test_cases[i].expiry_date));
    const bool result = VariationsService::IsStudyExpired(study, now);
    EXPECT_EQ(expiry_test_cases[i].expected_result, result)
        << "Case " << i << " failed!";
  }
}

TEST(VariationsServiceTest, ValidateStudy) {
  Study study;
  study.set_default_experiment_name("def");

  Study_Experiment* experiment = study.add_experiment();
  experiment->set_name("abc");
  experiment->set_probability_weight(100);

  Study_Experiment* default_group = study.add_experiment();
  default_group->set_name("def");
  default_group->set_probability_weight(200);

  base::FieldTrial::Probability total_probability = 0;
  bool valid = VariationsService::ValidateStudyAndComputeTotalProbability(
      study, &total_probability);
  EXPECT_TRUE(valid);
  EXPECT_EQ(300, total_probability);

  // Min version checks.
  study.mutable_filter()->set_min_version("1.2.3.*");
  valid = VariationsService::ValidateStudyAndComputeTotalProbability(
      study, &total_probability);
  EXPECT_TRUE(valid);
  study.mutable_filter()->set_min_version("1.*.3");
  valid = VariationsService::ValidateStudyAndComputeTotalProbability(
      study, &total_probability);
  EXPECT_FALSE(valid);
  study.mutable_filter()->set_min_version("1.2.3");
  valid = VariationsService::ValidateStudyAndComputeTotalProbability(
      study, &total_probability);
  EXPECT_TRUE(valid);

  // Max version checks.
  study.mutable_filter()->set_max_version("2.3.4.*");
  valid = VariationsService::ValidateStudyAndComputeTotalProbability(
      study, &total_probability);
  EXPECT_TRUE(valid);
  study.mutable_filter()->set_max_version("*.3");
  valid = VariationsService::ValidateStudyAndComputeTotalProbability(
      study, &total_probability);
  EXPECT_FALSE(valid);
  study.mutable_filter()->set_max_version("2.3.4");
  valid = VariationsService::ValidateStudyAndComputeTotalProbability(
      study, &total_probability);
  EXPECT_TRUE(valid);

  study.clear_default_experiment_name();
  valid = VariationsService::ValidateStudyAndComputeTotalProbability(study,
      &total_probability);
  EXPECT_FALSE(valid);

  study.set_default_experiment_name("xyz");
  valid = VariationsService::ValidateStudyAndComputeTotalProbability(study,
      &total_probability);
  EXPECT_FALSE(valid);

  study.set_default_experiment_name("def");
  default_group->clear_name();
  valid = VariationsService::ValidateStudyAndComputeTotalProbability(study,
      &total_probability);
  EXPECT_FALSE(valid);

  default_group->set_name("def");
  valid = VariationsService::ValidateStudyAndComputeTotalProbability(study,
      &total_probability);
  ASSERT_TRUE(valid);
  Study_Experiment* repeated_group = study.add_experiment();
  repeated_group->set_name("abc");
  repeated_group->set_probability_weight(1);
  valid = VariationsService::ValidateStudyAndComputeTotalProbability(study,
      &total_probability);
  EXPECT_FALSE(valid);
}

}  // namespace chrome_variations
