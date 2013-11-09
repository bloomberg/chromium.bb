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
using ::testing::AnyNumber;
using ::testing::DoAll;
using ::testing::Expectation;
using ::testing::InvokeWithoutArgs;
using ::testing::SaveArg;

namespace content {

namespace {

MATCHER_P2(EqualsCaptureCapability, width, height, "") {
  return arg.width == width && arg.height == height;
}

const int kTestFrameWidth1 = 100;
const int kTestFrameHeight1 = 100;
const int kTestFrameWidth2 = 200;
const int kTestFrameHeight2 = 150;

const int kFrameRate = 30;

class MockDeviceClient : public media::VideoCaptureDevice::Client {
 public:
  MOCK_METHOD1(ReserveOutputBuffer,
               scoped_refptr<media::VideoFrame>(const gfx::Size& size));
  MOCK_METHOD0(OnError, void());
  MOCK_METHOD7(OnIncomingCapturedFrame,
               void(const uint8* data,
                    int length,
                    base::Time timestamp,
                    int rotation,
                    bool flip_vert,
                    bool flip_horiz,
                    const media::VideoCaptureCapability& frame_info));
  MOCK_METHOD3(OnIncomingCapturedVideoFrame,
      void(const scoped_refptr<media::VideoFrame>& frame,
           base::Time timestamp,
           int frame_rate));
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

  scoped_ptr<MockDeviceClient> client(new MockDeviceClient());
  EXPECT_CALL(*client, OnError()).Times(0);
  EXPECT_CALL(*client, OnIncomingCapturedFrame(_, _, _, _, _, _, _))
      .WillRepeatedly(
           DoAll(SaveArg<1>(&frame_size),
                 SaveArg<6>(&caps),
                 InvokeWithoutArgs(&done_event, &base::WaitableEvent::Signal)));

  media::VideoCaptureCapability capture_format(
      640, 480, kFrameRate, media::PIXEL_FORMAT_I420,
      media::ConstantResolutionVideoCaptureDevice);
  capture_device.AllocateAndStart(
      capture_format, client.PassAs<media::VideoCaptureDevice::Client>());
  EXPECT_TRUE(done_event.TimedWait(TestTimeouts::action_max_timeout()));
  capture_device.StopAndDeAllocate();

  EXPECT_GT(caps.width, 0);
  EXPECT_GT(caps.height, 0);
  EXPECT_EQ(kFrameRate, caps.frame_rate);
  EXPECT_EQ(media::PIXEL_FORMAT_ARGB, caps.color);

  EXPECT_EQ(caps.width * caps.height * 4, frame_size);
  worker_pool_->FlushForTesting();
}

// Test that screen capturer behaves correctly if the source frame size changes
// but the caller cannot cope with variable resolution output.
TEST_F(DesktopCaptureDeviceTest, ScreenResolutionChangeConstantResolution) {
  FakeScreenCapturer* mock_capturer = new FakeScreenCapturer();

  DesktopCaptureDevice capture_device(
      worker_pool_->GetSequencedTaskRunner(worker_pool_->GetSequenceToken()),
      scoped_ptr<webrtc::DesktopCapturer>(mock_capturer));

  media::VideoCaptureCapability caps;
  base::WaitableEvent done_event(false, false);
  int frame_size;

  scoped_ptr<MockDeviceClient> client(new MockDeviceClient());
  EXPECT_CALL(*client, OnError()).Times(0);
  EXPECT_CALL(*client, OnIncomingCapturedFrame(_, _, _, _, _, _, _))
      .WillRepeatedly(
           DoAll(SaveArg<1>(&frame_size),
                 SaveArg<6>(&caps),
                 InvokeWithoutArgs(&done_event, &base::WaitableEvent::Signal)));

  media::VideoCaptureCapability capture_format(
      kTestFrameWidth1,
      kTestFrameHeight1,
      kFrameRate,
      media::PIXEL_FORMAT_I420,
      media::ConstantResolutionVideoCaptureDevice);

  capture_device.AllocateAndStart(
      capture_format, client.PassAs<media::VideoCaptureDevice::Client>());

  // Capture at least two frames, to ensure that the source frame size has
  // changed while capturing.
  EXPECT_TRUE(done_event.TimedWait(TestTimeouts::action_max_timeout()));
  done_event.Reset();
  EXPECT_TRUE(done_event.TimedWait(TestTimeouts::action_max_timeout()));

  capture_device.StopAndDeAllocate();

  EXPECT_EQ(kTestFrameWidth1, caps.width);
  EXPECT_EQ(kTestFrameHeight1, caps.height);
  EXPECT_EQ(kFrameRate, caps.frame_rate);
  EXPECT_EQ(media::PIXEL_FORMAT_ARGB, caps.color);

  EXPECT_EQ(caps.width * caps.height * 4, frame_size);
  worker_pool_->FlushForTesting();
}

// Test that screen capturer behaves correctly if the source frame size changes
// and the caller can cope with variable resolution output.
TEST_F(DesktopCaptureDeviceTest, ScreenResolutionChangeVariableResolution) {
  FakeScreenCapturer* mock_capturer = new FakeScreenCapturer();

  DesktopCaptureDevice capture_device(
      worker_pool_->GetSequencedTaskRunner(worker_pool_->GetSequenceToken()),
      scoped_ptr<webrtc::DesktopCapturer>(mock_capturer));

  media::VideoCaptureCapability caps;
  base::WaitableEvent done_event(false, false);

  scoped_ptr<MockDeviceClient> client(new MockDeviceClient());
  EXPECT_CALL(*client, OnError()).Times(0);
  EXPECT_CALL(*client, OnIncomingCapturedFrame(_, _, _, _, _, _, _))
      .WillRepeatedly(DoAll(
           SaveArg<6>(&caps),
           InvokeWithoutArgs(&done_event, &base::WaitableEvent::Signal)));

  media::VideoCaptureCapability capture_format(
      kTestFrameWidth2,
      kTestFrameHeight2,
      kFrameRate,
      media::PIXEL_FORMAT_I420,
      media::VariableResolutionVideoCaptureDevice);

  capture_device.AllocateAndStart(
      capture_format, client.PassAs<media::VideoCaptureDevice::Client>());

  // Capture at least three frames, to ensure that the source frame size has
  // changed at least twice while capturing.
  EXPECT_TRUE(done_event.TimedWait(TestTimeouts::action_max_timeout()));
  done_event.Reset();
  EXPECT_TRUE(done_event.TimedWait(TestTimeouts::action_max_timeout()));
  done_event.Reset();
  EXPECT_TRUE(done_event.TimedWait(TestTimeouts::action_max_timeout()));

  capture_device.StopAndDeAllocate();

  EXPECT_EQ(kTestFrameWidth1, caps.width);
  EXPECT_EQ(kTestFrameHeight1, caps.height);
  EXPECT_EQ(kFrameRate, caps.frame_rate);
  EXPECT_EQ(media::PIXEL_FORMAT_ARGB, caps.color);
  worker_pool_->FlushForTesting();
}

}  // namespace content
