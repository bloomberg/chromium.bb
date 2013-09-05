// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/device_orientation/data_fetcher_shared_memory_base.h"

#include "base/logging.h"
#include "base/process/process_handle.h"
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
      : start_motion_(false, false),
        start_orientation_(false, false),
        stop_motion_(false, false),
        stop_orientation_(false, false),
        updated_motion_(false, false),
        updated_orientation_(false, false),
        motion_buffer_(NULL),
        orientation_buffer_(NULL) {
  }
  virtual ~FakeDataFetcher() { }

  bool Init(ConsumerType consumer_type, void* buffer) {
    EXPECT_TRUE(buffer);

    switch (consumer_type) {
      case CONSUMER_TYPE_MOTION:
        motion_buffer_ = static_cast<DeviceMotionHardwareBuffer*>(buffer);
        break;
      case CONSUMER_TYPE_ORIENTATION:
        orientation_buffer_ =
            static_cast<DeviceOrientationHardwareBuffer*>(buffer);
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

  void WaitForStart(ConsumerType consumer_type) {
    switch (consumer_type) {
      case CONSUMER_TYPE_MOTION:
        start_motion_.Wait();
        break;
      case CONSUMER_TYPE_ORIENTATION:
        start_orientation_.Wait();
        break;
    }
  }

  void WaitForStop(ConsumerType consumer_type) {
    switch (consumer_type) {
      case CONSUMER_TYPE_MOTION:
        stop_motion_.Wait();
        break;
      case CONSUMER_TYPE_ORIENTATION:
        stop_orientation_.Wait();
        break;
    }
  }

  void WaitForUpdate(ConsumerType consumer_type) {
    switch (consumer_type) {
      case CONSUMER_TYPE_MOTION:
        updated_motion_.Wait();
        break;
      case CONSUMER_TYPE_ORIENTATION:
        updated_orientation_.Wait();
        break;
    }
  }

 protected:
  base::WaitableEvent start_motion_;
  base::WaitableEvent start_orientation_;
  base::WaitableEvent stop_motion_;
  base::WaitableEvent stop_orientation_;
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

  virtual bool Start(ConsumerType consumer_type, void* buffer) OVERRIDE {
    base::SharedMemoryHandle handle = GetSharedMemoryHandleForProcess(
        consumer_type, base::GetCurrentProcessHandle());
    EXPECT_TRUE(base::SharedMemory::IsHandleValid(handle));

    Init(consumer_type, buffer);
    switch (consumer_type) {
      case CONSUMER_TYPE_MOTION:
        UpdateMotion();
        start_motion_.Signal();
        break;
      case CONSUMER_TYPE_ORIENTATION:
        UpdateOrientation();
        start_orientation_.Signal();
        break;
      default:
        return false;
    }
    return true;
  }

  virtual bool Stop(ConsumerType consumer_type) OVERRIDE {
    switch (consumer_type) {
      case CONSUMER_TYPE_MOTION:
        stop_motion_.Signal();
        break;
      case CONSUMER_TYPE_ORIENTATION:
        stop_orientation_.Signal();
        break;
      default:
        return false;
    }
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

  virtual bool Start(ConsumerType consumer_type, void* buffer) OVERRIDE {
    EXPECT_TRUE(base::MessageLoop::current() == GetPollingMessageLoop());
    base::SharedMemoryHandle handle = GetSharedMemoryHandleForProcess(
        consumer_type, base::GetCurrentProcessHandle());
    EXPECT_TRUE(base::SharedMemory::IsHandleValid(handle));

    Init(consumer_type, buffer);
    switch (consumer_type) {
      case CONSUMER_TYPE_MOTION:
        start_motion_.Signal();
        break;
      case CONSUMER_TYPE_ORIENTATION:
        start_orientation_.Signal();
        break;
      default:
        return false;
    }
    return true;
  }

  virtual bool Stop(ConsumerType consumer_type) OVERRIDE {
    EXPECT_TRUE(base::MessageLoop::current() == GetPollingMessageLoop());

    switch (consumer_type) {
      case CONSUMER_TYPE_MOTION:
        stop_motion_.Signal();
        break;
      case CONSUMER_TYPE_ORIENTATION:
        stop_orientation_.Signal();
        break;
      default:
        return false;
    }
    return true;
  }

  virtual void Fetch(unsigned consumer_bitmask) OVERRIDE {
    EXPECT_TRUE(base::MessageLoop::current() == GetPollingMessageLoop());
    EXPECT_TRUE(consumer_bitmask & CONSUMER_TYPE_ORIENTATION ||
                consumer_bitmask & CONSUMER_TYPE_MOTION);

    if (consumer_bitmask & CONSUMER_TYPE_ORIENTATION)
      UpdateOrientation();
    if (consumer_bitmask & CONSUMER_TYPE_MOTION)
      UpdateMotion();
  }

  virtual bool IsPolling() const OVERRIDE {
    return true;
  }

 private:

  DISALLOW_COPY_AND_ASSIGN(FakePollingDataFetcher);
};


TEST(DataFetcherSharedMemoryBaseTest, DoesStartMotion) {
  FakeNonPollingDataFetcher fake_data_fetcher;
  EXPECT_FALSE(fake_data_fetcher.IsPolling());

  EXPECT_TRUE(fake_data_fetcher.StartFetchingDeviceData(CONSUMER_TYPE_MOTION));
  fake_data_fetcher.WaitForStart(CONSUMER_TYPE_MOTION);

  EXPECT_EQ(kPeriodInMilliseconds,
      fake_data_fetcher.GetMotionBuffer()->data.interval);

  fake_data_fetcher.StopFetchingDeviceData(CONSUMER_TYPE_MOTION);
  fake_data_fetcher.WaitForStop(CONSUMER_TYPE_MOTION);
}

TEST(DataFetcherSharedMemoryBaseTest, DoesStartOrientation) {
  FakeNonPollingDataFetcher fake_data_fetcher;
  EXPECT_FALSE(fake_data_fetcher.IsPolling());

  EXPECT_TRUE(fake_data_fetcher.StartFetchingDeviceData(
      CONSUMER_TYPE_ORIENTATION));
  fake_data_fetcher.WaitForStart(CONSUMER_TYPE_ORIENTATION);

  EXPECT_EQ(1, fake_data_fetcher.GetOrientationBuffer()->data.alpha);

  fake_data_fetcher.StopFetchingDeviceData(CONSUMER_TYPE_ORIENTATION);
  fake_data_fetcher.WaitForStop(CONSUMER_TYPE_ORIENTATION);
}

TEST(DataFetcherSharedMemoryBaseTest, DoesPollMotion) {
  FakePollingDataFetcher fake_data_fetcher;
  EXPECT_TRUE(fake_data_fetcher.IsPolling());

  EXPECT_TRUE(fake_data_fetcher.StartFetchingDeviceData(CONSUMER_TYPE_MOTION));
  fake_data_fetcher.WaitForStart(CONSUMER_TYPE_MOTION);
  fake_data_fetcher.WaitForUpdate(CONSUMER_TYPE_MOTION);

  EXPECT_EQ(kPeriodInMilliseconds,
      fake_data_fetcher.GetMotionBuffer()->data.interval);

  fake_data_fetcher.StopFetchingDeviceData(CONSUMER_TYPE_MOTION);
  fake_data_fetcher.WaitForStop(CONSUMER_TYPE_MOTION);
}

TEST(DataFetcherSharedMemoryBaseTest, DoesPollOrientation) {
  FakePollingDataFetcher fake_data_fetcher;
  EXPECT_TRUE(fake_data_fetcher.IsPolling());

  EXPECT_TRUE(fake_data_fetcher.StartFetchingDeviceData(
      CONSUMER_TYPE_ORIENTATION));
  fake_data_fetcher.WaitForStart(CONSUMER_TYPE_ORIENTATION);
  fake_data_fetcher.WaitForUpdate(CONSUMER_TYPE_ORIENTATION);

  EXPECT_EQ(1, fake_data_fetcher.GetOrientationBuffer()->data.alpha);

  fake_data_fetcher.StopFetchingDeviceData(CONSUMER_TYPE_ORIENTATION);
  fake_data_fetcher.WaitForStop(CONSUMER_TYPE_ORIENTATION);
}

TEST(DataFetcherSharedMemoryBaseTest, DoesPollMotionAndOrientation) {
  FakePollingDataFetcher fake_data_fetcher;
  EXPECT_TRUE(fake_data_fetcher.IsPolling());

  EXPECT_TRUE(fake_data_fetcher.StartFetchingDeviceData(
      CONSUMER_TYPE_ORIENTATION));
  EXPECT_TRUE(fake_data_fetcher.StartFetchingDeviceData(
      CONSUMER_TYPE_MOTION));
  fake_data_fetcher.WaitForStart(CONSUMER_TYPE_ORIENTATION);
  fake_data_fetcher.WaitForStart(CONSUMER_TYPE_MOTION);

  fake_data_fetcher.WaitForUpdate(CONSUMER_TYPE_ORIENTATION);
  fake_data_fetcher.WaitForUpdate(CONSUMER_TYPE_MOTION);

  EXPECT_EQ(1, fake_data_fetcher.GetOrientationBuffer()->data.alpha);
  EXPECT_EQ(kPeriodInMilliseconds,
      fake_data_fetcher.GetMotionBuffer()->data.interval);

  fake_data_fetcher.StopFetchingDeviceData(CONSUMER_TYPE_ORIENTATION);
  fake_data_fetcher.StopFetchingDeviceData(CONSUMER_TYPE_MOTION);
  fake_data_fetcher.WaitForStop(CONSUMER_TYPE_ORIENTATION);
  fake_data_fetcher.WaitForStop(CONSUMER_TYPE_MOTION);
}

}  // namespace

}  // namespace content
