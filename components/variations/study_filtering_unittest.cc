// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/study_filtering.h"

#include <vector>

#include "base/strings/string_split.h"
#include "components/variations/processed_study.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chrome_variations {

namespace {

// Converts |time| to Study proto format.
int64 TimeToProtoTime(const base::Time& time) {
  return (time - base::Time::UnixEpoch()).InSeconds();
}

// Adds an experiment to |study| with the specified |name| and |probability|.
Study_Experiment* AddExperiment(const std::string& name, int probability,
                                Study* study) {
  Study_Experiment* experiment = study->add_experiment();
  experiment->set_name(name);
  experiment->set_probability_weight(probability);
  return experiment;
}

}  // namespace

TEST(VariationsStudyFilteringTest, CheckStudyChannel) {
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
      const bool result = internal::CheckStudyChannel(filter, channels[j]);
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
      const bool result = internal::CheckStudyChannel(filter, channels[j]);
      EXPECT_EQ(expected, result) << "Case " << i << "," << j << " failed!";
    }

    if (i < arraysize(channels)) {
      const int index = arraysize(channels) - i - 1;
      filter.add_channel(channels[index]);
      channel_added[index] = true;
    }
  }
}

TEST(VariationsStudyFilteringTest, CheckStudyFormFactor) {
  const Study_FormFactor form_factors[] = {
    Study_FormFactor_DESKTOP,
    Study_FormFactor_PHONE,
    Study_FormFactor_TABLET,
  };

  ASSERT_EQ(Study_FormFactor_FormFactor_ARRAYSIZE,
            static_cast<int>(arraysize(form_factors)));

  bool form_factor_added[arraysize(form_factors)] = { 0 };
  Study_Filter filter;

  for (size_t i = 0; i <= arraysize(form_factors); ++i) {
    for (size_t j = 0; j < arraysize(form_factors); ++j) {
      const bool expected = form_factor_added[j] ||
                            filter.form_factor_size() == 0;
      const bool result = internal::CheckStudyFormFactor(filter,
                                                         form_factors[j]);
      EXPECT_EQ(expected, result) << "Case " << i << "," << j << " failed!";
    }

    if (i < arraysize(form_factors)) {
      filter.add_form_factor(form_factors[i]);
      form_factor_added[i] = true;
    }
  }

  // Do the same check in the reverse order.
  filter.clear_form_factor();
  memset(&form_factor_added, 0, sizeof(form_factor_added));
  for (size_t i = 0; i <= arraysize(form_factors); ++i) {
    for (size_t j = 0; j < arraysize(form_factors); ++j) {
      const bool expected = form_factor_added[j] ||
                            filter.form_factor_size() == 0;
      const bool result = internal::CheckStudyFormFactor(filter,
                                                         form_factors[j]);
      EXPECT_EQ(expected, result) << "Case " << i << "," << j << " failed!";
    }

    if (i < arraysize(form_factors)) {
      const int index = arraysize(form_factors) - i - 1;;
      filter.add_form_factor(form_factors[index]);
      form_factor_added[index] = true;
    }
  }
}

TEST(VariationsStudyFilteringTest, CheckStudyLocale) {
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
              internal::CheckStudyLocale(filter, "en-US"));
    EXPECT_EQ(test_cases[i].en_ca_result,
              internal::CheckStudyLocale(filter, "en-CA"));
    EXPECT_EQ(test_cases[i].fr_result,
              internal::CheckStudyLocale(filter, "fr"));
  }
}

TEST(VariationsStudyFilteringTest, CheckStudyPlatform) {
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
      const bool result = internal::CheckStudyPlatform(filter, platforms[j]);
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
      const bool result = internal::CheckStudyPlatform(filter, platforms[j]);
      EXPECT_EQ(expected, result) << "Case " << i << "," << j << " failed!";
    }

    if (i < arraysize(platforms)) {
      const int index = arraysize(platforms) - i - 1;
      filter.add_platform(platforms[index]);
      platform_added[index] = true;
    }
  }
}

