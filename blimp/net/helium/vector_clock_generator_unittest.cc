// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/net/helium/vector_clock_generator.h"

#include "base/macros.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blimp {
namespace {

class VectorClockGeneratorTest : public testing::Test {
 public:
  VectorClockGeneratorTest() {}
  ~VectorClockGeneratorTest() override {}

 protected:
  VectorClockGenerator gen_;

 private:
  DISALLOW_COPY_AND_ASSIGN(VectorClockGeneratorTest);
};

TEST_F(VectorClockGeneratorTest, CheckCurrentDoesntIncrease) {
  VectorClock c1 = gen_.current();
  EXPECT_EQ(VectorClock::Comparison::EqualTo, c1.CompareTo(gen_.current()));
}

TEST_F(VectorClockGeneratorTest, MonotonicallyIncreasing) {
  VectorClock c1 = gen_.current();
  gen_.Increment();
  VectorClock c2 = gen_.current();
  EXPECT_EQ(VectorClock::Comparison::GreaterThan, c2.CompareTo(c1));
  EXPECT_EQ(VectorClock::Comparison::EqualTo, c2.CompareTo(gen_.current()));
}

}  // namespace
}  // namespace blimp
