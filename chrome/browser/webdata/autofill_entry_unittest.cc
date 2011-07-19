// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>

#include "base/time.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "chrome/browser/webdata/autofill_entry.h"

extern const unsigned int kMaxAutofillTimeStamps;
TEST(AutofillEntryTest, NoCulling) {
  std::vector<base::Time> source, result;
  base::Time current = base::Time::Now();
  for (size_t i = 0; i < kMaxAutofillTimeStamps -1 ; ++i) {
    source.push_back(current);
  }

  EXPECT_FALSE(AutofillEntry::CullTimeStamps(source, &result));
  EXPECT_EQ(result.size(), kMaxAutofillTimeStamps -1);
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
  sort(source.begin(), source.end());
  EXPECT_TRUE(AutofillEntry::CullTimeStamps(source, &result));

  EXPECT_EQ(result.size(), kMaxAutofillTimeStamps);
  int count = kMaxAutofillTimeStamps * 2 - 1;
  for (std::vector<base::Time>::const_iterator it = result.begin();
       it != result.end(); ++it) {
    EXPECT_EQ(*it, base::Time::FromInternalValue(
              count*offset + internal_value));
    --count;
  }
}
