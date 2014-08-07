// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/test/simple_test_tick_clock.h"
#include "components/copresence/timed_map.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

struct Value {
  Value() : value(0) {}
  explicit Value(int new_value) : value(new_value) {}

  int value;
};

}  // namespace

class TimedMapTest : public testing::Test {
 public:
  TimedMapTest() {}

 private:
  // Exists since the timer needs a message loop.
  base::MessageLoop message_loop_;

  DISALLOW_COPY_AND_ASSIGN(TimedMapTest);
};

// TODO(rkc): Find and fix the memory leak here.
#define MAYBE_Basic DISABLED_Basic

TEST_F(TimedMapTest, MAYBE_Basic) {
  typedef copresence::TimedMap<int, Value> Map;
  Map map(base::TimeDelta::FromSeconds(9999), 3);

  EXPECT_FALSE(map.HasKey(0));
  EXPECT_EQ(0, map.GetValue(0).value);

  map.Add(0x1337, Value(0x7331));
  EXPECT_TRUE(map.HasKey(0x1337));
  EXPECT_EQ(0x7331, map.GetValue(0x1337).value);

  map.Add(0xbaad, Value(0xf00d));
  EXPECT_TRUE(map.HasKey(0xbaad));
  EXPECT_EQ(0xf00d, map.GetValue(0xbaad).value);
  EXPECT_EQ(0x7331, map.GetValue(0x1337).value);

  map.Add(0x1234, Value(0x5678));
  EXPECT_TRUE(map.HasKey(0x1234));
  EXPECT_TRUE(map.HasKey(0xbaad));
  EXPECT_TRUE(map.HasKey(0x1337));

  EXPECT_EQ(0x5678, map.GetValue(0x1234).value);
  EXPECT_EQ(0xf00d, map.GetValue(0xbaad).value);
  EXPECT_EQ(0x7331, map.GetValue(0x1337).value);
}

// TODO(rkc): Find and fix the memory leak here.
#define MAYBE_ValueReplacement DISABLED_ValueReplacement

TEST_F(TimedMapTest, MAYBE_ValueReplacement) {
  typedef copresence::TimedMap<int, Value> Map;
  Map map(base::TimeDelta::FromSeconds(9999), 10);

  map.Add(0x1337, Value(0x7331));
  EXPECT_TRUE(map.HasKey(0x1337));
  EXPECT_EQ(0x7331, map.GetValue(0x1337).value);

  map.Add(0xbaad, Value(0xf00d));
  EXPECT_TRUE(map.HasKey(0xbaad));
  EXPECT_EQ(0xf00d, map.GetValue(0xbaad).value);

  map.Add(0x1337, Value(0xd00d));
  EXPECT_TRUE(map.HasKey(0x1337));
  EXPECT_EQ(0xd00d, map.GetValue(0x1337).value);
}

// TODO(rkc): Find and fix the memory leak here.
#define MAYBE_SizeEvict DISABLED_SizeEvict

TEST_F(TimedMapTest, MAYBE_SizeEvict) {
  typedef copresence::TimedMap<int, Value> Map;
  Map two_element_map(base::TimeDelta::FromSeconds(9999), 2);

  two_element_map.Add(0x1337, Value(0x7331));
  EXPECT_TRUE(two_element_map.HasKey(0x1337));
  EXPECT_EQ(0x7331, two_element_map.GetValue(0x1337).value);

  two_element_map.Add(0xbaad, Value(0xf00d));
  EXPECT_TRUE(two_element_map.HasKey(0xbaad));
  EXPECT_EQ(0xf00d, two_element_map.GetValue(0xbaad).value);

  two_element_map.Add(0x1234, Value(0x5678));
  EXPECT_TRUE(two_element_map.HasKey(0x1234));
  EXPECT_EQ(0xf00d, two_element_map.GetValue(0xbaad).value);

  EXPECT_FALSE(two_element_map.HasKey(0x1337));
  EXPECT_EQ(0, two_element_map.GetValue(0x1337).value);
}

// TODO(rkc): Find and fix the memory leak here.
#define MAYBE_TimedEvict DISABLED_TimedEvict

TEST_F(TimedMapTest, MAYBE_TimedEvict) {
  const int kLargeTimeValueSeconds = 9999;
  base::SimpleTestTickClock clock;
  typedef copresence::TimedMap<int, Value> Map;
  Map map(base::TimeDelta::FromSeconds(kLargeTimeValueSeconds), 2);
  map.set_clock_for_testing(&clock);

  // Add value at T=0.
  map.Add(0x1337, Value(0x7331));
  EXPECT_TRUE(map.HasKey(0x1337));
  EXPECT_EQ(0x7331, map.GetValue(0x1337).value);

  // Add value at T=kLargeTimeValueSeconds-1.
  clock.Advance(base::TimeDelta::FromSeconds(kLargeTimeValueSeconds - 1));
  map.Add(0xbaad, Value(0xf00d));

  // Check values at T=kLargeTimeValueSeconds-1.
  EXPECT_TRUE(map.HasKey(0xbaad));
  EXPECT_EQ(0xf00d, map.GetValue(0xbaad).value);
  EXPECT_TRUE(map.HasKey(0x1337));
  EXPECT_EQ(0x7331, map.GetValue(0x1337).value);

  // Check values at T=kLargeTimeValueSeconds.
  clock.Advance(base::TimeDelta::FromSeconds(1));
  EXPECT_FALSE(map.HasKey(0x1337));
  EXPECT_EQ(0, map.GetValue(0x1337).value);
  EXPECT_TRUE(map.HasKey(0xbaad));
  EXPECT_EQ(0xf00d, map.GetValue(0xbaad).value);

  // Check values at T=2*kLargeTimeValueSeconds
  clock.Advance(base::TimeDelta::FromSeconds(kLargeTimeValueSeconds));
  EXPECT_FALSE(map.HasKey(0xbaad));
  EXPECT_EQ(0, map.GetValue(0xbaad).value);
}
