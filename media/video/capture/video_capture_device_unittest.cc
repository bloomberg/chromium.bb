// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/memory/scoped_ptr.h"
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

namespace media {

class MockClient : public media::VideoCaptureDevice::Client {
 public:
  MOCK_METHOD2(ReserveOutputBuffer,
               scoped_refptr<Buffer>(media::VideoFrame::Format format,
                                     const gfx::Size& dimensions));
  MOCK_METHOD0(OnErr, void());

  explicit MockClient(base::Callback<void(const VideoCaptureFormat&)> frame_cb)
      : main_thread_(base::MessageLoopProxy::current()), frame_cb_(frame_cb) {}

  virtual void OnError(const std::string& error_message) OVERRIDE {
    OnErr();
  }

  virtual void OnIncomingCapturedData(const uint8* data,
                                      int length,
                                      const VideoCaptureFormat& format,
                                      int rotation,
                                      base::TimeTicks timestamp) OVERRIDE {
    main_thread_->PostTask(FROM_HERE, base::Bind(frame_cb_, format));
  }

  virtual void OnIncomingCapturedVideoFrame(
      const scoped_refptr<Buffer>& buffer,
      const media::VideoCaptureFormat& buffer_format,
      const scoped_refptr<media::VideoFrame>& frame,
      base::TimeTicks timestamp) OVERRIDE {
    NOTREACHED();
  }

 private:
  scoped_refptr<base::SingleThreadTaskRunner> main_thread_;
  base::Callback<void(const VideoCaptureFormat&)> frame_cb_;
};

class VideoCaptureDeviceTest : public testing::Test {
 protected:
  typedef media::VideoCaptureDevice::Client Client;

  VideoCaptureDeviceTest()
      : loop_(new base::MessageLoop()),
        client_(
            new MockClient(base::Bind(&VideoCaptureDeviceTest::OnFrameCaptured,
                                      base::Unretained(this)))) {}

  virtual void SetUp() {
#if defined(OS_ANDROID)
    media::VideoCaptureDeviceAndroid::RegisterVideoCaptureDevice(
        base::android::AttachCurrentThread());
#endif
  }

  void ResetWithNewClient() {
    client_.reset(new MockClient(base::Bind(
        &VideoCaptureDeviceTest::OnFrameCaptured, base::Unretained(this))));
  }

  void OnFrameCaptured(const VideoCaptureFormat& format) {
    last_format_ = format;
    run_loop_->QuitClosure().Run();
  }

  void WaitForCapturedFrame() {
    run_loop_.reset(new base::RunLoop());
    run_loop_->Run();
  }

  const VideoCaptureFormat& last_format() const { return last_format_; }

  scoped_ptr<VideoCaptureDevice::Name> GetFirstDeviceNameSupportingPixelFormat(
      const VideoPixelFormat& pixel_format) {
    VideoCaptureDevice::GetDeviceNames(&names_);
    if (!names_.size()) {
      DVLOG(1) << "No camera available.";
      return scoped_ptr<VideoCaptureDevice::Name>();
    }
    VideoCaptureDevice::Names::iterator names_iterator;
    for (names_iterator = names_.begin(); names_iterator != names_.end();
         ++names_iterator) {
      VideoCaptureFormats supported_formats;
      VideoCaptureDevice::GetDeviceSupportedFormats(*names_iterator,
                                                    &supported_formats);
      VideoCaptureFormats::iterator formats_iterator;
      for (formats_iterator = supported_formats.begin();
           formats_iterator != supported_formats.end(); ++formats_iterator) {
        if (formats_iterator->pixel_format == pixel_format) {
          return scoped_ptr<VideoCaptureDevice::Name>(
              new VideoCaptureDevice::Name(*names_iterator));
        }
      }
    }
    DVLOG(1) << "No camera can capture the format: " << pixel_format;
    return scoped_ptr<VideoCaptureDevice::Name>();
  }

#if defined(OS_WIN)
  base::win::ScopedCOMInitializer initialize_com_;
#endif
  VideoCaptureDevice::Names names_;
  scoped_ptr<base::MessageLoop> loop_;
  scoped_ptr<base::RunLoop> run_loop_;
  scoped_ptr<MockClient> client_;
  VideoCaptureFormat last_format_;
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
  ASSERT_TRUE(device);
  DVLOG(1) << names_.front().id();

  EXPECT_CALL(*client_, OnErr())
      .Times(0);

  VideoCaptureParams capture_params;
  capture_params.requested_format.frame_size.SetSize(640, 480);
  capture_params.requested_format.frame_rate = 30;
  capture_params.requested_format.pixel_format = PIXEL_FORMAT_I420;
  capture_params.allow_resolution_change = false;
  device->AllocateAndStart(capture_params, client_.PassAs<Client>());
  // Get captured video frames.
  WaitForCapturedFrame();
  EXPECT_EQ(last_format().frame_size.width(), 640);
  EXPECT_EQ(last_format().frame_size.height(), 480);
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
  ASSERT_TRUE(device);

