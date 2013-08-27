// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/device_orientation/data_fetcher_shared_memory_base.h"

#include "base/logging.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread.h"
#include "content/common/device_motion_hardware_buffer.h"
#include "content/common/device_orientation/device_orientation_hardware_buffer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

const int kPeriodInMilliseconds = 100;


class FakeDataFetcher : public DataFetcherSharedMemoryBase {
 public:
  FakeDataFetcher()
      : start_(false, false),
        stop_(false, false),
        updated_motion_(false, false),
        updated_orientation_(false, false) {
  }
  virtual ~FakeDataFetcher() { }

  bool Init(ConsumerType consumer_type) {
    switch (consumer_type) {
      case CONSUMER_TYPE_MOTION:
        motion_buffer_ = static_cast<DeviceMotionHardwareBuffer*>(
             InitSharedMemoryBuffer(consumer_type,
                 sizeof(DeviceMotionHardwareBuffer)));
        break;
      case CONSUMER_TYPE_ORIENTATION:
        orientation_buffer_ = static_cast<DeviceOrientationHardwareBuffer*>(
            InitSharedMemoryBuffer(consumer_type,
                sizeof(DeviceOrientationHardwareBuffer)));
        break;
      default:
        return false;
    }
    return true;
  }

  void UpdateMotion() {
    DeviceMotionHardwareBuffer* buffer = GetMotionBuffer();
    ASSERT_TRUE(buffer);
    buffer->seqlock.WriteBegin();
    buffer->data.interval = kPeriodInMilliseconds;
    buffer->seqlock.WriteEnd();
    updated_motion_.Signal();
  }

  void UpdateOrientation() {
    DeviceOrientationHardwareBuffer* buffer = GetOrientationBuffer();
    ASSERT_TRUE(buffer);
    buffer->seqlock.WriteBegin();
    buffer->data.alpha = 1;
    buffer->seqlock.WriteEnd();
    updated_orientation_.Signal();
  }

  DeviceMotionHardwareBuffer* GetMotionBuffer() const {
    return motion_buffer_;
  }

  DeviceOrientationHardwareBuffer* GetOrientationBuffer() const {
    return orientation_buffer_;
  }

  void WaitForStart() {
    start_.Wait();
  }

  void WaitForStop() {
    stop_.Wait();
  }

  void WaitForUpdateMotion() {
    updated_motion_.Wait();
  }

  void WaitForUpdateOrientation() {
    updated_orientation_.Wait();
  }

 protected:
  base::WaitableEvent start_;
  base::WaitableEvent stop_;
  base::WaitableEvent updated_motion_;
  base::WaitableEvent updated_orientation_;

 private:
  DeviceMotionHardwareBuffer* motion_buffer_;
  DeviceOrientationHardwareBuffer* orientation_buffer_;

  DISALLOW_COPY_AND_ASSIGN(FakeDataFetcher);
};

class FakeNonPollingDataFetcher : public FakeDataFetcher {
 public:
  FakeNonPollingDataFetcher() { }
  virtual ~FakeNonPollingDataFetcher() { }

  virtual bool Start(ConsumerType consumer_type) OVERRIDE {
    Init(consumer_type);
    switch (consumer_type) {
      case CONSUMER_TYPE_MOTION:
        UpdateMotion();
        break;
      case CONSUMER_TYPE_ORIENTATION:
        UpdateOrientation();
        break;
      default:
        return false;
    }
    start_.Signal();
    return true;
  }

  virtual bool Stop(ConsumerType consumer_type) OVERRIDE {
    stop_.Signal();
    return true;
  }

  virtual bool IsPolling() const OVERRIDE {
    return false;
  }

  virtual void Fetch(unsigned consumer_bitmask) OVERRIDE {
    FAIL() << "fetch should not be called, "
        << "because this is a non-polling fetcher";
  }

 private:

  DISALLOW_COPY_AND_ASSIGN(FakeNonPollingDataFetcher);
};

class FakePollingDataFetcher : public FakeDataFetcher {
 public:
  FakePollingDataFetcher() { }
  virtual ~FakePollingDataFetcher() { }

