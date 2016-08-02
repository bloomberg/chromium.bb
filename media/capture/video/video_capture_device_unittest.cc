// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/video_capture_device.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/test/test_timeouts.h"
#include "base/threading/thread.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/video_capture_types.h"
#include "media/capture/video/video_capture_device_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_WIN)
#include "base/win/scoped_com_initializer.h"
#include "base/win/windows_version.h"  // For fine-grained suppression.
#include "media/capture/video/win/video_capture_device_factory_win.h"
#endif

#if defined(OS_MACOSX)
#include "media/base/mac/avfoundation_glue.h"
#include "media/capture/video/mac/video_capture_device_factory_mac.h"
#endif

#if defined(OS_ANDROID)
#include "base/android/jni_android.h"
#include "media/capture/video/android/video_capture_device_android.h"
#endif

#if defined(OS_MACOSX)
// Mac will always give you the size you ask for and this case will fail.
#define MAYBE_AllocateBadSize DISABLED_AllocateBadSize
// We will always get YUYV from the Mac AVFoundation implementations.
#define MAYBE_CaptureMjpeg DISABLED_CaptureMjpeg
#define MAYBE_TakePhoto TakePhoto
#elif defined(OS_WIN)
#define MAYBE_AllocateBadSize AllocateBadSize
#define MAYBE_CaptureMjpeg CaptureMjpeg
#define MAYBE_TakePhoto DISABLED_TakePhoto
#elif defined(OS_ANDROID)
// TODO(wjia): enable those tests on Android.
// On Android, native camera (JAVA) delivers frames on UI thread which is the
// main thread for tests. This results in no frame received by
// VideoCaptureAndroid.
#define MAYBE_AllocateBadSize DISABLED_AllocateBadSize
#define DeAllocateCameraWhileRunning DISABLED_DeAllocateCameraWhileRunning
#define DeAllocateCameraWhileRunning DISABLED_DeAllocateCameraWhileRunning
#define MAYBE_CaptureMjpeg DISABLED_CaptureMjpeg
#define MAYBE_TakePhoto DISABLED_TakePhoto
#elif defined(OS_LINUX)
// AllocateBadSize will hang when a real camera is attached and if more than one
// test is trying to use the camera (even across processes). Do NOT renable
// this test without fixing the many bugs associated with it:
// http://crbug.com/94134 http://crbug.com/137260 http://crbug.com/417824
#define MAYBE_AllocateBadSize DISABLED_AllocateBadSize
#define MAYBE_CaptureMjpeg CaptureMjpeg
#define MAYBE_TakePhoto DISABLED_TakePhoto
#else
#define MAYBE_AllocateBadSize AllocateBadSize
#define MAYBE_CaptureMjpeg CaptureMjpeg
#define MAYBE_TakePhoto DISABLED_TakePhoto
#endif

using ::testing::_;
using ::testing::SaveArg;

namespace media {
namespace {

class MockVideoCaptureClient : public VideoCaptureDevice::Client {
 public:
  MOCK_METHOD0(DoReserveOutputBuffer, void(void));
  MOCK_METHOD0(DoOnIncomingCapturedBuffer, void(void));
  MOCK_METHOD0(DoOnIncomingCapturedVideoFrame, void(void));
  MOCK_METHOD0(DoResurrectLastOutputBuffer, void(void));
  MOCK_METHOD2(OnError,
               void(const tracked_objects::Location& from_here,
                    const std::string& reason));
  MOCK_CONST_METHOD0(GetBufferPoolUtilization, double(void));

  explicit MockVideoCaptureClient(
      base::Callback<void(const VideoCaptureFormat&)> frame_cb)
      : main_thread_(base::ThreadTaskRunnerHandle::Get()),
        frame_cb_(frame_cb) {}

  void OnIncomingCapturedData(const uint8_t* data,
                              int length,
                              const VideoCaptureFormat& format,
                              int rotation,
                              base::TimeTicks reference_time,
                              base::TimeDelta timestamp) override {
    ASSERT_GT(length, 0);
    ASSERT_TRUE(data != NULL);
    main_thread_->PostTask(FROM_HERE, base::Bind(frame_cb_, format));
  }