  EXPECT_CALL(*client_, OnErr())
      .Times(0);

  VideoCaptureParams capture_params;
  capture_params.requested_format.frame_size.SetSize(1280, 720);
  capture_params.requested_format.frame_rate = 30;
  capture_params.requested_format.pixel_format = PIXEL_FORMAT_I420;
  capture_params.allow_resolution_change = false;
  device->AllocateAndStart(capture_params, client_.PassAs<Client>());
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
  ASSERT_TRUE(device);

  EXPECT_CALL(*client_, OnErr())
      .Times(0);

  VideoCaptureParams capture_params;
  capture_params.requested_format.frame_size.SetSize(637, 472);
  capture_params.requested_format.frame_rate = 35;
  capture_params.requested_format.pixel_format = PIXEL_FORMAT_I420;
  capture_params.allow_resolution_change = false;
  device->AllocateAndStart(capture_params, client_.PassAs<Client>());
  WaitForCapturedFrame();
  device->StopAndDeAllocate();
  EXPECT_EQ(last_format().frame_size.width(), 640);
  EXPECT_EQ(last_format().frame_size.height(), 480);
}

TEST_F(VideoCaptureDeviceTest, ReAllocateCamera) {
  VideoCaptureDevice::GetDeviceNames(&names_);
  if (!names_.size()) {
    DVLOG(1) << "No camera available. Exiting test.";
    return;
  }

  // First, do a number of very fast device start/stops.
  for (int i = 0; i <= 5; i++) {
    ResetWithNewClient();
    scoped_ptr<VideoCaptureDevice> device(
        VideoCaptureDevice::Create(names_.front()));
    gfx::Size resolution;
    if (i % 2) {
      resolution = gfx::Size(640, 480);
    } else {
      resolution = gfx::Size(1280, 1024);
    }
    VideoCaptureParams capture_params;
    capture_params.requested_format.frame_size = resolution;
    capture_params.requested_format.frame_rate = 30;
    capture_params.requested_format.pixel_format = PIXEL_FORMAT_I420;
    capture_params.allow_resolution_change = false;
    device->AllocateAndStart(capture_params, client_.PassAs<Client>());
    device->StopAndDeAllocate();
  }

  // Finally, do a device start and wait for it to finish.
  VideoCaptureParams capture_params;
  capture_params.requested_format.frame_size.SetSize(320, 240);
  capture_params.requested_format.frame_rate = 30;
  capture_params.requested_format.pixel_format = PIXEL_FORMAT_I420;
  capture_params.allow_resolution_change = false;

  ResetWithNewClient();
  scoped_ptr<VideoCaptureDevice> device(
      VideoCaptureDevice::Create(names_.front()));

  device->AllocateAndStart(capture_params, client_.PassAs<Client>());
  WaitForCapturedFrame();
  device->StopAndDeAllocate();
  device.reset();
  EXPECT_EQ(last_format().frame_size.width(), 320);
  EXPECT_EQ(last_format().frame_size.height(), 240);
}

TEST_F(VideoCaptureDeviceTest, DeAllocateCameraWhileRunning) {
  VideoCaptureDevice::GetDeviceNames(&names_);
  if (!names_.size()) {
    DVLOG(1) << "No camera available. Exiting test.";
    return;
  }
  scoped_ptr<VideoCaptureDevice> device(
      VideoCaptureDevice::Create(names_.front()));
  ASSERT_TRUE(device);

  EXPECT_CALL(*client_, OnErr())
      .Times(0);

  VideoCaptureParams capture_params;
  capture_params.requested_format.frame_size.SetSize(640, 480);
  capture_params.requested_format.frame_rate = 30;
  capture_params.requested_format.pixel_format = PIXEL_FORMAT_I420;
  capture_params.allow_resolution_change = false;
  device->AllocateAndStart(capture_params, client_.PassAs<Client>());
  // Get captured video frames.
  WaitForCapturedFrame();
  EXPECT_EQ(last_format().frame_size.width(), 640);
  EXPECT_EQ(last_format().frame_size.height(), 480);
  EXPECT_EQ(last_format().frame_rate, 30);
  device->StopAndDeAllocate();
}

TEST_F(VideoCaptureDeviceTest, FakeCapture) {
  VideoCaptureDevice::Names names;

  FakeVideoCaptureDevice::GetDeviceNames(&names);

  ASSERT_GT(static_cast<int>(names.size()), 0);

  scoped_ptr<VideoCaptureDevice> device(
      FakeVideoCaptureDevice::Create(names.front()));
  ASSERT_TRUE(device);

  EXPECT_CALL(*client_, OnErr())
      .Times(0);

  VideoCaptureParams capture_params;
  capture_params.requested_format.frame_size.SetSize(640, 480);
  capture_params.requested_format.frame_rate = 30;
  capture_params.requested_format.pixel_format = PIXEL_FORMAT_I420;
  capture_params.allow_resolution_change = false;
  device->AllocateAndStart(capture_params, client_.PassAs<Client>());
  WaitForCapturedFrame();
  EXPECT_EQ(last_format().frame_size.width(), 640);
  EXPECT_EQ(last_format().frame_size.height(), 480);
  EXPECT_EQ(last_format().frame_rate, 30);
  device->StopAndDeAllocate();
}

// Start the camera in 720p to capture MJPEG instead of a raw format.
TEST_F(VideoCaptureDeviceTest, MAYBE_CaptureMjpeg) {
  scoped_ptr<VideoCaptureDevice::Name> name =
      GetFirstDeviceNameSupportingPixelFormat(PIXEL_FORMAT_MJPEG);
  if (!name) {
    DVLOG(1) << "No camera supports MJPEG format. Exiting test.";
    return;
  }
  scoped_ptr<VideoCaptureDevice> device(VideoCaptureDevice::Create(*name));
  ASSERT_TRUE(device);

  EXPECT_CALL(*client_, OnErr())
      .Times(0);

  VideoCaptureParams capture_params;
  capture_params.requested_format.frame_size.SetSize(1280, 720);
  capture_params.requested_format.frame_rate = 30;
  capture_params.requested_format.pixel_format = PIXEL_FORMAT_MJPEG;
  capture_params.allow_resolution_change = false;
  device->AllocateAndStart(capture_params, client_.PassAs<Client>());
  // Get captured video frames.
  WaitForCapturedFrame();
  // Verify we get MJPEG from the device. Not all devices can capture 1280x720
  // @ 30 fps, so we don't care about the exact resolution we get.
  EXPECT_EQ(last_format().pixel_format, PIXEL_FORMAT_MJPEG);
  device->StopAndDeAllocate();
}

TEST_F(VideoCaptureDeviceTest, GetDeviceSupportedFormats) {
  // Use PIXEL_FORMAT_MAX to iterate all device names for testing
  // GetDeviceSupportedFormats().
  scoped_ptr<VideoCaptureDevice::Name> name =
      GetFirstDeviceNameSupportingPixelFormat(PIXEL_FORMAT_MAX);
  // Verify no camera returned for PIXEL_FORMAT_MAX. Nothing else to test here
  // since we cannot forecast the hardware capabilities.
  ASSERT_FALSE(name);
}

TEST_F(VideoCaptureDeviceTest, FakeCaptureVariableResolution) {
  VideoCaptureDevice::Names names;

  FakeVideoCaptureDevice::GetDeviceNames(&names);
  VideoCaptureParams capture_params;
  capture_params.requested_format.frame_size.SetSize(640, 480);
  capture_params.requested_format.frame_rate = 30;
  capture_params.requested_format.pixel_format = PIXEL_FORMAT_I420;
  capture_params.allow_resolution_change = true;

  ASSERT_GT(static_cast<int>(names.size()), 0);

  scoped_ptr<VideoCaptureDevice> device(
      FakeVideoCaptureDevice::Create(names.front()));
  ASSERT_TRUE(device);

  EXPECT_CALL(*client_, OnErr())
      .Times(0);
  int action_count = 200;

  device->AllocateAndStart(capture_params, client_.PassAs<Client>());

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

  VideoCaptureFormats supported_formats;
  VideoCaptureDevice::Names::iterator names_iterator;

  for (names_iterator = names.begin(); names_iterator != names.end();
       ++names_iterator) {
    FakeVideoCaptureDevice::GetDeviceSupportedFormats(*names_iterator,
                                                      &supported_formats);
    EXPECT_EQ(supported_formats.size(), 3u);
    EXPECT_EQ(supported_formats[0].frame_size.width(), 320);
    EXPECT_EQ(supported_formats[0].frame_size.height(), 240);
    EXPECT_EQ(supported_formats[0].pixel_format, media::PIXEL_FORMAT_I420);
    EXPECT_GE(supported_formats[0].frame_rate, 20);
    EXPECT_EQ(supported_formats[1].frame_size.width(), 640);
    EXPECT_EQ(supported_formats[1].frame_size.height(), 480);
    EXPECT_EQ(supported_formats[1].pixel_format, media::PIXEL_FORMAT_I420);
    EXPECT_GE(supported_formats[1].frame_rate, 20);
    EXPECT_EQ(supported_formats[2].frame_size.width(), 1280);
    EXPECT_EQ(supported_formats[2].frame_size.height(), 720);
    EXPECT_EQ(supported_formats[2].pixel_format, media::PIXEL_FORMAT_I420);
    EXPECT_GE(supported_formats[2].frame_rate, 20);
  }
}

};  // namespace media
