// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/fake_video_capture_device.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/test/test_timeouts.h"
#include "base/threading/thread.h"
#include "build/build_config.h"
#include "media/base/media_switches.h"
#include "media/base/video_capture_types.h"
#include "media/capture/video/fake_video_capture_device_factory.h"
#include "media/capture/video/video_capture_device.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Bool;
using ::testing::Combine;
using ::testing::SaveArg;
using ::testing::Values;

namespace media {

namespace {

// This class is a Client::Buffer that allocates and frees the requested |size|.
class MockBuffer : public VideoCaptureDevice::Client::Buffer {
 public:
  MockBuffer(int buffer_id, size_t mapped_size)
      : id_(buffer_id),
        mapped_size_(mapped_size),
        data_(new uint8_t[mapped_size]) {}
  ~MockBuffer() override { delete[] data_; }

  int id() const override { return id_; }
  gfx::Size dimensions() const override { return gfx::Size(); }
  size_t mapped_size() const override { return mapped_size_; }
  void* data(int plane) override { return data_; }
  ClientBuffer AsClientBuffer(int plane) override { return nullptr; }
#if defined(OS_POSIX) && !(defined(OS_MACOSX) && !defined(OS_IOS))
  base::FileDescriptor AsPlatformFile() override {
    return base::FileDescriptor();
  }
#endif

 private:
  const int id_;
  const size_t mapped_size_;
  uint8_t* const data_;
};

class MockClient : public VideoCaptureDevice::Client {
 public:
  MOCK_METHOD2(OnError,
               void(const tracked_objects::Location& from_here,
                    const std::string& reason));

  explicit MockClient(base::Callback<void(const VideoCaptureFormat&)> frame_cb)
      : frame_cb_(frame_cb) {}

  // Client virtual methods for capturing using Device Buffers.
  void OnIncomingCapturedData(const uint8_t* data,
                              int length,
                              const VideoCaptureFormat& format,
                              int rotation,
                              const base::TimeTicks& timestamp) {
    frame_cb_.Run(format);
  }
  // Virtual methods for capturing using Client's Buffers.
  std::unique_ptr<Buffer> ReserveOutputBuffer(
      const gfx::Size& dimensions,
      media::VideoPixelFormat format,
      media::VideoPixelStorage storage) {
    EXPECT_TRUE((format == media::PIXEL_FORMAT_ARGB &&
                 storage == media::PIXEL_STORAGE_CPU) ||
                (format == media::PIXEL_FORMAT_I420 &&
                 storage == media::PIXEL_STORAGE_GPUMEMORYBUFFER));
    EXPECT_GT(dimensions.GetArea(), 0);
    const VideoCaptureFormat frame_format(dimensions, 0.0, format);
    return base::WrapUnique(
        new MockBuffer(0, frame_format.ImageAllocationSize()));
  }
  void OnIncomingCapturedBuffer(std::unique_ptr<Buffer> buffer,
                                const VideoCaptureFormat& frame_format,
                                const base::TimeTicks& timestamp) {
    frame_cb_.Run(frame_format);
  }
  void OnIncomingCapturedVideoFrame(
      std::unique_ptr<Buffer> buffer,
      const scoped_refptr<media::VideoFrame>& frame,
      const base::TimeTicks& timestamp) {
    VideoCaptureFormat format(frame->natural_size(), 30.0,
                              PIXEL_FORMAT_I420);
    frame_cb_.Run(format);
  }
  std::unique_ptr<Buffer> ResurrectLastOutputBuffer(
      const gfx::Size& dimensions,
      media::VideoPixelFormat format,
      media::VideoPixelStorage storage) {
    return std::unique_ptr<Buffer>();
  }
  double GetBufferPoolUtilization() const override { return 0.0; }

 private:
  base::Callback<void(const VideoCaptureFormat&)> frame_cb_;
};

class DeviceEnumerationListener
    : public base::RefCounted<DeviceEnumerationListener> {
 public:
  MOCK_METHOD1(OnEnumeratedDevicesCallbackPtr,
               void(VideoCaptureDevice::Names* names));
  // GMock doesn't support move-only arguments, so we use this forward method.
  void OnEnumeratedDevicesCallback(
      std::unique_ptr<VideoCaptureDevice::Names> names) {
    OnEnumeratedDevicesCallbackPtr(names.release());
  }

 private:
  friend class base::RefCounted<DeviceEnumerationListener>;
  virtual ~DeviceEnumerationListener() {}
};

}  // namespace

class FakeVideoCaptureDeviceBase : public ::testing::Test {
 protected:
  FakeVideoCaptureDeviceBase()
      : loop_(new base::MessageLoop()),
        client_(new MockClient(
            base::Bind(&FakeVideoCaptureDeviceBase::OnFrameCaptured,
                       base::Unretained(this)))),
        video_capture_device_factory_(new FakeVideoCaptureDeviceFactory()) {
    device_enumeration_listener_ = new DeviceEnumerationListener();
  }

  void SetUp() override { EXPECT_CALL(*client_, OnError(_, _)).Times(0); }

  void OnFrameCaptured(const VideoCaptureFormat& format) {
    last_format_ = format;
    run_loop_->QuitClosure().Run();
  }

  void WaitForCapturedFrame() {
    run_loop_.reset(new base::RunLoop());
    run_loop_->Run();
  }

  std::unique_ptr<VideoCaptureDevice::Names> EnumerateDevices() {
    VideoCaptureDevice::Names* names;
    EXPECT_CALL(*device_enumeration_listener_.get(),
                OnEnumeratedDevicesCallbackPtr(_)).WillOnce(SaveArg<0>(&names));

    video_capture_device_factory_->EnumerateDeviceNames(
        base::Bind(&DeviceEnumerationListener::OnEnumeratedDevicesCallback,
                   device_enumeration_listener_));
    base::MessageLoop::current()->RunUntilIdle();
    return std::unique_ptr<VideoCaptureDevice::Names>(names);
  }