  // Trampoline methods to workaround GMOCK problems with std::unique_ptr<>.
  std::unique_ptr<Buffer> ReserveOutputBuffer(
      const gfx::Size& dimensions,
      media::VideoPixelFormat format,
      media::VideoPixelStorage storage) override {
    DoReserveOutputBuffer();
    NOTREACHED() << "This should never be called";
    return std::unique_ptr<Buffer>();
  }
  void OnIncomingCapturedBuffer(std::unique_ptr<Buffer> buffer,
                                const VideoCaptureFormat& frame_format,
                                base::TimeTicks reference_time,
                                base::TimeDelta timestamp) override {
    DoOnIncomingCapturedBuffer();
  }
  void OnIncomingCapturedVideoFrame(
      std::unique_ptr<Buffer> buffer,
      const scoped_refptr<VideoFrame>& frame) override {
    DoOnIncomingCapturedVideoFrame();
  }
  std::unique_ptr<Buffer> ResurrectLastOutputBuffer(
      const gfx::Size& dimensions,
      media::VideoPixelFormat format,
      media::VideoPixelStorage storage) {
    DoResurrectLastOutputBuffer();
    NOTREACHED() << "This should never be called";
    return std::unique_ptr<Buffer>();
  }

 private:
  scoped_refptr<base::SingleThreadTaskRunner> main_thread_;
  base::Callback<void(const VideoCaptureFormat&)> frame_cb_;
};

class MockImageCaptureClient : public base::RefCounted<MockImageCaptureClient> {
 public:
  // GMock doesn't support move-only arguments, so we use this forward method.
  void DoOnPhotoTaken(mojom::BlobPtr blob) {
    EXPECT_STREQ("image/jpeg", blob->mime_type.c_str());
    ASSERT_GT(blob->data.size(), 4u);
    // Check some bytes that univocally identify |data| as a JPEG File.
    // https://en.wikipedia.org/wiki/JPEG_File_Interchange_Format#File_format_structure
    EXPECT_EQ(0xFF, blob->data[0]);  // First SOI byte
    EXPECT_EQ(0xD8, blob->data[1]);  // Second SOI byte
    EXPECT_EQ(0xFF, blob->data[2]);  // First JFIF-APP0 byte
    EXPECT_EQ(0xE0, blob->data[3]);  // Second JFIF-APP0 byte
    OnCorrectPhotoTaken();
  }
  MOCK_METHOD0(OnCorrectPhotoTaken, void(void));
  MOCK_METHOD1(OnTakePhotoFailure,
               void(const base::Callback<void(mojom::BlobPtr)>&));

 private:
  friend class base::RefCounted<MockImageCaptureClient>;
  virtual ~MockImageCaptureClient() {}
};

class DeviceEnumerationListener
    : public base::RefCounted<DeviceEnumerationListener> {
 public:
  MOCK_METHOD1(OnEnumerateDeviceDescriptorsCallbackPtr,
               void(VideoCaptureDeviceDescriptors* device_descriptors));
  // GMock doesn't support move-only arguments, so we use this forward method.
  void OnEnumerateDeviceDescriptorsCallback(
      std::unique_ptr<VideoCaptureDeviceDescriptors> device_descriptors) {
    OnEnumerateDeviceDescriptorsCallbackPtr(device_descriptors.release());
  }

 private:
  friend class base::RefCounted<DeviceEnumerationListener>;
  virtual ~DeviceEnumerationListener() {}
};

}  // namespace

class VideoCaptureDeviceTest : public testing::TestWithParam<gfx::Size> {
 protected:
  typedef VideoCaptureDevice::Client Client;