  virtual bool Start(ConsumerType consumer_type) OVERRIDE {
    EXPECT_TRUE(base::MessageLoop::current() == GetPollingMessageLoop());
    Init(consumer_type);
    start_.Signal();
    return true;
  }

  virtual bool Stop(ConsumerType consumer_type) OVERRIDE {
    EXPECT_TRUE(base::MessageLoop::current() == GetPollingMessageLoop());
    stop_.Signal();
    return true;
  }

  virtual void Fetch(unsigned consumer_bitmask) OVERRIDE {
    EXPECT_TRUE(base::MessageLoop::current() == GetPollingMessageLoop());

    if (consumer_bitmask & CONSUMER_TYPE_ORIENTATION)
      UpdateOrientation();
    else if (consumer_bitmask & CONSUMER_TYPE_MOTION)
      UpdateMotion();
    else
      FAIL() << "wrong consumer bitmask";
  }

  virtual bool IsPolling() const OVERRIDE {
    return true;
  }

 private:

  DISALLOW_COPY_AND_ASSIGN(FakePollingDataFetcher);
};


TEST(DataFetcherSharedMemoryBaseTest, DoesStartMotion) {
  FakeNonPollingDataFetcher mock_data_fetcher;
  EXPECT_FALSE(mock_data_fetcher.IsPolling());

  EXPECT_TRUE(mock_data_fetcher.StartFetchingDeviceData(CONSUMER_TYPE_MOTION));
  mock_data_fetcher.WaitForStart();

  EXPECT_EQ(kPeriodInMilliseconds,
      mock_data_fetcher.GetMotionBuffer()->data.interval);

  mock_data_fetcher.StopFetchingDeviceData(CONSUMER_TYPE_MOTION);
  mock_data_fetcher.WaitForStop();
}

TEST(DataFetcherSharedMemoryBaseTest, DoesStartOrientation) {
  FakeNonPollingDataFetcher mock_data_fetcher;
  EXPECT_FALSE(mock_data_fetcher.IsPolling());

  EXPECT_TRUE(mock_data_fetcher.StartFetchingDeviceData(
      CONSUMER_TYPE_ORIENTATION));
  mock_data_fetcher.WaitForStart();

  EXPECT_EQ(1, mock_data_fetcher.GetOrientationBuffer()->data.alpha);

  mock_data_fetcher.StopFetchingDeviceData(CONSUMER_TYPE_ORIENTATION);
  mock_data_fetcher.WaitForStop();
}

TEST(DataFetcherSharedMemoryBaseTest, DoesPollMotion) {
  FakePollingDataFetcher mock_data_fetcher;
  EXPECT_TRUE(mock_data_fetcher.IsPolling());

  EXPECT_TRUE(mock_data_fetcher.StartFetchingDeviceData(CONSUMER_TYPE_MOTION));
  mock_data_fetcher.WaitForStart();
  mock_data_fetcher.WaitForUpdateMotion();

  EXPECT_EQ(kPeriodInMilliseconds,
      mock_data_fetcher.GetMotionBuffer()->data.interval);

  mock_data_fetcher.StopFetchingDeviceData(CONSUMER_TYPE_MOTION);
  mock_data_fetcher.WaitForStop();
}

TEST(DataFetcherSharedMemoryBaseTest, DoesPollOrientation) {
  FakePollingDataFetcher mock_data_fetcher;
  EXPECT_TRUE(mock_data_fetcher.IsPolling());

  EXPECT_TRUE(mock_data_fetcher.StartFetchingDeviceData(
      CONSUMER_TYPE_ORIENTATION));
  mock_data_fetcher.WaitForStart();
  mock_data_fetcher.WaitForUpdateOrientation();

  EXPECT_EQ(1, mock_data_fetcher.GetOrientationBuffer()->data.alpha);

  mock_data_fetcher.StopFetchingDeviceData(CONSUMER_TYPE_ORIENTATION);
  mock_data_fetcher.WaitForStop();
}

}  // namespace

}  // namespace content
