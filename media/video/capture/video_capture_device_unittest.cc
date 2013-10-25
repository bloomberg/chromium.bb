// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/run_loop.h"
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

class MockClient : public media::VideoCaptureDevice::Client {
 public:
  MOCK_METHOD1(ReserveOutputBuffer,
      scoped_refptr<media::VideoFrame>(const gfx::Size&));
  MOCK_METHOD0(OnErr, void());
  MOCK_METHOD1(OnFrameInfo, void(const VideoCaptureCapability&));
  MOCK_METHOD1(OnFrameInfoChanged, void(const VideoCaptureCapability&));

  explicit MockClient(
      base::Closure frame_cb)
      : main_thread_(base::MessageLoopProxy::current()),
        frame_cb_(frame_cb) {}

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
    main_thread_->PostTask(FROM_HERE, frame_cb_);
  }

  virtual void OnIncomingCapturedVideoFrame(
      const scoped_refptr<media::VideoFrame>& frame,
      base::Time timestamp) OVERRIDE {
    main_thread_->PostTask(FROM_HERE, frame_cb_);
  }

 private:
  scoped_refptr<base::MessageLoopProxy> main_thread_;
  base::Closure frame_cb_;
};

class VideoCaptureDeviceTest : public testing::Test {
 protected:
  typedef media::VideoCaptureDevice::Client Client;

  virtual void SetUp() {
    loop_.reset(new base::MessageLoopForUI());
    client_.reset(new MockClient(loop_->QuitClosure()));
#if defined(OS_ANDROID)
    media::VideoCaptureDeviceAndroid::RegisterVideoCaptureDevice(
        base::android::AttachCurrentThread());
#endif
  }

  void WaitForCapturedFrame() {
    loop_->Run();
  }

#if defined(OS_WIN)
  base::win::ScopedCOMInitializer initialize_com_;
#endif
  scoped_ptr<MockClient> client_;
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
  EXPECT_CALL(*client_, OnFrameInfo(_))
      .Times(1).WillOnce(SaveArg<0>(&rx_capability));

  EXPECT_CALL(*client_, OnErr())
      .Times(0);

  VideoCaptureCapability capture_format(640,
                                        480,
                                        30,
                                        PIXEL_FORMAT_I420,
                                        ConstantResolutionVideoCaptureDevice);
  device->AllocateAndStart(capture_format,
                           client_.PassAs<Client>());
  // Get captured video frames.
  loop_->Run();
  EXPECT_EQ(rx_capability.width, 640);
  EXPECT_EQ(rx_capability.height, 480);
  device->StopAndDeAllocate();
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
  EXPECT_CALL(*client_, OnFrameInfo(_))
      .Times(1);

  EXPECT_CALL(*client_, OnErr())
      .Times(0);

  VideoCaptureCapability capture_format(1280,
                                        720,
                                        30,
                                        PIXEL_FORMAT_I420,
                                        ConstantResolutionVideoCaptureDevice);
  device->AllocateAndStart(capture_format,
                           client_.PassAs<Client>());
  // Get captured video frames.
  WaitForCapturedFrame();
  device->StopAndDeAllocate();
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

  EXPECT_CALL(*client_, OnErr())
      .Times(0);

  // Get info about the new resolution.
  VideoCaptureCapability rx_capability;
  EXPECT_CALL(*client_, OnFrameInfo(_))
      .Times(AtLeast(1)).WillOnce(SaveArg<0>(&rx_capability));

  VideoCaptureCapability capture_format(637,
                                        472,
                                        35,
                                        PIXEL_FORMAT_I420,
                                        ConstantResolutionVideoCaptureDevice);
  device->AllocateAndStart(capture_format,
                           client_.PassAs<Client>());
  device->StopAndDeAllocate();
  EXPECT_EQ(rx_capability.width, 640);
  EXPECT_EQ(rx_capability.height, 480);
}

