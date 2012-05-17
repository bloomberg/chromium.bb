// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/proto/study.pb.h"
#include "chrome/browser/metrics/variations_service.h"
#include "chrome/common/chrome_version_info.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// Converts |time| to chrome_variations::Study proto format.
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
  const chrome_variations::Study_Channel study_channels[] = {
    chrome_variations::Study_Channel_CANARY,
    chrome_variations::Study_Channel_DEV,
    chrome_variations::Study_Channel_BETA,
    chrome_variations::Study_Channel_STABLE,
  };
  ASSERT_EQ(arraysize(channels), arraysize(study_channels));
  bool channel_added[arraysize(channels)] = { 0 };

  chrome_variations::Study study;

  // Check in the forwarded order. The loop cond is <= arraysize(study_channels)
  // instead of < so that the result of adding the last channel gets checked.
  for (size_t i = 0; i <= arraysize(study_channels); ++i) {
    for (size_t j = 0; j < arraysize(channels); ++j) {
      const bool expected = channel_added[j] || study.channel_size() == 0;
      const bool result = VariationsService::CheckStudyChannel(study,
                                                               channels[j]);
      EXPECT_EQ(expected, result) << "Case " << i << "," << j << " failed!";
    }

    if (i < arraysize(study_channels))
    {
      study.add_channel(study_channels[i]);
      channel_added[i] = true;
    }
  }

  // Do the same check in the reverse order.
  study.clear_channel();
  memset(&channel_added, 0, sizeof(channel_added));
  for (size_t i = 0; i <= arraysize(study_channels); ++i) {
    for (size_t j = 0; j < arraysize(channels); ++j) {
      const bool expected = channel_added[j] || study.channel_size() == 0;
      const bool result = VariationsService::CheckStudyChannel(study,
                                                               channels[j]);
      EXPECT_EQ(expected, result) << "Case " << i << "," << j << " failed!";
    }

    if (i < arraysize(study_channels))
    {
      const int index = arraysize(study_channels) - i - 1;
      study.add_channel(study_channels[index]);
      channel_added[index] = true;
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
  };

  chrome_variations::Study study;

  // Min/max version not set should result in true.
  EXPECT_TRUE(VariationsService::CheckStudyVersion(study, "1.2.3"));

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(min_test_cases); ++i) {
    study.set_min_version(min_test_cases[i].min_version);
    const bool result =
        VariationsService::CheckStudyVersion(study, min_test_cases[i].version);
    EXPECT_EQ(min_test_cases[i].expected_result, result) <<
        "Case " << i << " failed!";
  }
  study.clear_min_version();

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(max_test_cases); ++i) {
    study.set_max_version(max_test_cases[i].max_version);
    const bool result =
        VariationsService::CheckStudyVersion(study, max_test_cases[i].version);
    EXPECT_EQ(max_test_cases[i].expected_result, result) <<
        "Case " << i << " failed!";
  }

  // Check intersection semantics.
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(min_test_cases); ++i) {
    for (size_t j = 0; j < ARRAYSIZE_UNSAFE(max_test_cases); ++j) {
      study.set_min_version(min_test_cases[i].min_version);
      study.set_max_version(max_test_cases[j].max_version);

      if (!min_test_cases[i].expected_result) {
        const bool result =
            VariationsService::CheckStudyVersion(study,
                                                 min_test_cases[i].version);
        EXPECT_FALSE(result) << "Case " << i << "," << j << " failed!";
      }

      if (!max_test_cases[j].expected_result) {
        const bool result =
            VariationsService::CheckStudyVersion(study,
                                                 max_test_cases[j].version);
        EXPECT_FALSE(result) << "Case " << i << "," << j << " failed!";
      }
    }
  }
}

// The current client logic does not handle version number strings containing
// wildcards. Check that any such values received from the server result in the
// study being disqualified.
TEST(VariationsServiceTest, CheckStudyVersionWildcards) {
  chrome_variations::Study study;

  study.set_min_version("1.0.*");
  EXPECT_FALSE(VariationsService::CheckStudyVersion(study, "1.2.3"));

  study.clear_min_version();
  study.set_max_version("2.0.*");
  EXPECT_FALSE(VariationsService::CheckStudyVersion(study, "1.2.3"));
}

TEST(VariationsServiceTest, CheckStudyDate) {
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
  const struct {
    const base::Time expiry_date;
    bool expected_result;
  } expiry_test_cases[] = {
    { now - delta, false },
    { now, false },
    { now + delta, true },
  };

  chrome_variations::Study study;

  // Start/expiry date not set should result in true.
  EXPECT_TRUE(VariationsService::CheckStudyDate(study, now));

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(start_test_cases); ++i) {
    study.set_start_date(TimeToProtoTime(start_test_cases[i].start_date));
    const bool result = VariationsService::CheckStudyDate(study, now);
    EXPECT_EQ(start_test_cases[i].expected_result, result)
        << "Case " << i << " failed!";
  }
  study.clear_start_date();

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(expiry_test_cases); ++i) {
    study.set_expiry_date(TimeToProtoTime(expiry_test_cases[i].expiry_date));
    const bool result = VariationsService::CheckStudyDate(study, now);
    EXPECT_EQ(expiry_test_cases[i].expected_result, result)
        << "Case " << i << " failed!";
  }

  // Check intersection semantics.
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(start_test_cases); ++i) {
    for (size_t j = 0; j < ARRAYSIZE_UNSAFE(expiry_test_cases); ++j) {
      study.set_start_date(TimeToProtoTime(start_test_cases[i].start_date));
      study.set_expiry_date(TimeToProtoTime(expiry_test_cases[j].expiry_date));
      const bool expected = start_test_cases[i].expected_result &&
                            expiry_test_cases[j].expected_result;
      const bool result = VariationsService::CheckStudyDate(study, now);
      EXPECT_EQ(expected, result) << "Case " << i << "," << j << " failed!";
    }
  }
}
