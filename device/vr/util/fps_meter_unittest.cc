// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/util/fps_meter.h"

#include "base/macros.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device {

namespace {

static constexpr double kTolerance = 0.01;

base::TimeTicks MicrosecondsToTicks(uint64_t us) {
  return base::TimeDelta::FromMicroseconds(us) + base::TimeTicks();
}

}  // namespace

TEST(FPSMeter, GetFPSWithTooFewFrames) {
  FPSMeter meter;
  EXPECT_FALSE(meter.CanComputeFPS());
  EXPECT_FLOAT_EQ(0.0, meter.GetFPS());

  meter.AddFrame(MicrosecondsToTicks(16000));
  EXPECT_FALSE(meter.CanComputeFPS());
  EXPECT_FLOAT_EQ(0.0, meter.GetFPS());

  meter.AddFrame(MicrosecondsToTicks(32000));
  EXPECT_TRUE(meter.CanComputeFPS());
  EXPECT_LT(0.0, meter.GetFPS());
}

TEST(FPSMeter, AccurateFPSWithManyFrames) {
  FPSMeter meter;
  EXPECT_FALSE(meter.CanComputeFPS());
  EXPECT_FLOAT_EQ(0.0, meter.GetFPS());

  base::TimeTicks now = MicrosecondsToTicks(1);
  base::TimeDelta frame_time = base::TimeDelta::FromMicroseconds(16666);

  meter.AddFrame(now);
  EXPECT_FALSE(meter.CanComputeFPS());
  EXPECT_FLOAT_EQ(0.0, meter.GetFPS());

  for (size_t i = 0; i < 2 * meter.GetNumFrameTimes(); ++i) {
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

  base::TimeTicks now = MicrosecondsToTicks(1);
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

}  // namespace device
