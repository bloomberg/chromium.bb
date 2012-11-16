// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/timing_function.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace cc {
namespace {

TEST(TimingFunctionTest, CubicBezierTimingFunction) {
  scoped_ptr<CubicBezierTimingFunction> function =
      CubicBezierTimingFunction::create(0.25, 0, 0.75, 1);

  double epsilon = 0.00015;

  EXPECT_NEAR(function->getValue(0), 0, epsilon);
  EXPECT_NEAR(function->getValue(0.05), 0.01136, epsilon);
  EXPECT_NEAR(function->getValue(0.1), 0.03978, epsilon);
  EXPECT_NEAR(function->getValue(0.15), 0.079780, epsilon);
  EXPECT_NEAR(function->getValue(0.2), 0.12803, epsilon);
  EXPECT_NEAR(function->getValue(0.25), 0.18235, epsilon);
  EXPECT_NEAR(function->getValue(0.3), 0.24115, epsilon);
  EXPECT_NEAR(function->getValue(0.35), 0.30323, epsilon);
  EXPECT_NEAR(function->getValue(0.4), 0.36761, epsilon);
  EXPECT_NEAR(function->getValue(0.45), 0.43345, epsilon);
  EXPECT_NEAR(function->getValue(0.5), 0.5, epsilon);
  EXPECT_NEAR(function->getValue(0.6), 0.63238, epsilon);
  EXPECT_NEAR(function->getValue(0.65), 0.69676, epsilon);
  EXPECT_NEAR(function->getValue(0.7), 0.75884, epsilon);
  EXPECT_NEAR(function->getValue(0.75), 0.81764, epsilon);
  EXPECT_NEAR(function->getValue(0.8), 0.87196, epsilon);
  EXPECT_NEAR(function->getValue(0.85), 0.92021, epsilon);
  EXPECT_NEAR(function->getValue(0.9), 0.96021, epsilon);
  EXPECT_NEAR(function->getValue(0.95), 0.98863, epsilon);
  EXPECT_NEAR(function->getValue(1), 1, epsilon);
}

}  // namespace
}  // namespace cc
