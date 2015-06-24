// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "base/run_loop.h"
#include "base/test/test_timeouts.h"
#include "base/threading/thread.h"
#include "media/base/video_capture_types.h"
#include "media/video/capture/fake_video_capture_device.h"
#include "media/video/capture/fake_video_capture_device_factory.h"
#include "media/video/capture/video_capture_device.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::SaveArg;

namespace media {

namespace {

static const FakeVideoCaptureDevice::FakeVideoCaptureDeviceType
kCaptureTypes[] = {
  FakeVideoCaptureDevice::USING_OWN_BUFFERS,
  FakeVideoCaptureDevice::USING_OWN_BUFFERS_TRIPLANAR,
  FakeVideoCaptureDevice::USING_CLIENT_BUFFERS_I420,
  FakeVideoCaptureDevice::USING_CLIENT_BUFFERS_GPU,
};

// This class is a Client::Buffer that allocates and frees the requested |size|.
class MockBuffer : public VideoCaptureDevice::Client::Buffer {
 public:
  MockBuffer(int buffer_id, size_t size)
      : id_(buffer_id), size_(size), data_(new uint8[size_]) {}
  ~MockBuffer() override { delete[] data_; }

  int id() const override { return id_; }
  size_t size() const override { return size_; }
  void* data() override { return data_; }
  ClientBuffer AsClientBuffer() override { return nullptr; }
#if defined(OS_POSIX)
  base::FileDescriptor AsPlatformFile() override {
    return base::FileDescriptor();
  }
#endif

 private:
  const int id_;
  const size_t size_;
  uint8* const data_;
};

class MockClient : public VideoCaptureDevice::Client {
 public:
  MOCK_METHOD1(OnError, void(const std::string& reason));

  explicit MockClient(base::Callback<void(const VideoCaptureFormat&)> frame_cb)
      : frame_cb_(frame_cb) {}

  // Client virtual methods for capturing using Device Buffers.
  void OnIncomingCapturedData(const uint8* data,
                              int length,
                              const VideoCaptureFormat& format,
                              int rotation,
                              const base::TimeTicks& timestamp) {
    frame_cb_.Run(format);
  }
  void OnIncomingCapturedYuvData(const uint8* y_data,
                                 const uint8* u_data,
                                 const uint8* v_data,
                                 size_t y_stride,
                                 size_t u_stride,
                                 size_t v_stride,
                                 const VideoCaptureFormat& frame_format,
                                 int clockwise_rotation,
                                 const base::TimeTicks& timestamp) {
    frame_cb_.Run(frame_format);
  }

  // Virtual methods for capturing using Client's Buffers.
  scoped_ptr<Buffer> ReserveOutputBuffer(media::VideoPixelFormat format,
                                         const gfx::Size& dimensions) {
    EXPECT_TRUE(format == PIXEL_FORMAT_I420 ||
                format == PIXEL_FORMAT_GPUMEMORYBUFFER);
    EXPECT_GT(dimensions.GetArea(), 0);
    const VideoCaptureFormat frame_format(dimensions, 0.0, format);
    return make_scoped_ptr(
        new MockBuffer(0, frame_format.ImageAllocationSize()));
  }
  void OnIncomingCapturedBuffer(scoped_ptr<Buffer> buffer,
                                const VideoCaptureFormat& frame_format,
                                const base::TimeTicks& timestamp) {
    frame_cb_.Run(frame_format);
  }
  void OnIncomingCapturedVideoFrame(
      scoped_ptr<Buffer> buffer,
      const scoped_refptr<media::VideoFrame>& frame,
      const base::TimeTicks& timestamp) {
    VideoCaptureFormat format(frame->natural_size(), 30.0, PIXEL_FORMAT_I420);
    frame_cb_.Run(format);
  }

  double GetBufferPoolUtilization() const override { return 0.0; }

