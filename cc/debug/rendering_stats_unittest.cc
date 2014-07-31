// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/time/time.h"
#include "base/values.h"
#include "cc/debug/rendering_stats.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {
namespace {

void CompareDoubleValue(const base::ListValue& list_value,
                        int index,
                        double expected_value) {
  double value;
  EXPECT_TRUE(list_value.GetDouble(index, &value));
  EXPECT_EQ(expected_value, value);
}

TEST(RenderingStatsTest, TimeDeltaListEmpty) {
  RenderingStats::TimeDeltaList time_delta_list;
  scoped_ptr<base::ListValue> list_value =
      time_delta_list.AsListValueInMilliseconds();
  EXPECT_TRUE(list_value->empty());
  EXPECT_EQ(0ul, list_value->GetSize());
}

TEST(RenderingStatsTest, TimeDeltaListNonEmpty) {
  RenderingStats::TimeDeltaList time_delta_list;
  time_delta_list.Append(base::TimeDelta::FromMilliseconds(234));
  time_delta_list.Append(base::TimeDelta::FromMilliseconds(827));

  scoped_ptr<base::ListValue> list_value =
      time_delta_list.AsListValueInMilliseconds();
  EXPECT_FALSE(list_value->empty());
  EXPECT_EQ(2ul, list_value->GetSize());

  CompareDoubleValue(*list_value.get(), 0, 234);
  CompareDoubleValue(*list_value.get(), 1, 827);
}

TEST(RenderingStatsTest, TimeDeltaListAdd) {
  RenderingStats::TimeDeltaList time_delta_list_a;
  time_delta_list_a.Append(base::TimeDelta::FromMilliseconds(810));
  time_delta_list_a.Append(base::TimeDelta::FromMilliseconds(32));

  RenderingStats::TimeDeltaList time_delta_list_b;
  time_delta_list_b.Append(base::TimeDelta::FromMilliseconds(43));
  time_delta_list_b.Append(base::TimeDelta::FromMilliseconds(938));
  time_delta_list_b.Append(base::TimeDelta::FromMilliseconds(2));

  time_delta_list_a.Add(time_delta_list_b);
  scoped_ptr<base::ListValue> list_value =
      time_delta_list_a.AsListValueInMilliseconds();
  EXPECT_FALSE(list_value->empty());
  EXPECT_EQ(5ul, list_value->GetSize());

  CompareDoubleValue(*list_value.get(), 0, 810);
  CompareDoubleValue(*list_value.get(), 1, 32);
  CompareDoubleValue(*list_value.get(), 2, 43);
  CompareDoubleValue(*list_value.get(), 3, 938);
  CompareDoubleValue(*list_value.get(), 4, 2);
}

}  // namespace
}  // namespace cc
