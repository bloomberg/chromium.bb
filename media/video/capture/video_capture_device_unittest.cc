// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/test_timeouts.h"
#include "base/threading/thread.h"
#include "media/video/capture/fake_video_capture_device.h"
#include "media/video/capture/video_capture_device.h"
#include "media/video/capture/video_capture_types.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_WIN)
#include "base/win/scoped_com_initializer.h"
#include "media/video/capture/win/video_capture_device_mf_win.h"
#endif

#if defined(OS_ANDROID)
#include "base/android/jni_android.h"
#include "media/video/capture/android/video_capture_device_android.h"
#endif

#if defined(OS_MACOSX)
// Mac/QTKit will always give you the size you ask for and this case will fail.
#define MAYBE_AllocateBadSize DISABLED_AllocateBadSize
// We will always get ARGB from the Mac/QTKit implementation.
#define MAYBE_CaptureMjpeg DISABLED_CaptureMjpeg
#elif defined(OS_WIN)
#define MAYBE_AllocateBadSize AllocateBadSize
// Windows currently uses DirectShow to convert from MJPEG and a raw format is
// always delivered.
#define MAYBE_CaptureMjpeg DISABLED_CaptureMjpeg
#elif defined(OS_ANDROID)
// TODO(wjia): enable those tests on Android.
// On Android, native camera (JAVA) delivers frames on UI thread which is the
// main thread for tests. This results in no frame received by
// VideoCaptureAndroid.
#define CaptureVGA DISABLED_CaptureVGA
#define Capture720p DISABLED_Capture720p
#define MAYBE_AllocateBadSize DISABLED_AllocateBadSize
#define ReAllocateCamera DISABLED_ReAllocateCamera
#define DeAllocateCameraWhileRunning DISABLED_DeAllocateCameraWhileRunning
#define DeAllocateCameraWhileRunning DISABLED_DeAllocateCameraWhileRunning
#define MAYBE_CaptureMjpeg DISABLED_CaptureMjpeg
#else
#define MAYBE_AllocateBadSize AllocateBadSize
#define MAYBE_CaptureMjpeg CaptureMjpeg
#endif

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::Return;
using ::testing::AtLeast;
using ::testing::SaveArg;

namespace media {

class MockFrameObserver : public media::VideoCaptureDevice::EventHandler {
 public:
  MOCK_METHOD0(ReserveOutputBuffer, scoped_refptr<media::VideoFrame>());
  MOCK_METHOD0(OnErr, void());
  MOCK_METHOD1(OnFrameInfo, void(const VideoCaptureCapability&));
  MOCK_METHOD1(OnFrameInfoChanged, void(const VideoCaptureCapability&));

  explicit MockFrameObserver(base::WaitableEvent* wait_event)
     : wait_event_(wait_event) {}

  virtual void OnError() OVERRIDE {
    OnErr();
  }

  virtual void OnIncomingCapturedFrame(
      const uint8* data,
      int length,
      base::Time timestamp,
      int rotation,
      bool flip_vert,
      bool flip_horiz) OVERRIDE {
    wait_event_->Signal();
  }

  virtual void OnIncomingCapturedVideoFrame(
      const scoped_refptr<media::VideoFrame>& frame,
      base::Time timestamp) OVERRIDE {
    wait_event_->Signal();
  }

 private:
  base::WaitableEvent* wait_event_;
};

class VideoCaptureDeviceTest : public testing::Test {
 public:
  VideoCaptureDeviceTest(): wait_event_(false, false) { }

  void PostQuitTask() {
    loop_->PostTask(FROM_HERE, base::MessageLoop::QuitClosure());
    loop_->Run();
  }

 protected:
  virtual void SetUp() {
    frame_observer_.reset(new MockFrameObserver(&wait_event_));
    loop_.reset(new base::MessageLoopForUI());
#if defined(OS_ANDROID)
    media::VideoCaptureDeviceAndroid::RegisterVideoCaptureDevice(
        base::android::AttachCurrentThread());
#endif
  }

