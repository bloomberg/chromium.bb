// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/device_orientation/data_fetcher_impl_android.h"

#include "base/android/jni_android.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

const int kPeriodInMilliseconds = 100;

class FakeDataFetcherImplAndroid : public DataFetcherImplAndroid {
 public:
  FakeDataFetcherImplAndroid() { }
  virtual ~FakeDataFetcherImplAndroid() { }

  virtual bool Start(DeviceData::Type event_type) OVERRIDE {
    return true;
  }

  virtual void Stop(DeviceData::Type event_type) OVERRIDE {
  }

  virtual int GetNumberActiveDeviceMotionSensors() OVERRIDE {
    return number_active_sensors_;
  }

  void SetNumberActiveDeviceMotionSensors(int number_active_sensors) {
    number_active_sensors_ = number_active_sensors;
  }

 private:
  int number_active_sensors_;
};

class AndroidDataFetcherTest : public testing::Test {
 protected:
  AndroidDataFetcherTest() {
    buffer_.reset(new DeviceMotionHardwareBuffer);
  }

  scoped_ptr<DeviceMotionHardwareBuffer> buffer_;
};

TEST_F(AndroidDataFetcherTest, ThreeDeviceMotionSensorsActive) {
  FakeDataFetcherImplAndroid::Register(base::android::AttachCurrentThread());
  FakeDataFetcherImplAndroid fetcher;
  fetcher.SetNumberActiveDeviceMotionSensors(3);

  fetcher.StartFetchingDeviceMotionData(buffer_.get());
  ASSERT_FALSE(buffer_->data.allAvailableSensorsAreActive);

  fetcher.GotAcceleration(0, 0, 1, 2, 3);
  ASSERT_FALSE(buffer_->data.allAvailableSensorsAreActive);

  fetcher.GotAccelerationIncludingGravity(0, 0, 1, 2, 3);
  ASSERT_FALSE(buffer_->data.allAvailableSensorsAreActive);

  fetcher.GotRotationRate(0, 0, 1, 2, 3);
  ASSERT_TRUE(buffer_->data.allAvailableSensorsAreActive);
  ASSERT_EQ(kPeriodInMilliseconds, buffer_->data.interval);

  fetcher.StopFetchingDeviceMotionData();
  ASSERT_FALSE(buffer_->data.allAvailableSensorsAreActive);
}

TEST_F(AndroidDataFetcherTest, TwoDeviceMotionSensorsActive) {
  FakeDataFetcherImplAndroid::Register(base::android::AttachCurrentThread());
  FakeDataFetcherImplAndroid fetcher;
  fetcher.SetNumberActiveDeviceMotionSensors(2);

  fetcher.StartFetchingDeviceMotionData(buffer_.get());
  ASSERT_FALSE(buffer_->data.allAvailableSensorsAreActive);

  fetcher.GotAcceleration(0, 0, 1, 2, 3);
  ASSERT_FALSE(buffer_->data.allAvailableSensorsAreActive);

  fetcher.GotAccelerationIncludingGravity(0, 0, 1, 2, 3);
  ASSERT_TRUE(buffer_->data.allAvailableSensorsAreActive);
  ASSERT_EQ(kPeriodInMilliseconds, buffer_->data.interval);

  fetcher.StopFetchingDeviceMotionData();
  ASSERT_FALSE(buffer_->data.allAvailableSensorsAreActive);
}

TEST_F(AndroidDataFetcherTest, ZeroDeviceMotionSensorsActive) {
  FakeDataFetcherImplAndroid::Register(base::android::AttachCurrentThread());
  FakeDataFetcherImplAndroid fetcher;
  fetcher.SetNumberActiveDeviceMotionSensors(0);

  fetcher.StartFetchingDeviceMotionData(buffer_.get());
  ASSERT_TRUE(buffer_->data.allAvailableSensorsAreActive);
  ASSERT_EQ(kPeriodInMilliseconds, buffer_->data.interval);

  fetcher.StopFetchingDeviceMotionData();
  ASSERT_FALSE(buffer_->data.allAvailableSensorsAreActive);
}

}  // namespace

}  // namespace content
