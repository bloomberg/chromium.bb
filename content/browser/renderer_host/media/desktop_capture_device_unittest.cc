// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/desktop_capture_device.h"

#include "base/basictypes.h"
#include "base/sequenced_task_runner.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/test_timeouts.h"
#include "base/threading/sequenced_worker_pool.h"
#include "base/time/time.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_frame.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_geometry.h"
#include "third_party/webrtc/modules/desktop_capture/screen_capturer.h"

using ::testing::_;
using ::testing::DoAll;
using ::testing::InvokeWithoutArgs;
using ::testing::SaveArg;

namespace content {

namespace {

const int kTestFrameWidth1 = 100;
const int kTestFrameHeight1 = 100;
const int kTestFrameWidth2 = 200;
const int kTestFrameHeight2 = 150;
const int kBufferSize = kTestFrameWidth2 * kTestFrameHeight2 * 4;

const int kFrameRate = 30;

class MockFrameObserver : public media::VideoCaptureDevice::EventHandler {
 public:
  MOCK_METHOD0(ReserveOutputBuffer, scoped_refptr<media::VideoFrame>());
  MOCK_METHOD0(OnError, void());
  MOCK_METHOD1(OnFrameInfo, void(const media::VideoCaptureCapability& info));
  MOCK_METHOD6(OnIncomingCapturedFrame, void(const uint8* data,
                                             int length,
                                             base::Time timestamp,
                                             int rotation,
                                             bool flip_vert,
                                             bool flip_horiz));
  MOCK_METHOD2(OnIncomingCapturedVideoFrame,
      void(const scoped_refptr<media::VideoFrame>& frame,
           base::Time timestamp));
};

// TODO(sergeyu): Move this to a separate file where it can be reused.
class FakeScreenCapturer : public webrtc::ScreenCapturer {
 public:
  FakeScreenCapturer()
      : callback_(NULL),
        frame_index_(0) {
  }
  virtual ~FakeScreenCapturer() {}

  // VideoFrameCapturer interface.
  virtual void Start(Callback* callback) OVERRIDE {
    callback_ = callback;
  }

  virtual void Capture(const webrtc::DesktopRegion& region) OVERRIDE {
    webrtc::DesktopSize size;
    if (frame_index_ % 2 == 0) {
      size = webrtc::DesktopSize(kTestFrameWidth1, kTestFrameHeight1);
    } else {
      size = webrtc::DesktopSize(kTestFrameWidth2, kTestFrameHeight2);
    }
    frame_index_++;
    callback_->OnCaptureCompleted(new webrtc::BasicDesktopFrame(size));
  }

  virtual void SetMouseShapeObserver(
      MouseShapeObserver* mouse_shape_observer) OVERRIDE {
  }

 private:
  Callback* callback_;
  int frame_index_;
};

class DesktopCaptureDeviceTest : public testing::Test {
 public:
  virtual void SetUp() OVERRIDE {
    worker_pool_ = new base::SequencedWorkerPool(3, "TestCaptureThread");
  }

 protected:
  scoped_refptr<base::SequencedWorkerPool> worker_pool_;
};

}  // namespace

// There is currently no screen capturer implementation for ozone. So disable
// the test that uses a real screen-capturer instead of FakeScreenCapturer.
// http://crbug.com/260318
#if defined(USE_OZONE)
#define MAYBE_Capture DISABLED_Capture
#else
#define MAYBE_Capture Capture
#endif
TEST_F(DesktopCaptureDeviceTest, MAYBE_Capture) {
  scoped_ptr<webrtc::DesktopCapturer> capturer(
      webrtc::ScreenCapturer::Create());
  DesktopCaptureDevice capture_device(
      worker_pool_->GetSequencedTaskRunner(worker_pool_->GetSequenceToken()),
      capturer.Pass());
  media::VideoCaptureCapability caps;
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

  media::VideoCaptureCapability capture_format(
      640, 480, kFrameRate, media::VideoCaptureCapability::kI420, 0, false,
      media::ConstantResolutionVideoCaptureDevice);
  capture_device.Allocate(capture_format, &frame_observer);
  capture_device.Start();
  EXPECT_TRUE(done_event.TimedWait(TestTimeouts::action_max_timeout()));
  capture_device.Stop();
  capture_device.DeAllocate();

  EXPECT_GT(caps.width, 0);
  EXPECT_GT(caps.height, 0);
  EXPECT_EQ(kFrameRate, caps.frame_rate);
  EXPECT_EQ(media::VideoCaptureCapability::kARGB, caps.color);
  EXPECT_FALSE(caps.interlaced);

  EXPECT_EQ(caps.width * caps.height * 4, frame_size);
}

// Test that screen capturer can handle resolution change without crashing.
TEST_F(DesktopCaptureDeviceTest, ScreenResolutionChange) {
  FakeScreenCapturer* mock_capturer = new FakeScreenCapturer();

  DesktopCaptureDevice capture_device(
      worker_pool_->GetSequencedTaskRunner(worker_pool_->GetSequenceToken()),
      scoped_ptr<webrtc::DesktopCapturer>(mock_capturer));

  media::VideoCaptureCapability caps;
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

  media::VideoCaptureCapability capture_format(
      kTestFrameWidth1,
      kTestFrameHeight1,
      kFrameRate,
      media::VideoCaptureCapability::kI420,
      0,
      false,
      media::ConstantResolutionVideoCaptureDevice);
  capture_device.Allocate(capture_format, &frame_observer);
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
  EXPECT_EQ(media::VideoCaptureCapability::kARGB, caps.color);
  EXPECT_FALSE(caps.interlaced);

  EXPECT_EQ(caps.width * caps.height * 4, frame_size);
}

}  // namespace content