  const VideoCaptureFormat& last_format() const { return last_format_; }

  VideoCaptureDevice::Names names_;
  const std::unique_ptr<base::MessageLoop> loop_;
  std::unique_ptr<base::RunLoop> run_loop_;
  std::unique_ptr<MockClient> client_;
  scoped_refptr<DeviceEnumerationListener> device_enumeration_listener_;
  VideoCaptureFormat last_format_;
  const std::unique_ptr<VideoCaptureDeviceFactory>
      video_capture_device_factory_;
};

class FakeVideoCaptureDeviceTest
    : public FakeVideoCaptureDeviceBase,
      public ::testing::WithParamInterface<
          ::testing::tuple<FakeVideoCaptureDevice::BufferOwnership, float>> {};

struct CommandLineTestData {
  // Command line argument
  std::string argument;
  // Expected values
  float fps;
};

class FakeVideoCaptureDeviceCommandLineTest
    : public FakeVideoCaptureDeviceBase,
      public ::testing::WithParamInterface<CommandLineTestData> {};

TEST_P(FakeVideoCaptureDeviceTest, CaptureUsing) {
  const std::unique_ptr<VideoCaptureDevice::Names> names(EnumerateDevices());
  ASSERT_FALSE(names->empty());

  std::unique_ptr<VideoCaptureDevice> device(new FakeVideoCaptureDevice(
      testing::get<0>(GetParam()), testing::get<1>(GetParam())));
  ASSERT_TRUE(device);

  VideoCaptureParams capture_params;
  capture_params.requested_format.frame_size.SetSize(640, 480);
  capture_params.requested_format.frame_rate = testing::get<1>(GetParam());
  device->AllocateAndStart(capture_params, std::move(client_));

  WaitForCapturedFrame();
  EXPECT_EQ(last_format().frame_size.width(), 640);
  EXPECT_EQ(last_format().frame_size.height(), 480);
  EXPECT_EQ(last_format().frame_rate, testing::get<1>(GetParam()));
  device->StopAndDeAllocate();
}

INSTANTIATE_TEST_CASE_P(
    ,
    FakeVideoCaptureDeviceTest,
    Combine(Values(FakeVideoCaptureDevice::BufferOwnership::OWN_BUFFERS,
                   FakeVideoCaptureDevice::BufferOwnership::CLIENT_BUFFERS),
            Values(20, 29.97, 30, 50, 60)));

TEST_F(FakeVideoCaptureDeviceTest, GetDeviceSupportedFormats) {
  std::unique_ptr<VideoCaptureDevice::Names> names(EnumerateDevices());

  VideoCaptureFormats supported_formats;

  for (const auto& names_iterator : *names) {
    video_capture_device_factory_->GetDeviceSupportedFormats(
        names_iterator, &supported_formats);
    ASSERT_EQ(supported_formats.size(), 4u);
    EXPECT_EQ(supported_formats[0].frame_size.width(), 320);
    EXPECT_EQ(supported_formats[0].frame_size.height(), 240);
    EXPECT_EQ(supported_formats[0].pixel_format, PIXEL_FORMAT_I420);
    EXPECT_GE(supported_formats[0].frame_rate, 20.0);
    EXPECT_EQ(supported_formats[1].frame_size.width(), 640);
    EXPECT_EQ(supported_formats[1].frame_size.height(), 480);
    EXPECT_EQ(supported_formats[1].pixel_format, PIXEL_FORMAT_I420);
    EXPECT_GE(supported_formats[1].frame_rate, 20.0);
    EXPECT_EQ(supported_formats[2].frame_size.width(), 1280);
    EXPECT_EQ(supported_formats[2].frame_size.height(), 720);
    EXPECT_EQ(supported_formats[2].pixel_format, PIXEL_FORMAT_I420);
    EXPECT_GE(supported_formats[2].frame_rate, 20.0);
    EXPECT_EQ(supported_formats[3].frame_size.width(), 1920);
    EXPECT_EQ(supported_formats[3].frame_size.height(), 1080);
    EXPECT_EQ(supported_formats[3].pixel_format, PIXEL_FORMAT_I420);
    EXPECT_GE(supported_formats[3].frame_rate, 20.0);
  }
}

TEST_P(FakeVideoCaptureDeviceCommandLineTest, FrameRate) {
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kUseFakeDeviceForMediaStream, GetParam().argument);
  const std::unique_ptr<VideoCaptureDevice::Names> names(EnumerateDevices());
  ASSERT_FALSE(names->empty());

  for (const auto& names_iterator : *names) {
    std::unique_ptr<VideoCaptureDevice> device =
        video_capture_device_factory_->Create(names_iterator);
    ASSERT_TRUE(device);

    VideoCaptureParams capture_params;
    capture_params.requested_format.frame_size.SetSize(1280, 720);
    capture_params.requested_format.frame_rate = GetParam().fps;
    device->AllocateAndStart(capture_params, std::move(client_));

    WaitForCapturedFrame();
    EXPECT_EQ(last_format().frame_size.width(), 1280);
    EXPECT_EQ(last_format().frame_size.height(), 720);
    EXPECT_EQ(last_format().frame_rate, GetParam().fps);
    device->StopAndDeAllocate();
  }
}

INSTANTIATE_TEST_CASE_P(,
                        FakeVideoCaptureDeviceCommandLineTest,
                        Values(CommandLineTestData{"fps=-1", 5},
                               CommandLineTestData{"fps=29.97", 29.97f},
                               CommandLineTestData{"fps=60", 60},
                               CommandLineTestData{"fps=1000", 60}));
};  // namespace media
