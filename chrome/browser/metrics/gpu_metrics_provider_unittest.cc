// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/gpu_metrics_provider.h"

#include "base/basictypes.h"
#include "components/metrics/proto/chrome_user_metrics_extension.pb.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/size.h"

namespace {

const int kScreenWidth = 1024;
const int kScreenHeight = 768;
const int kScreenCount = 3;
const float kScreenScaleFactor = 2;

class TestGPUMetricsProvider : public GPUMetricsProvider {
 public:
  TestGPUMetricsProvider() {}
  virtual ~TestGPUMetricsProvider() {}

 private:
  virtual gfx::Size GetScreenSize() const OVERRIDE {
    return gfx::Size(kScreenWidth, kScreenHeight);
  }

  virtual float GetScreenDeviceScaleFactor() const OVERRIDE {
    return kScreenScaleFactor;
  }

  virtual int GetScreenCount() const OVERRIDE {
    return kScreenCount;
  }

  DISALLOW_COPY_AND_ASSIGN(TestGPUMetricsProvider);
};

}  // namespace

class GPUMetricsProviderTest : public testing::Test {
 public:
  GPUMetricsProviderTest() {}
  virtual ~GPUMetricsProviderTest() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(GPUMetricsProviderTest);
};

TEST_F(GPUMetricsProviderTest, ProvideSystemProfileMetrics) {
  TestGPUMetricsProvider provider;
  metrics::ChromeUserMetricsExtension uma_proto;

  provider.ProvideSystemProfileMetrics(uma_proto.mutable_system_profile());

  // Check that the system profile has the correct values set.
  const metrics::SystemProfileProto::Hardware& hardware =
      uma_proto.system_profile().hardware();
  EXPECT_EQ(kScreenWidth, hardware.primary_screen_width());
  EXPECT_EQ(kScreenHeight, hardware.primary_screen_height());
  EXPECT_EQ(kScreenScaleFactor, hardware.primary_screen_scale_factor());
  EXPECT_EQ(kScreenCount, hardware.screen_count());
}