TEST(VariationsStudyFilteringTest, CheckStudyStartDate) {
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
  EXPECT_TRUE(internal::CheckStudyStartDate(filter, now));

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(start_test_cases); ++i) {
    filter.set_start_date(TimeToProtoTime(start_test_cases[i].start_date));
    const bool result = internal::CheckStudyStartDate(filter, now);
    EXPECT_EQ(start_test_cases[i].expected_result, result)
        << "Case " << i << " failed!";
  }
}

TEST(VariationsStudyFilteringTest, CheckStudyVersion) {
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
  EXPECT_TRUE(internal::CheckStudyVersion(filter, base::Version("1.2.3")));

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(min_test_cases); ++i) {
    filter.set_min_version(min_test_cases[i].min_version);
    const bool result =
        internal::CheckStudyVersion(filter, Version(min_test_cases[i].version));
    EXPECT_EQ(min_test_cases[i].expected_result, result) <<
        "Min. version case " << i << " failed!";
  }
  filter.clear_min_version();

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(max_test_cases); ++i) {
    filter.set_max_version(max_test_cases[i].max_version);
    const bool result =
        internal::CheckStudyVersion(filter, Version(max_test_cases[i].version));
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
            internal::CheckStudyVersion(
                filter, Version(min_test_cases[i].version));
        EXPECT_FALSE(result) << "Case " << i << "," << j << " failed!";
      }

      if (!max_test_cases[j].expected_result) {
        const bool result =
            internal::CheckStudyVersion(
                filter, Version(max_test_cases[j].version));
        EXPECT_FALSE(result) << "Case " << i << "," << j << " failed!";
      }
    }
  }
}

TEST(VariationsStudyFilteringTest, CheckStudyHardwareClass) {
  struct {
    const char* hardware_class;
    const char* exclude_hardware_class;
    const char* actual_hardware_class;
    bool expected_result;
  } test_cases[] = {
    // Neither filtered nor excluded set:
    // True since empty is always a match.
    {"", "", "fancy INTEL pear device", true},
    {"", "", "", true},

    // Filtered set:
    {"apple,pear,orange", "", "apple", true},
    {"apple,pear,orange", "", "fancy INTEL pear device", true},
    {"apple,pear,orange", "", "fancy INTEL GRAPE device", false},
    // Somehow tagged as both, but still valid.
    {"apple,pear,orange", "", "fancy INTEL pear GRAPE device", true},
    // Substring, so still valid.
    {"apple,pear,orange", "", "fancy INTEL SNapple device", true},
    // No issues with doubling.
    {"apple,pear,orange", "", "fancy orange orange device", true},
    // Empty, which is what would happen for non ChromeOS platforms.
    {"apple,pear,orange", "", "", false},

    // Excluded set:
    {"", "apple,pear,orange", "apple", false},
    {"", "apple,pear,orange", "fancy INTEL pear device", false},
    {"", "apple,pear,orange", "fancy INTEL GRAPE device", true},
    // Somehow tagged as both. Very excluded!
    {"", "apple,pear,orange", "fancy INTEL pear GRAPE device", false},
    // Substring, so still invalid.
    {"", "apple,pear,orange", "fancy INTEL SNapple device", false},
    // Empty.
    {"", "apple,pear,orange", "", true},

    // Not testing when both are set as it should never occur and should be
    // considered undefined.
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(test_cases); ++i) {
    Study_Filter filter;
    std::vector<std::string> hardware_class;
    base::SplitString(test_cases[i].hardware_class, ',', &hardware_class);
    for (size_t j = 0; j < hardware_class.size(); ++j)
      filter.add_hardware_class(hardware_class[j]);

    std::vector<std::string> exclude_hardware_class;
    base::SplitString(test_cases[i].exclude_hardware_class, ',',
                      &exclude_hardware_class);
    for (size_t j = 0; j < exclude_hardware_class.size(); ++j)
      filter.add_exclude_hardware_class(exclude_hardware_class[j]);

    EXPECT_EQ(test_cases[i].expected_result,
              internal::CheckStudyHardwareClass(
                  filter, test_cases[i].actual_hardware_class));
  }
}