  VideoCaptureDeviceTest()
      : loop_(new base::MessageLoop()),
        video_capture_client_(new MockVideoCaptureClient(
            base::Bind(&VideoCaptureDeviceTest::OnFrameCaptured,
                       base::Unretained(this)))),
        device_enumeration_listener_(new DeviceEnumerationListener()),
        image_capture_client_(new MockImageCaptureClient()),
        video_capture_device_factory_(VideoCaptureDeviceFactory::CreateFactory(
            base::ThreadTaskRunnerHandle::Get())) {}

  void SetUp() override {
#if defined(OS_ANDROID)
    VideoCaptureDeviceAndroid::RegisterVideoCaptureDevice(
        base::android::AttachCurrentThread());
#endif
#if defined(OS_MACOSX)
    AVFoundationGlue::InitializeAVFoundation();
#endif
    EXPECT_CALL(*video_capture_client_, DoReserveOutputBuffer()).Times(0);
    EXPECT_CALL(*video_capture_client_, DoOnIncomingCapturedBuffer()).Times(0);
    EXPECT_CALL(*video_capture_client_, DoOnIncomingCapturedVideoFrame())
        .Times(0);
  }

  void ResetWithNewClient() {
    video_capture_client_.reset(new MockVideoCaptureClient(base::Bind(
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

  std::unique_ptr<VideoCaptureDeviceDescriptors> EnumerateDeviceDescriptors() {
    VideoCaptureDeviceDescriptors* device_descriptors;
    EXPECT_CALL(*device_enumeration_listener_.get(),
                OnEnumerateDeviceDescriptorsCallbackPtr(_))
        .WillOnce(SaveArg<0>(&device_descriptors));

    video_capture_device_factory_->EnumerateDeviceDescriptors(base::Bind(
        &DeviceEnumerationListener::OnEnumerateDeviceDescriptorsCallback,
        device_enumeration_listener_));
    base::RunLoop().RunUntilIdle();
    return std::unique_ptr<VideoCaptureDeviceDescriptors>(device_descriptors);
  }

  const VideoCaptureFormat& last_format() const { return last_format_; }

  std::unique_ptr<VideoCaptureDeviceDescriptor>
  GetFirstDeviceDescriptorSupportingPixelFormat(
      const VideoPixelFormat& pixel_format) {
    device_descriptors_ = EnumerateDeviceDescriptors();
    if (device_descriptors_->empty()) {
      DVLOG(1) << "No camera available.";
      return std::unique_ptr<VideoCaptureDeviceDescriptor>();
    }
    for (const auto& descriptors_iterator : *device_descriptors_) {
      VideoCaptureFormats supported_formats;
      video_capture_device_factory_->GetSupportedFormats(descriptors_iterator,
                                                         &supported_formats);
      for (const auto& formats_iterator : supported_formats) {
        if (formats_iterator.pixel_format == pixel_format) {
          return std::unique_ptr<VideoCaptureDeviceDescriptor>(
              new VideoCaptureDeviceDescriptor(descriptors_iterator));
        }
      }
    }
    DVLOG_IF(1, pixel_format != PIXEL_FORMAT_MAX)
        << "No camera can capture the"
        << " format: " << VideoPixelFormatToString(pixel_format);
    return std::unique_ptr<VideoCaptureDeviceDescriptor>();
  }

  bool IsCaptureSizeSupported(const VideoCaptureDeviceDescriptor& device,
                              const gfx::Size& size) {
    VideoCaptureFormats supported_formats;
    video_capture_device_factory_->GetSupportedFormats(device,
                                                       &supported_formats);
    const auto it = std::find_if(
        supported_formats.begin(), supported_formats.end(),
        [&size](VideoCaptureFormat const& f) { return f.frame_size == size; });
    if (it == supported_formats.end()) {
      DVLOG(1) << "Size " << size.ToString() << " is not supported.";
      return false;
    }
    return true;
  }

#if defined(OS_WIN)
  base::win::ScopedCOMInitializer initialize_com_;
#endif
  std::unique_ptr<VideoCaptureDeviceDescriptors> device_descriptors_;
  const std::unique_ptr<base::MessageLoop> loop_;
  std::unique_ptr<base::RunLoop> run_loop_;
  std::unique_ptr<MockVideoCaptureClient> video_capture_client_;
  const scoped_refptr<DeviceEnumerationListener> device_enumeration_listener_;
  const scoped_refptr<MockImageCaptureClient> image_capture_client_;
  VideoCaptureFormat last_format_;
  const std::unique_ptr<VideoCaptureDeviceFactory>
      video_capture_device_factory_;
};

// Cause hangs on Windows Debug. http://crbug.com/417824
#if defined(OS_WIN) && !defined(NDEBUG)
#define MAYBE_OpenInvalidDevice DISABLED_OpenInvalidDevice
#else
#define MAYBE_OpenInvalidDevice OpenInvalidDevice
#endif

TEST_F(VideoCaptureDeviceTest, MAYBE_OpenInvalidDevice) {
  VideoCaptureDeviceDescriptor invalid_descriptor;
  invalid_descriptor.device_id = "jibberish";
  invalid_descriptor.display_name = "jibberish";
#if defined(OS_WIN)
  invalid_descriptor.capture_api =
      VideoCaptureDeviceFactoryWin::PlatformSupportsMediaFoundation()
          ? VideoCaptureApi::WIN_MEDIA_FOUNDATION
          : VideoCaptureApi::WIN_DIRECT_SHOW;
#elif defined(OS_MACOSX)
  invalid_descriptor.capture_api = VideoCaptureApi::MACOSX_AVFOUNDATION;
#endif
  std::unique_ptr<VideoCaptureDevice> device =
      video_capture_device_factory_->CreateDevice(invalid_descriptor);

#if !defined(OS_MACOSX)
  EXPECT_TRUE(device == NULL);
#else
  // The presence of the actual device is only checked on AllocateAndStart()
  // and not on creation.
  EXPECT_CALL(*video_capture_client_, OnError(_, _)).Times(1);

  VideoCaptureParams capture_params;
  capture_params.requested_format.frame_size.SetSize(640, 480);
  capture_params.requested_format.frame_rate = 30;
  capture_params.requested_format.pixel_format = PIXEL_FORMAT_I420;
  device->AllocateAndStart(capture_params, std::move(video_capture_client_));
  device->StopAndDeAllocate();
#endif
}

TEST_P(VideoCaptureDeviceTest, CaptureWithSize) {
  device_descriptors_ = EnumerateDeviceDescriptors();
  if (device_descriptors_->empty()) {
    VLOG(1) << "No camera available. Exiting test.";
    return;
  }

  const gfx::Size& size = GetParam();
  if (!IsCaptureSizeSupported(device_descriptors_->front(), size))
    return;
  const int width = size.width();
  const int height = size.height();

  std::unique_ptr<VideoCaptureDevice> device(
      video_capture_device_factory_->CreateDevice(
          device_descriptors_->front()));
  ASSERT_TRUE(device);
  DVLOG(1) << device_descriptors_->front().device_id;

  EXPECT_CALL(*video_capture_client_, OnError(_, _)).Times(0);

  VideoCaptureParams capture_params;
  capture_params.requested_format.frame_size.SetSize(width, height);
  capture_params.requested_format.frame_rate = 30.0f;
  capture_params.requested_format.pixel_format = PIXEL_FORMAT_I420;
  device->AllocateAndStart(capture_params, std::move(video_capture_client_));
  // Get captured video frames.
  WaitForCapturedFrame();
  EXPECT_EQ(last_format().frame_size.width(), width);
  EXPECT_EQ(last_format().frame_size.height(), height);
  if (last_format().pixel_format != PIXEL_FORMAT_MJPEG)
    EXPECT_EQ(size.GetArea(), last_format().frame_size.GetArea());
  device->StopAndDeAllocate();
}

#if !defined(OS_ANDROID)
const gfx::Size kCaptureSizes[] = {gfx::Size(640, 480), gfx::Size(1280, 720)};

INSTANTIATE_TEST_CASE_P(VideoCaptureDeviceTests,
                        VideoCaptureDeviceTest,
                        testing::ValuesIn(kCaptureSizes));
#endif

TEST_F(VideoCaptureDeviceTest, MAYBE_AllocateBadSize) {
  device_descriptors_ = EnumerateDeviceDescriptors();
  if (device_descriptors_->empty()) {
    VLOG(1) << "No camera available. Exiting test.";
    return;
  }
  std::unique_ptr<VideoCaptureDevice> device(
      video_capture_device_factory_->CreateDevice(
          device_descriptors_->front()));
  ASSERT_TRUE(device);

  EXPECT_CALL(*video_capture_client_, OnError(_, _)).Times(0);

  const gfx::Size input_size(640, 480);
  VideoCaptureParams capture_params;
  capture_params.requested_format.frame_size.SetSize(637, 472);
  capture_params.requested_format.frame_rate = 35;
  capture_params.requested_format.pixel_format = PIXEL_FORMAT_I420;
  device->AllocateAndStart(capture_params, std::move(video_capture_client_));
  WaitForCapturedFrame();
  device->StopAndDeAllocate();
  EXPECT_EQ(last_format().frame_size.width(), input_size.width());
  EXPECT_EQ(last_format().frame_size.height(), input_size.height());
  if (last_format().pixel_format != PIXEL_FORMAT_MJPEG)
    EXPECT_EQ(input_size.GetArea(), last_format().frame_size.GetArea());
}

// Cause hangs on Windows, Linux. Fails Android. http://crbug.com/417824
TEST_F(VideoCaptureDeviceTest, DISABLED_ReAllocateCamera) {
  device_descriptors_ = EnumerateDeviceDescriptors();
  if (device_descriptors_->empty()) {
    VLOG(1) << "No camera available. Exiting test.";
    return;
  }

  // First, do a number of very fast device start/stops.
  for (int i = 0; i <= 5; i++) {
    ResetWithNewClient();
    std::unique_ptr<VideoCaptureDevice> device(
        video_capture_device_factory_->CreateDevice(
            device_descriptors_->front()));
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
    device->AllocateAndStart(capture_params, std::move(video_capture_client_));
    device->StopAndDeAllocate();
  }

  // Finally, do a device start and wait for it to finish.
  VideoCaptureParams capture_params;
  capture_params.requested_format.frame_size.SetSize(320, 240);
  capture_params.requested_format.frame_rate = 30;
  capture_params.requested_format.pixel_format = PIXEL_FORMAT_I420;

  ResetWithNewClient();
  std::unique_ptr<VideoCaptureDevice> device(
      video_capture_device_factory_->CreateDevice(
          device_descriptors_->front()));

  device->AllocateAndStart(capture_params, std::move(video_capture_client_));
  WaitForCapturedFrame();
  device->StopAndDeAllocate();
  device.reset();
  EXPECT_EQ(last_format().frame_size.width(), 320);
  EXPECT_EQ(last_format().frame_size.height(), 240);
}

TEST_F(VideoCaptureDeviceTest, DeAllocateCameraWhileRunning) {
  device_descriptors_ = EnumerateDeviceDescriptors();
  if (device_descriptors_->empty()) {
    VLOG(1) << "No camera available. Exiting test.";
    return;
  }
  std::unique_ptr<VideoCaptureDevice> device(
      video_capture_device_factory_->CreateDevice(
          device_descriptors_->front()));
  ASSERT_TRUE(device);

  EXPECT_CALL(*video_capture_client_, OnError(_, _)).Times(0);

  VideoCaptureParams capture_params;
  capture_params.requested_format.frame_size.SetSize(640, 480);
  capture_params.requested_format.frame_rate = 30;
  capture_params.requested_format.pixel_format = PIXEL_FORMAT_I420;
  device->AllocateAndStart(capture_params, std::move(video_capture_client_));
  // Get captured video frames.
  WaitForCapturedFrame();
  EXPECT_EQ(last_format().frame_size.width(), 640);
  EXPECT_EQ(last_format().frame_size.height(), 480);
  EXPECT_EQ(last_format().frame_rate, 30);
  device->StopAndDeAllocate();
}

// Start the camera in 720p to capture MJPEG instead of a raw format.
TEST_F(VideoCaptureDeviceTest, MAYBE_CaptureMjpeg) {
  std::unique_ptr<VideoCaptureDeviceDescriptor> device_descriptor =
      GetFirstDeviceDescriptorSupportingPixelFormat(PIXEL_FORMAT_MJPEG);
  if (!device_descriptor) {
    VLOG(1) << "No camera supports MJPEG format. Exiting test.";
    return;
  }
#if defined(OS_WIN)
  base::win::Version version = base::win::GetVersion();
  VLOG(1) << "Windows version: " << (int)version;
  if (version >= base::win::VERSION_WIN10) {
    VLOG(1) << "Skipped on Win10: http://crbug.com/570604.";
    return;
  }
#endif
  std::unique_ptr<VideoCaptureDevice> device(
      video_capture_device_factory_->CreateDevice(*device_descriptor));
  ASSERT_TRUE(device);

  EXPECT_CALL(*video_capture_client_, OnError(_, _)).Times(0);

  VideoCaptureParams capture_params;
  capture_params.requested_format.frame_size.SetSize(1280, 720);
  capture_params.requested_format.frame_rate = 30;
  capture_params.requested_format.pixel_format = PIXEL_FORMAT_MJPEG;
  device->AllocateAndStart(capture_params, std::move(video_capture_client_));
  // Get captured video frames.
  WaitForCapturedFrame();
  // Verify we get MJPEG from the device. Not all devices can capture 1280x720
  // @ 30 fps, so we don't care about the exact resolution we get.
  EXPECT_EQ(last_format().pixel_format, PIXEL_FORMAT_MJPEG);
  EXPECT_GE(static_cast<size_t>(1280 * 720),
            last_format().ImageAllocationSize());
  device->StopAndDeAllocate();
}

TEST_F(VideoCaptureDeviceTest, GetDeviceSupportedFormats) {
  // Use PIXEL_FORMAT_MAX to iterate all device names for testing
  // GetDeviceSupportedFormats().
  std::unique_ptr<VideoCaptureDeviceDescriptor> device_descriptor =
      GetFirstDeviceDescriptorSupportingPixelFormat(PIXEL_FORMAT_MAX);
  // Verify no camera returned for PIXEL_FORMAT_MAX. Nothing else to test here
  // since we cannot forecast the hardware capabilities.
  ASSERT_FALSE(device_descriptor);
}

// Start the camera and take a photo.
TEST_F(VideoCaptureDeviceTest, MAYBE_TakePhoto) {
  device_descriptors_ = EnumerateDeviceDescriptors();
  if (device_descriptors_->empty()) {
    VLOG(1) << "No camera available. Exiting test.";
    return;
  }
  std::unique_ptr<VideoCaptureDevice> device(
      video_capture_device_factory_->CreateDevice(
          device_descriptors_->front()));
  ASSERT_TRUE(device);

  EXPECT_CALL(*video_capture_client_, OnError(_, _)).Times(0);

  VideoCaptureParams capture_params;
  capture_params.requested_format.frame_size.SetSize(640, 480);
  capture_params.requested_format.frame_rate = 30;
  capture_params.requested_format.pixel_format = PIXEL_FORMAT_I420;
  device->AllocateAndStart(capture_params, std::move(video_capture_client_));
  WaitForCapturedFrame();

  VideoCaptureDevice::TakePhotoCallback scoped_callback(
      base::Bind(&MockImageCaptureClient::DoOnPhotoTaken,
                 image_capture_client_),
      media::BindToCurrentLoop(base::Bind(
          &MockImageCaptureClient::OnTakePhotoFailure, image_capture_client_)));

  EXPECT_CALL(*image_capture_client_.get(), OnCorrectPhotoTaken()).Times(1);
  device->TakePhoto(std::move(scoped_callback));
  WaitForCapturedFrame();

  device->StopAndDeAllocate();
}

};  // namespace media