 private:
  base::Callback<void(const VideoCaptureFormat&)> frame_cb_;
};

class DeviceEnumerationListener :
    public base::RefCounted<DeviceEnumerationListener> {
 public:
  MOCK_METHOD1(OnEnumeratedDevicesCallbackPtr,
               void(VideoCaptureDevice::Names* names));
  // GMock doesn't support move-only arguments, so we use this forward method.
  void OnEnumeratedDevicesCallback(
      scoped_ptr<VideoCaptureDevice::Names> names) {
    OnEnumeratedDevicesCallbackPtr(names.release());
  }

 private:
  friend class base::RefCounted<DeviceEnumerationListener>;
  virtual ~DeviceEnumerationListener() {}
};

}  // namespace

class FakeVideoCaptureDeviceTest
    : public testing::TestWithParam<
          FakeVideoCaptureDevice::FakeVideoCaptureDeviceType>{
 protected:
  FakeVideoCaptureDeviceTest()
      : loop_(new base::MessageLoop()),
        client_(new MockClient(
            base::Bind(&FakeVideoCaptureDeviceTest::OnFrameCaptured,
                       base::Unretained(this)))),
        video_capture_device_factory_(new FakeVideoCaptureDeviceFactory()) {
    device_enumeration_listener_ = new DeviceEnumerationListener();
  }

  void SetUp() override {
    EXPECT_CALL(*client_, OnError(_)).Times(0);
  }

  void OnFrameCaptured(const VideoCaptureFormat& format) {
    last_format_ = format;
    run_loop_->QuitClosure().Run();
  }

  void WaitForCapturedFrame() {
    run_loop_.reset(new base::RunLoop());
    run_loop_->Run();
  }

  scoped_ptr<VideoCaptureDevice::Names> EnumerateDevices() {
    VideoCaptureDevice::Names* names;
    EXPECT_CALL(*device_enumeration_listener_.get(),
                OnEnumeratedDevicesCallbackPtr(_)).WillOnce(SaveArg<0>(&names));

    video_capture_device_factory_->EnumerateDeviceNames(
        base::Bind(&DeviceEnumerationListener::OnEnumeratedDevicesCallback,
                   device_enumeration_listener_));
    base::MessageLoop::current()->RunUntilIdle();
    return scoped_ptr<VideoCaptureDevice::Names>(names);
  }

  const VideoCaptureFormat& last_format() const { return last_format_; }

  VideoCaptureDevice::Names names_;
  const scoped_ptr<base::MessageLoop> loop_;
  scoped_ptr<base::RunLoop> run_loop_;
  scoped_ptr<MockClient> client_;
  scoped_refptr<DeviceEnumerationListener> device_enumeration_listener_;
  VideoCaptureFormat last_format_;
  const scoped_ptr<VideoCaptureDeviceFactory> video_capture_device_factory_;
};

TEST_P(FakeVideoCaptureDeviceTest, CaptureUsing) {
  const scoped_ptr<VideoCaptureDevice::Names> names(EnumerateDevices());
  ASSERT_FALSE(names->empty());

  scoped_ptr<VideoCaptureDevice> device(new FakeVideoCaptureDevice(GetParam()));
  ASSERT_TRUE(device);

  VideoCaptureParams capture_params;
  capture_params.requested_format.frame_size.SetSize(640, 480);
  capture_params.requested_format.frame_rate = 30;
  capture_params.requested_format.pixel_format = PIXEL_FORMAT_I420;
  device->AllocateAndStart(capture_params, client_.Pass());

  WaitForCapturedFrame();
  EXPECT_EQ(last_format().frame_size.width(), 640);
  EXPECT_EQ(last_format().frame_size.height(), 480);
  EXPECT_EQ(last_format().frame_rate, 30.0);
  device->StopAndDeAllocate();
}

INSTANTIATE_TEST_CASE_P(,
                        FakeVideoCaptureDeviceTest,
                        testing::ValuesIn(kCaptureTypes));

TEST_F(FakeVideoCaptureDeviceTest, GetDeviceSupportedFormats) {
  scoped_ptr<VideoCaptureDevice::Names> names(EnumerateDevices());

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

};  // namespace media