TEST_F(VideoCaptureDeviceTest, ReAllocateCamera) {
  VideoCaptureDevice::GetDeviceNames(&names_);
  if (!names_.size()) {
    DVLOG(1) << "No camera available. Exiting test.";
    return;
  }

  // First, do a number of very fast device start/stops.
  for (int i = 0; i <= 5; i++) {
    scoped_ptr<MockClient> client(
        new MockClient(base::Bind(&base::DoNothing)));
    scoped_ptr<VideoCaptureDevice> device(
        VideoCaptureDevice::Create(names_.front()));
    gfx::Size resolution;
    if (i % 2) {
      resolution = gfx::Size(640, 480);
    } else {
      resolution = gfx::Size(1280, 1024);
    }
    VideoCaptureCapability requested_format(
        resolution.width(),
        resolution.height(),
        30,
        PIXEL_FORMAT_I420,
        ConstantResolutionVideoCaptureDevice);

    // The device (if it is an async implementation) may or may not get as far
    // as the OnFrameInfo() step; we're intentionally not going to wait for it
    // to get that far.
    EXPECT_CALL(*client, OnFrameInfo(_)).Times(testing::AtMost(1));
    device->AllocateAndStart(requested_format,
                             client.PassAs<Client>());
    device->StopAndDeAllocate();
  }

  // Finally, do a device start and wait for it to finish.
  gfx::Size resolution;
  VideoCaptureCapability requested_format(
      320,
      240,
      30,
      PIXEL_FORMAT_I420,
      ConstantResolutionVideoCaptureDevice);

  base::RunLoop run_loop;
  scoped_ptr<MockClient> client(
      new MockClient(base::Bind(run_loop.QuitClosure())));
  scoped_ptr<VideoCaptureDevice> device(
      VideoCaptureDevice::Create(names_.front()));

    // The device (if it is an async implementation) may or may not get as far
    // as the OnFrameInfo() step; we're intentionally not going to wait for it
    // to get that far.
  VideoCaptureCapability final_format;
  EXPECT_CALL(*client, OnFrameInfo(_))
      .Times(1).WillOnce(SaveArg<0>(&final_format));
  device->AllocateAndStart(requested_format,
                           client.PassAs<Client>());
  run_loop.Run();  // Waits for a frame.
  device->StopAndDeAllocate();
  device.reset();
  EXPECT_EQ(final_format.width, 320);
  EXPECT_EQ(final_format.height, 240);
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

  EXPECT_CALL(*client_, OnErr())
      .Times(0);
  // Get info about the new resolution.
  VideoCaptureCapability rx_capability;
  EXPECT_CALL(*client_, OnFrameInfo(_))
      .WillOnce(SaveArg<0>(&rx_capability));

  VideoCaptureCapability capture_format(640,
                                        480,
                                        30,
                                        PIXEL_FORMAT_I420,
                                        ConstantResolutionVideoCaptureDevice);
  device->AllocateAndStart(capture_format, client_.PassAs<Client>());
  // Get captured video frames.
  WaitForCapturedFrame();
  EXPECT_EQ(rx_capability.width, 640);
  EXPECT_EQ(rx_capability.height, 480);
  EXPECT_EQ(rx_capability.frame_rate, 30);
  device->StopAndDeAllocate();
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
  EXPECT_CALL(*client_, OnFrameInfo(_))
      .Times(1).WillOnce(SaveArg<0>(&rx_capability));

  EXPECT_CALL(*client_, OnErr())
      .Times(0);

  VideoCaptureCapability capture_format(640,
                                        480,
                                        30,
                                        PIXEL_FORMAT_I420,
                                        ConstantResolutionVideoCaptureDevice);
  device->AllocateAndStart(capture_format,
                           client_.PassAs<Client>());
  WaitForCapturedFrame();
  EXPECT_EQ(rx_capability.width, 640);
  EXPECT_EQ(rx_capability.height, 480);
  EXPECT_EQ(rx_capability.frame_rate, 30);
  device->StopAndDeAllocate();
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

  EXPECT_CALL(*client_, OnErr())
      .Times(0);
  // Verify we get MJPEG from the device. Not all devices can capture 1280x720
  // @ 30 fps, so we don't care about the exact resolution we get.
  VideoCaptureCapability rx_capability;
  EXPECT_CALL(*client_, OnFrameInfo(_))
      .WillOnce(SaveArg<0>(&rx_capability));

  VideoCaptureCapability capture_format(1280,
                                        720,
                                        30,
                                        PIXEL_FORMAT_MJPEG,
                                        ConstantResolutionVideoCaptureDevice);
  device->AllocateAndStart(capture_format, client_.PassAs<Client>());
  // Get captured video frames.
  WaitForCapturedFrame();
  EXPECT_EQ(rx_capability.color, PIXEL_FORMAT_MJPEG);
  device->StopAndDeAllocate();
}

TEST_F(VideoCaptureDeviceTest, GetDeviceSupportedFormats) {
  VideoCaptureDevice::GetDeviceNames(&names_);
  if (!names_.size()) {
    DVLOG(1) << "No camera available. Exiting test.";
    return;
  }
  VideoCaptureCapabilities capture_formats;
  VideoCaptureDevice::Names::iterator names_iterator;
  for (names_iterator = names_.begin(); names_iterator != names_.end();
       ++names_iterator) {
    VideoCaptureDevice::GetDeviceSupportedFormats(*names_iterator,
                                                  &capture_formats);
    // Nothing to test here since we cannot forecast the hardware capabilities.
  }
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
  EXPECT_CALL(*client_, OnFrameInfo(_))
      .Times(1);

  EXPECT_CALL(*client_, OnErr())
      .Times(0);
  int action_count = 200;
  EXPECT_CALL(*client_, OnFrameInfoChanged(_))
      .Times(AtLeast(action_count / 30));

  device->AllocateAndStart(capture_format, client_.PassAs<Client>());

  // The amount of times the OnFrameInfoChanged gets called depends on how often
  // FakeDevice is supposed to change and what is its actual frame rate.
  // We set TimeWait to 200 action timeouts and this should be enough for at
  // least action_count/kFakeCaptureCapabilityChangePeriod calls.
  for (int i = 0; i < action_count; ++i) {
    WaitForCapturedFrame();
  }
  device->StopAndDeAllocate();
}

TEST_F(VideoCaptureDeviceTest, FakeGetDeviceSupportedFormats) {
  VideoCaptureDevice::Names names;
  FakeVideoCaptureDevice::GetDeviceNames(&names);

  VideoCaptureCapabilities capture_formats;
  VideoCaptureDevice::Names::iterator names_iterator;

  for (names_iterator = names.begin(); names_iterator != names.end();
       ++names_iterator) {
    FakeVideoCaptureDevice::GetDeviceSupportedFormats(*names_iterator,
                                                      &capture_formats);
    EXPECT_GE(capture_formats.size(), 1u);
    EXPECT_EQ(capture_formats[0].width, 640);
    EXPECT_EQ(capture_formats[0].height, 480);
    EXPECT_EQ(capture_formats[0].color, media::PIXEL_FORMAT_I420);
    EXPECT_GE(capture_formats[0].frame_rate, 20);
  }
}

};  // namespace media
