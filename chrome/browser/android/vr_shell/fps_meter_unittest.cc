// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/vr_shell/fps_meter.h"

#include "base/macros.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace vr_shell {

namespace {

static constexpr double kTolerance = 0.01;

base::TimeTicks usToTicks(uint64_t us) {
  return base::TimeTicks::FromInternalValue(us);
}

base::TimeDelta usToDelta(uint64_t us) {
  return base::TimeDelta::FromInternalValue(us);
}

}  // namespace

TEST(FPSMeter, GetFPSWithTooFewFrames) {
  FPSMeter meter;
  EXPECT_FALSE(meter.CanComputeFPS());
  EXPECT_FLOAT_EQ(0.0, meter.GetFPS());

  meter.AddFrame(usToTicks(16000));
  EXPECT_FALSE(meter.CanComputeFPS());
  EXPECT_FLOAT_EQ(0.0, meter.GetFPS());

  meter.AddFrame(usToTicks(32000));
  EXPECT_TRUE(meter.CanComputeFPS());
  EXPECT_LT(0.0, meter.GetFPS());
}

TEST(FPSMeter, AccurateFPSWithManyFrames) {
  FPSMeter meter;
  EXPECT_FALSE(meter.CanComputeFPS());
  EXPECT_FLOAT_EQ(0.0, meter.GetFPS());

  base::TimeTicks now = usToTicks(1);
  base::TimeDelta frame_time = usToDelta(16666);

  meter.AddFrame(now);
  EXPECT_FALSE(meter.CanComputeFPS());
  EXPECT_FLOAT_EQ(0.0, meter.GetFPS());

  for (int i = 0; i < 5; ++i) {
    now += frame_time;
    meter.AddFrame(now);
    EXPECT_TRUE(meter.CanComputeFPS());
    EXPECT_NEAR(60.0, meter.GetFPS(), kTolerance);
  }
}

TEST(FPSMeter, AccurateFPSWithHigherFramerate) {
  FPSMeter meter;
  EXPECT_FALSE(meter.CanComputeFPS());
  EXPECT_FLOAT_EQ(0.0, meter.GetFPS());

  base::TimeTicks now = usToTicks(1);
  base::TimeDelta frame_time = base::TimeDelta::FromSecondsD(1.0 / 90.0);

  meter.AddFrame(now);
  EXPECT_FALSE(meter.CanComputeFPS());
  EXPECT_FLOAT_EQ(0.0, meter.GetFPS());

  for (int i = 0; i < 5; ++i) {
    now += frame_time;
    meter.AddFrame(now);
    EXPECT_TRUE(meter.CanComputeFPS());
    EXPECT_NEAR(1.0 / frame_time.InSecondsF(), meter.GetFPS(), kTolerance);
  }
}

}  // namespace vr_shell
