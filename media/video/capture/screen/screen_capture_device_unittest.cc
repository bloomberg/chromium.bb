// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/video/capture/screen/screen_capture_device.h"

#include "base/basictypes.h"
#include "base/sequenced_task_runner.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/test_timeouts.h"
#include "base/threading/sequenced_worker_pool.h"
#include "base/time.h"
#include "media/video/capture/screen/screen_capture_data.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::DoAll;
using ::testing::InvokeWithoutArgs;
using ::testing::SaveArg;

namespace media {

namespace {

const int kTestFrameWidth1 = 100;
const int kTestFrameHeight1 = 100;
const int kTestFrameWidth2 = 200;
const int kTestFrameHeight2 = 150;
const int kBufferSize = kTestFrameWidth2 * kTestFrameHeight2 * 4;

const int kFrameRate = 30;

class MockFrameObserver : public VideoCaptureDevice::EventHandler {
 public:
  MOCK_METHOD0(OnError, void());
  MOCK_METHOD1(OnFrameInfo, void(const VideoCaptureCapability& info));
  MOCK_METHOD6(OnIncomingCapturedFrame, void(const uint8* data,
                                             int length,
                                             base::Time timestamp,
                                             int rotation,
                                             bool flip_vert,
                                             bool flip_horiz));
  MOCK_METHOD2(OnIncomingCapturedVideoFrame, void(media::VideoFrame* frame,
                                                  base::Time timestamp));
};

// TODO(sergeyu): Move this to a separate file where it can be reused.
class FakeScreenCapturer : public ScreenCapturer {
 public:
  FakeScreenCapturer()
      : delegate_(NULL),
        frame_index_(0) {
    buffer_.reset(new uint8[kBufferSize]);
    frames_[0] = new ScreenCaptureData(
        buffer_.get(), kTestFrameWidth1 * ScreenCaptureData::kBytesPerPixel,
        SkISize::Make(kTestFrameWidth1, kTestFrameHeight1));
    frames_[1] = new ScreenCaptureData(
        buffer_.get(), kTestFrameWidth2 * ScreenCaptureData::kBytesPerPixel,
        SkISize::Make(kTestFrameWidth2, kTestFrameHeight2));
  }
  virtual ~FakeScreenCapturer() {}

  // VideoFrameCapturer interface.
  virtual void Start(Delegate* delegate) OVERRIDE {
    delegate_ = delegate;
  }
  virtual void Stop() OVERRIDE {
    delegate_ = NULL;
  }
  virtual void InvalidateRegion(const SkRegion& invalid_region) OVERRIDE {
    NOTIMPLEMENTED();
  }
  virtual void CaptureFrame() OVERRIDE {
    scoped_refptr<ScreenCaptureData> frame =
        frames_[frame_index_ % arraysize(frames_)];
    frame_index_++;
    delegate_->OnCaptureCompleted(frame);
  }

 private:
  Delegate* delegate_;
  scoped_array<uint8> buffer_;
  scoped_refptr<ScreenCaptureData> frames_[2];
  int frame_index_;
};

class ScreenCaptureDeviceTest : public testing::Test {
 public:
  virtual void SetUp() OVERRIDE {
    worker_pool_ = new base::SequencedWorkerPool(3, "TestCaptureThread");
  }

 protected:
  scoped_refptr<base::SequencedWorkerPool> worker_pool_;
};

}  // namespace

TEST_F(ScreenCaptureDeviceTest, Capture) {
  ScreenCaptureDevice capture_device(
      worker_pool_->GetSequencedTaskRunner(worker_pool_->GetSequenceToken()));
  VideoCaptureCapability caps;
  base::WaitableEvent done_event(false, false);
  int frame_size;

  MockFrameObserver frame_observer;
  EXPECT_CALL(frame_observer, OnFrameInfo(_))
      .WillOnce(SaveArg<0>(&caps));
  EXPECT_CALL(frame_observer, OnError())
      .Times(0);
  EXPECT_CALL(frame_observer, OnIncomingCapturedFrame(_, _, _, _, _, _))
      .WillRepeatedly(DoAll(
          SaveArg<1>(&frame_size),
          InvokeWithoutArgs(&done_event, &base::WaitableEvent::Signal)));

  capture_device.Allocate(640, 480, kFrameRate, &frame_observer);
  capture_device.Start();
  EXPECT_TRUE(done_event.TimedWait(TestTimeouts::action_max_timeout()));
  capture_device.Stop();
  capture_device.DeAllocate();

  EXPECT_GT(caps.width, 0);
  EXPECT_GT(caps.height, 0);
  EXPECT_EQ(kFrameRate, caps.frame_rate);
  EXPECT_EQ(VideoCaptureCapability::kARGB, caps.color);
  EXPECT_FALSE(caps.interlaced);

  EXPECT_EQ(caps.width * caps.height * 4, frame_size);
}

// Test that screen capturer can handle resolution change without crashing.
TEST_F(ScreenCaptureDeviceTest, ScreenResolutionChange) {
  FakeScreenCapturer* mock_capturer = new FakeScreenCapturer();

  ScreenCaptureDevice capture_device(
      worker_pool_->GetSequencedTaskRunner(worker_pool_->GetSequenceToken()));
  capture_device.SetScreenCapturerForTest(
      scoped_ptr<ScreenCapturer>(mock_capturer));

  VideoCaptureCapability caps;
  base::WaitableEvent done_event(false, false);
  int frame_size;

  MockFrameObserver frame_observer;
  EXPECT_CALL(frame_observer, OnFrameInfo(_))
      .WillOnce(SaveArg<0>(&caps));
  EXPECT_CALL(frame_observer, OnError())
      .Times(0);
  EXPECT_CALL(frame_observer, OnIncomingCapturedFrame(_, _, _, _, _, _))
      .WillRepeatedly(DoAll(
          SaveArg<1>(&frame_size),
          InvokeWithoutArgs(&done_event, &base::WaitableEvent::Signal)));

  capture_device.Allocate(640, 480, kFrameRate, &frame_observer);
  capture_device.Start();
  // Capture first frame.
  EXPECT_TRUE(done_event.TimedWait(TestTimeouts::action_max_timeout()));
  done_event.Reset();
  // Capture second frame.
  EXPECT_TRUE(done_event.TimedWait(TestTimeouts::action_max_timeout()));
  capture_device.Stop();
  capture_device.DeAllocate();

  EXPECT_EQ(kTestFrameWidth1, caps.width);
  EXPECT_EQ(kTestFrameHeight1, caps.height);
  EXPECT_EQ(kFrameRate, caps.frame_rate);
  EXPECT_EQ(VideoCaptureCapability::kARGB, caps.color);
  EXPECT_FALSE(caps.interlaced);

  EXPECT_EQ(caps.width * caps.height * 4, frame_size);
}

}  // namespace media