  virtual void TearDown() {
  }

#if defined(OS_WIN)
  base::win::ScopedCOMInitializer initialize_com_;
#endif
  base::WaitableEvent wait_event_;
  scoped_ptr<MockFrameObserver> frame_observer_;
  VideoCaptureDevice::Names names_;
  scoped_ptr<base::MessageLoop> loop_;
};

TEST_F(VideoCaptureDeviceTest, OpenInvalidDevice) {
#if defined(OS_WIN)
  VideoCaptureDevice::Name::CaptureApiType api_type =
      VideoCaptureDeviceMFWin::PlatformSupported()
          ? VideoCaptureDevice::Name::MEDIA_FOUNDATION
          : VideoCaptureDevice::Name::DIRECT_SHOW;
  VideoCaptureDevice::Name device_name("jibberish", "jibberish", api_type);
#else
  VideoCaptureDevice::Name device_name("jibberish", "jibberish");
#endif
  VideoCaptureDevice* device = VideoCaptureDevice::Create(device_name);
  EXPECT_TRUE(device == NULL);
}

TEST_F(VideoCaptureDeviceTest, CaptureVGA) {
  VideoCaptureDevice::GetDeviceNames(&names_);
  if (!names_.size()) {
    DVLOG(1) << "No camera available. Exiting test.";
    return;
  }

  scoped_ptr<VideoCaptureDevice> device(
      VideoCaptureDevice::Create(names_.front()));
  ASSERT_FALSE(device.get() == NULL);
  DVLOG(1) << names_.front().id();
  // Get info about the new resolution.
  VideoCaptureCapability rx_capability;
  EXPECT_CALL(*frame_observer_, OnFrameInfo(_))
      .Times(1).WillOnce(SaveArg<0>(&rx_capability));

  EXPECT_CALL(*frame_observer_, OnErr())
      .Times(0);

  VideoCaptureCapability capture_format(640,
                                        480,
                                        30,
                                        VideoCaptureCapability::kI420,
                                        0,
                                        false,
                                        ConstantResolutionVideoCaptureDevice);
  device->Allocate(capture_format, frame_observer_.get());
  device->Start();
  // Get captured video frames.
  PostQuitTask();
  EXPECT_TRUE(wait_event_.TimedWait(TestTimeouts::action_max_timeout()));
  EXPECT_EQ(rx_capability.width, 640);
  EXPECT_EQ(rx_capability.height, 480);
  device->Stop();
  device->DeAllocate();
}

TEST_F(VideoCaptureDeviceTest, Capture720p) {
  VideoCaptureDevice::GetDeviceNames(&names_);
  if (!names_.size()) {
    DVLOG(1) << "No camera available. Exiting test.";
    return;
  }

  scoped_ptr<VideoCaptureDevice> device(
      VideoCaptureDevice::Create(names_.front()));
  ASSERT_FALSE(device.get() == NULL);

  // Get info about the new resolution.
  // We don't care about the resulting resolution or frame rate as it might
  // be different from one machine to the next.
  EXPECT_CALL(*frame_observer_, OnFrameInfo(_))
      .Times(1);

  EXPECT_CALL(*frame_observer_, OnErr())
      .Times(0);

  VideoCaptureCapability capture_format(1280,
                                        720,
                                        30,
                                        VideoCaptureCapability::kI420,
                                        0,
                                        false,
                                        ConstantResolutionVideoCaptureDevice);
  device->Allocate(capture_format, frame_observer_.get());
  device->Start();
  // Get captured video frames.
  PostQuitTask();
  EXPECT_TRUE(wait_event_.TimedWait(TestTimeouts::action_max_timeout()));
  device->Stop();
  device->DeAllocate();
}

TEST_F(VideoCaptureDeviceTest, MAYBE_AllocateBadSize) {
  VideoCaptureDevice::GetDeviceNames(&names_);
  if (!names_.size()) {
    DVLOG(1) << "No camera available. Exiting test.";
    return;
  }
  scoped_ptr<VideoCaptureDevice> device(
      VideoCaptureDevice::Create(names_.front()));
  ASSERT_TRUE(device.get() != NULL);

  EXPECT_CALL(*frame_observer_, OnErr())
      .Times(0);

  // Get info about the new resolution.
  VideoCaptureCapability rx_capability;
  EXPECT_CALL(*frame_observer_, OnFrameInfo(_))
      .Times(AtLeast(1)).WillOnce(SaveArg<0>(&rx_capability));

  VideoCaptureCapability capture_format(637,
                                        472,
                                        35,
                                        VideoCaptureCapability::kI420,
                                        0,
                                        false,
                                        ConstantResolutionVideoCaptureDevice);
  device->Allocate(capture_format, frame_observer_.get());
  device->DeAllocate();
  EXPECT_EQ(rx_capability.width, 640);
  EXPECT_EQ(rx_capability.height, 480);
}

TEST_F(VideoCaptureDeviceTest, ReAllocateCamera) {
  VideoCaptureDevice::GetDeviceNames(&names_);
  if (!names_.size()) {
    DVLOG(1) << "No camera available. Exiting test.";
    return;
  }
  scoped_ptr<VideoCaptureDevice> device(
      VideoCaptureDevice::Create(names_.front()));
  ASSERT_TRUE(device.get() != NULL);
  EXPECT_CALL(*frame_observer_, OnErr())
      .Times(0);
  // Get info about the new resolution.
  VideoCaptureCapability rx_capability_1;
  VideoCaptureCapability rx_capability_2;
  VideoCaptureCapability capture_format_1(640,
                                          480,
                                          30,
                                          VideoCaptureCapability::kI420,
                                          0,
                                          false,
                                          ConstantResolutionVideoCaptureDevice);
  VideoCaptureCapability capture_format_2(1280,
                                          1024,
                                          30,
                                          VideoCaptureCapability::kI420,
                                          0,
                                          false,
                                          ConstantResolutionVideoCaptureDevice);
  VideoCaptureCapability capture_format_3(320,
                                          240,
                                          30,
                                          VideoCaptureCapability::kI420,
                                          0,
                                          false,
                                          ConstantResolutionVideoCaptureDevice);

  EXPECT_CALL(*frame_observer_, OnFrameInfo(_))
      .WillOnce(SaveArg<0>(&rx_capability_1));
  device->Allocate(capture_format_1, frame_observer_.get());
  device->Start();
  // Nothing shall happen.
  device->Allocate(capture_format_2, frame_observer_.get());
  device->DeAllocate();
  // Allocate new size 320, 240
  EXPECT_CALL(*frame_observer_, OnFrameInfo(_))
      .WillOnce(SaveArg<0>(&rx_capability_2));
  device->Allocate(capture_format_3, frame_observer_.get());

  device->Start();
  // Get captured video frames.
  PostQuitTask();
  EXPECT_TRUE(wait_event_.TimedWait(TestTimeouts::action_max_timeout()));
  EXPECT_EQ(rx_capability_1.width, 640);
  EXPECT_EQ(rx_capability_1.height, 480);
  EXPECT_EQ(rx_capability_2.width, 320);
  EXPECT_EQ(rx_capability_2.height, 240);
  device->Stop();
  device->DeAllocate();
}

TEST_F(VideoCaptureDeviceTest, DeAllocateCameraWhileRunning) {
  VideoCaptureDevice::GetDeviceNames(&names_);
  if (!names_.size()) {
    DVLOG(1) << "No camera available. Exiting test.";
    return;
  }
  scoped_ptr<VideoCaptureDevice> device(
      VideoCaptureDevice::Create(names_.front()));
  ASSERT_TRUE(device.get() != NULL);

  EXPECT_CALL(*frame_observer_, OnErr())
      .Times(0);
  // Get info about the new resolution.
  VideoCaptureCapability rx_capability;
  EXPECT_CALL(*frame_observer_, OnFrameInfo(_))
      .WillOnce(SaveArg<0>(&rx_capability));

  VideoCaptureCapability capture_format(640,
                                        480,
                                        30,
                                        VideoCaptureCapability::kI420,
                                        0,
                                        false,
                                        ConstantResolutionVideoCaptureDevice);
  device->Allocate(capture_format, frame_observer_.get());

  device->Start();
  // Get captured video frames.
  PostQuitTask();
  EXPECT_TRUE(wait_event_.TimedWait(TestTimeouts::action_max_timeout()));
  EXPECT_EQ(rx_capability.width, 640);
  EXPECT_EQ(rx_capability.height, 480);
  EXPECT_EQ(rx_capability.frame_rate, 30);
  device->DeAllocate();
}

TEST_F(VideoCaptureDeviceTest, FakeCapture) {
  VideoCaptureDevice::Names names;

  FakeVideoCaptureDevice::GetDeviceNames(&names);

  ASSERT_GT(static_cast<int>(names.size()), 0);

  scoped_ptr<VideoCaptureDevice> device(
      FakeVideoCaptureDevice::Create(names.front()));
  ASSERT_TRUE(device.get() != NULL);

  // Get info about the new resolution.
  VideoCaptureCapability rx_capability;
  EXPECT_CALL(*frame_observer_, OnFrameInfo(_))
      .Times(1).WillOnce(SaveArg<0>(&rx_capability));

  EXPECT_CALL(*frame_observer_, OnErr())
      .Times(0);

  VideoCaptureCapability capture_format(640,
                                        480,
                                        30,
                                        VideoCaptureCapability::kI420,
                                        0,
                                        false,
                                        ConstantResolutionVideoCaptureDevice);
  device->Allocate(capture_format, frame_observer_.get());

  device->Start();
  EXPECT_TRUE(wait_event_.TimedWait(TestTimeouts::action_max_timeout()));
  EXPECT_EQ(rx_capability.width, 640);
  EXPECT_EQ(rx_capability.height, 480);
  EXPECT_EQ(rx_capability.frame_rate, 30);
  device->Stop();
  device->DeAllocate();
}

// Start the camera in 720p to capture MJPEG instead of a raw format.
TEST_F(VideoCaptureDeviceTest, MAYBE_CaptureMjpeg) {
  VideoCaptureDevice::GetDeviceNames(&names_);
  if (!names_.size()) {
    DVLOG(1) << "No camera available. Exiting test.";
    return;
  }
  scoped_ptr<VideoCaptureDevice> device(
      VideoCaptureDevice::Create(names_.front()));
  ASSERT_TRUE(device.get() != NULL);

  EXPECT_CALL(*frame_observer_, OnErr())
      .Times(0);
  // Verify we get MJPEG from the device. Not all devices can capture 1280x720
  // @ 30 fps, so we don't care about the exact resolution we get.
  VideoCaptureCapability rx_capability;
  EXPECT_CALL(*frame_observer_, OnFrameInfo(_))
      .WillOnce(SaveArg<0>(&rx_capability));

  VideoCaptureCapability capture_format(1280,
                                        720,
                                        30,
                                        VideoCaptureCapability::kMJPEG,
                                        0,
                                        false,
                                        ConstantResolutionVideoCaptureDevice);
  device->Allocate(capture_format, frame_observer_.get());

  device->Start();
  // Get captured video frames.
  PostQuitTask();
  EXPECT_TRUE(wait_event_.TimedWait(TestTimeouts::action_max_timeout()));
  EXPECT_EQ(rx_capability.color, VideoCaptureCapability::kMJPEG);
  device->DeAllocate();
}

TEST_F(VideoCaptureDeviceTest, FakeCaptureVariableResolution) {
  VideoCaptureDevice::Names names;

  FakeVideoCaptureDevice::GetDeviceNames(&names);
  media::VideoCaptureCapability capture_format;
  capture_format.width = 640;
  capture_format.height = 480;
  capture_format.frame_rate = 30;
  capture_format.frame_size_type = media::VariableResolutionVideoCaptureDevice;

  ASSERT_GT(static_cast<int>(names.size()), 0);

  scoped_ptr<VideoCaptureDevice> device(
      FakeVideoCaptureDevice::Create(names.front()));
  ASSERT_TRUE(device.get() != NULL);

  // Get info about the new resolution.
  EXPECT_CALL(*frame_observer_, OnFrameInfo(_))
      .Times(1);

  EXPECT_CALL(*frame_observer_, OnErr())
      .Times(0);

  device->Allocate(capture_format, frame_observer_.get());

  // The amount of times the OnFrameInfoChanged gets called depends on how often
  // FakeDevice is supposed to change and what is its actual frame rate.
  // We set TimeWait to 200 action timeouts and this should be enough for at
  // least action_count/kFakeCaptureCapabilityChangePeriod calls.
  int action_count = 200;
  EXPECT_CALL(*frame_observer_, OnFrameInfoChanged(_))
      .Times(AtLeast(action_count / 30));
  device->Start();
  for (int i = 0; i < action_count; ++i) {
    EXPECT_TRUE(wait_event_.TimedWait(TestTimeouts::action_timeout()));
  }
  device->Stop();
  device->DeAllocate();
}

};  // namespace media