TEST(VariationsStudyFilteringTest, FilterAndValidateStudies) {
  const std::string kTrial1Name = "A";
  const std::string kGroup1Name = "Group1";
  const std::string kTrial3Name = "B";

  VariationsSeed seed;
  Study* study1 = seed.add_study();
  study1->set_name(kTrial1Name);
  study1->set_default_experiment_name("Default");
  AddExperiment(kGroup1Name, 100, study1);
  AddExperiment("Default", 0, study1);

  Study* study2 = seed.add_study();
  *study2 = *study1;
  study2->mutable_experiment(0)->set_name("Bam");
  ASSERT_EQ(seed.study(0).name(), seed.study(1).name());

  Study* study3 = seed.add_study();
  study3->set_name(kTrial3Name);
  study3->set_default_experiment_name("Default");
  AddExperiment("A", 10, study3);
  AddExperiment("Default", 25, study3);

  std::vector<ProcessedStudy> processed_studies;
  FilterAndValidateStudies(
      seed, "en-CA", base::Time::Now(), base::Version("20.0.0.0"),
      Study_Channel_STABLE, Study_FormFactor_DESKTOP, "", &processed_studies);

  // Check that only the first kTrial1Name study was kept.
  ASSERT_EQ(2U, processed_studies.size());
  EXPECT_EQ(kTrial1Name, processed_studies[0].study()->name());
  EXPECT_EQ(kGroup1Name, processed_studies[0].study()->experiment(0).name());
  EXPECT_EQ(kTrial3Name, processed_studies[1].study()->name());
}

TEST(VariationsStudyFilteringTest, IsStudyExpired) {
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
  EXPECT_FALSE(internal::IsStudyExpired(study, now));

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(expiry_test_cases); ++i) {
    study.set_expiry_date(TimeToProtoTime(expiry_test_cases[i].expiry_date));
    const bool result = internal::IsStudyExpired(study, now);
    EXPECT_EQ(expiry_test_cases[i].expected_result, result)
        << "Case " << i << " failed!";
  }
}

TEST(VariationsStudyFilteringTest, ValidateStudy) {
  Study study;
  study.set_default_experiment_name("def");
  AddExperiment("abc", 100, &study);
  Study_Experiment* default_group = AddExperiment("def", 200, &study);

  ProcessedStudy processed_study;
  EXPECT_TRUE(processed_study.Init(&study, false));
  EXPECT_EQ(300, processed_study.total_probability());

  // Min version checks.
  study.mutable_filter()->set_min_version("1.2.3.*");
  EXPECT_TRUE(processed_study.Init(&study, false));
  study.mutable_filter()->set_min_version("1.*.3");
  EXPECT_FALSE(processed_study.Init(&study, false));
  study.mutable_filter()->set_min_version("1.2.3");
  EXPECT_TRUE(processed_study.Init(&study, false));

  // Max version checks.
  study.mutable_filter()->set_max_version("2.3.4.*");
  EXPECT_TRUE(processed_study.Init(&study, false));
  study.mutable_filter()->set_max_version("*.3");
  EXPECT_FALSE(processed_study.Init(&study, false));
  study.mutable_filter()->set_max_version("2.3.4");
  EXPECT_TRUE(processed_study.Init(&study, false));

  study.clear_default_experiment_name();
  EXPECT_FALSE(processed_study.Init(&study, false));

  study.set_default_experiment_name("xyz");
  EXPECT_FALSE(processed_study.Init(&study, false));

  study.set_default_experiment_name("def");
  default_group->clear_name();
  EXPECT_FALSE(processed_study.Init(&study, false));

  default_group->set_name("def");
  EXPECT_TRUE(processed_study.Init(&study, false));
  Study_Experiment* repeated_group = study.add_experiment();
  repeated_group->set_name("abc");
  repeated_group->set_probability_weight(1);
  EXPECT_FALSE(processed_study.Init(&study, false));
}

}  // namespace chrome_variations
