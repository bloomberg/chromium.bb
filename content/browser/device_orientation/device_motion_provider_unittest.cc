// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/device_orientation/device_motion_provider.h"

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/synchronization/waitable_event.h"
#include "content/browser/device_orientation/data_fetcher_shared_memory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

const int kPeriodInMilliseconds = 100;

class FakeDataFetcherSharedMemory : public DataFetcherSharedMemory {
 public:
  FakeDataFetcherSharedMemory()
      : start_fetching_data_(false, false),
        stop_fetching_data_(false, false),
        fetched_data_(false, false) {
  }
  virtual ~FakeDataFetcherSharedMemory() { }

  virtual bool NeedsPolling() OVERRIDE {
    return true;
  }

  virtual bool FetchDeviceMotionDataIntoBuffer() OVERRIDE {
    buffer_->seqlock.WriteBegin();
    buffer_->data.interval = kPeriodInMilliseconds;
    buffer_->seqlock.WriteEnd();
    fetched_data_.Signal();
    return true;
  }

  virtual bool StartFetchingDeviceMotionData(
    DeviceMotionHardwareBuffer* buffer) OVERRIDE {
    buffer_ = buffer;
    start_fetching_data_.Signal();
    return true;
  }

  virtual void StopFetchingDeviceMotionData() OVERRIDE {
    stop_fetching_data_.Signal();
  }

  void WaitForStart() {
    start_fetching_data_.Wait();
  }

  void WaitForStop() {
    stop_fetching_data_.Wait();
  }

  void WaitForDataFetch() {
    fetched_data_.Wait();
  }

  DeviceMotionHardwareBuffer* GetBuffer() {
    return buffer_;
  }

 private:
  base::WaitableEvent start_fetching_data_;
  base::WaitableEvent stop_fetching_data_;
  base::WaitableEvent fetched_data_;
  DeviceMotionHardwareBuffer* buffer_;

  DISALLOW_COPY_AND_ASSIGN(FakeDataFetcherSharedMemory);
};


TEST(DeviceMotionProviderTest, DoesPolling) {
  FakeDataFetcherSharedMemory* mock_data_fetcher =
      new FakeDataFetcherSharedMemory();
  EXPECT_TRUE(mock_data_fetcher->NeedsPolling());

  scoped_ptr<DeviceMotionProvider> provider(new DeviceMotionProvider(
      scoped_ptr<DataFetcherSharedMemory>(mock_data_fetcher)));

  provider->StartFetchingDeviceMotionData();
  mock_data_fetcher->WaitForStart();
  mock_data_fetcher->WaitForDataFetch();

  EXPECT_EQ(kPeriodInMilliseconds,
      mock_data_fetcher->GetBuffer()->data.interval);

  provider->StopFetchingDeviceMotionData();
  mock_data_fetcher->WaitForStop();
}

}  // namespace

}  // namespace content
