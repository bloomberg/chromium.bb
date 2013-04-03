// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>

#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "components/webdata/autofill/autofill_entry.h"
#include "testing/gtest/include/gtest/gtest.h"

const unsigned int kMaxAutofillTimeStamps = 2;

TEST(AutofillEntryTest, NoCulling) {
  std::vector<base::Time> source, result;
  base::Time current = base::Time::Now();
  for (size_t i = 0; i < kMaxAutofillTimeStamps; ++i)
    source.push_back(current);

  EXPECT_FALSE(AutofillEntry::CullTimeStamps(source, &result));
  EXPECT_EQ(result.size(), kMaxAutofillTimeStamps);
  for (std::vector<base::Time>::const_iterator it = result.begin();
       it != result.end(); ++it) {
    EXPECT_EQ(*it, current);
  }
}

TEST(AutofillEntryTest, Culling) {
  std::vector<base::Time> source, result;
  base::Time current = base::Time::Now();
  const int offset = 10000;

  int64 internal_value = current.ToInternalValue();
  for (size_t i = 0; i < kMaxAutofillTimeStamps * 2 ; ++i) {
    source.push_back(base::Time::FromInternalValue(
        internal_value + i * offset));
  }
  std::sort(source.begin(), source.end());
  EXPECT_TRUE(AutofillEntry::CullTimeStamps(source, &result));

  EXPECT_EQ(result.size(), kMaxAutofillTimeStamps);
  EXPECT_EQ(result.front(), base::Time::FromInternalValue(internal_value));
  int last_offset = (kMaxAutofillTimeStamps * 2 - 1) * offset;
  EXPECT_EQ(result.back(),
            base::Time::FromInternalValue(last_offset + internal_value));
}

TEST(AutofillEntryTest, CullByTime) {
  base::TimeDelta one_hour = base::TimeDelta::FromHours(1);

  std::vector<base::Time> timestamps;
  base::Time cutoff_time = AutofillEntry::ExpirationTime();

  // Within the time limit.
  timestamps.push_back(cutoff_time + one_hour);

  AutofillKey key(UTF8ToUTF16("test_key"), UTF8ToUTF16("test_value"));

  AutofillEntry entry_within_the_limits(key, timestamps);
  EXPECT_FALSE(entry_within_the_limits.IsExpired());

  // One within the time limit, one outside.
  timestamps.push_back(cutoff_time - one_hour);

  AutofillEntry entry_partially_within_the_limits(key, timestamps);
  EXPECT_TRUE(
      entry_partially_within_the_limits.IsExpired());

  // All outside the time limit.
  timestamps.clear();
  timestamps.push_back(cutoff_time - one_hour);
  timestamps.push_back(cutoff_time - one_hour * 2);
  timestamps.push_back(cutoff_time - one_hour * 3);

  AutofillEntry entry_outside_the_limits(key, timestamps);
  EXPECT_TRUE(entry_outside_the_limits.IsExpired());
  EXPECT_TRUE(entry_outside_the_limits.timestamps_culled());
}
