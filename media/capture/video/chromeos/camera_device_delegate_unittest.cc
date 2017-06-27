// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/chromeos/camera_device_delegate.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <utility>

#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/test/scoped_task_environment.h"
#include "media/base/bind_to_current_loop.h"
#include "media/capture/video/chromeos/camera_device_context.h"
#include "media/capture/video/chromeos/camera_hal_delegate.h"
#include "media/capture/video/chromeos/mock_camera_module.h"
#include "media/capture/video/chromeos/mock_video_capture_client.h"
#include "media/capture/video/chromeos/mojo/arc_camera3.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::A;
using testing::AtLeast;
using testing::Invoke;

namespace media {

namespace {

class MockCameraDevice : public arc::mojom::Camera3DeviceOps {
 public:
  MockCameraDevice() {}

  ~MockCameraDevice() {}

  void Initialize(arc::mojom::Camera3CallbackOpsPtr callback_ops,
                  InitializeCallback callback) override {
    DoInitialize(callback_ops, callback);
  }
  MOCK_METHOD2(DoInitialize,
               void(arc::mojom::Camera3CallbackOpsPtr& callback_ops,
                    InitializeCallback& callback));

  void ConfigureStreams(arc::mojom::Camera3StreamConfigurationPtr config,
                        ConfigureStreamsCallback callback) override {
    DoConfigureStreams(config, callback);
  }
  MOCK_METHOD2(DoConfigureStreams,
               void(arc::mojom::Camera3StreamConfigurationPtr& config,
                    ConfigureStreamsCallback& callback));

  void ConstructDefaultRequestSettings(
      arc::mojom::Camera3RequestTemplate type,
      ConstructDefaultRequestSettingsCallback callback) override {
    DoConstructDefaultRequestSettings(type, callback);
  }
  MOCK_METHOD2(DoConstructDefaultRequestSettings,
               void(arc::mojom::Camera3RequestTemplate type,
                    ConstructDefaultRequestSettingsCallback& callback));

  void ProcessCaptureRequest(arc::mojom::Camera3CaptureRequestPtr request,
                             ProcessCaptureRequestCallback callback) override {
    DoProcessCaptureRequest(request, callback);
  }
  MOCK_METHOD2(DoProcessCaptureRequest,
               void(arc::mojom::Camera3CaptureRequestPtr& request,
                    ProcessCaptureRequestCallback& callback));

  void Dump(mojo::ScopedHandle fd) override { DoDump(fd); }
  MOCK_METHOD1(DoDump, void(mojo::ScopedHandle& fd));

  void Flush(FlushCallback callback) override { DoFlush(callback); }
  MOCK_METHOD1(DoFlush, void(FlushCallback& callback));

  void RegisterBuffer(uint64_t buffer_id,
                      arc::mojom::Camera3DeviceOps::BufferType type,
                      std::vector<mojo::ScopedHandle> fds,
                      uint32_t drm_format,
                      arc::mojom::HalPixelFormat hal_pixel_format,
                      uint32_t width,
                      uint32_t height,
                      const std::vector<uint32_t>& strides,
                      const std::vector<uint32_t>& offsets,
                      RegisterBufferCallback callback) override {
    DoRegisterBuffer(buffer_id, type, fds, drm_format, hal_pixel_format, width,
                     height, strides, offsets, callback);
  }
  MOCK_METHOD10(DoRegisterBuffer,
                void(uint64_t buffer_id,
                     arc::mojom::Camera3DeviceOps::BufferType type,
                     std::vector<mojo::ScopedHandle>& fds,
                     uint32_t drm_format,
                     arc::mojom::HalPixelFormat hal_pixel_format,
                     uint32_t width,
                     uint32_t height,
                     const std::vector<uint32_t>& strides,
                     const std::vector<uint32_t>& offsets,
                     RegisterBufferCallback& callback));

  void Close(CloseCallback callback) override { DoClose(callback); }
  MOCK_METHOD1(DoClose, void(CloseCallback& callback));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockCameraDevice);
};

const VideoCaptureDeviceDescriptor kDefaultDescriptor("Fake device", "0");
const VideoCaptureFormat kDefaultCaptureFormat(gfx::Size(1280, 720),
                                               30.0,
                                               PIXEL_FORMAT_I420);

}  // namespace

class CameraDeviceDelegateTest : public ::testing::Test {
 public:
  CameraDeviceDelegateTest()
      : mock_camera_device_binding_(&mock_camera_device_),
        device_delegate_thread_("DeviceDelegateThread"),
        hal_delegate_thread_("HalDelegateThread") {}

  void SetUp() override {
    hal_delegate_thread_.Start();
    camera_hal_delegate_ =
        new CameraHalDelegate(hal_delegate_thread_.task_runner());
    camera_hal_delegate_->StartForTesting(
        mock_camera_module_.GetInterfacePtrInfo());

    ResetCaptureClient();
  }

  void TearDown() override {
    camera_hal_delegate_->Reset();
    hal_delegate_thread_.Stop();
  }

  void AllocateDeviceWithDescriptor(VideoCaptureDeviceDescriptor descriptor) {
    ASSERT_FALSE(device_delegate_thread_.IsRunning());
    ASSERT_FALSE(camera_device_delegate_);
    device_delegate_thread_.Start();
    camera_device_delegate_ = base::MakeUnique<CameraDeviceDelegate>(
        descriptor, camera_hal_delegate_,
        device_delegate_thread_.task_runner());
  }

  void GetFakeCameraInfo(uint32_t camera_id,
                         arc::mojom::CameraModule::GetCameraInfoCallback& cb) {
    arc::mojom::CameraInfoPtr camera_info = arc::mojom::CameraInfo::New();
    arc::mojom::CameraMetadataPtr static_metadata =
        arc::mojom::CameraMetadata::New();
    switch (camera_id) {
      case 0:
        camera_info->facing = arc::mojom::CameraFacing::CAMERA_FACING_FRONT;
        camera_info->orientation = 0;
        camera_info->static_camera_characteristics = std::move(static_metadata);
        break;
      default:
        FAIL() << "Invalid camera id";
    }
    std::move(cb).Run(0, std::move(camera_info));
  }

  void OpenMockCameraDevice(
      int32_t camera_id,
      arc::mojom::Camera3DeviceOpsRequest& device_ops_request,
      base::OnceCallback<void(int32_t)>& callback) {
    mock_camera_device_binding_.Bind(std::move(device_ops_request));
    std::move(callback).Run(0);
  }

  void InitializeMockCameraDevice(
      arc::mojom::Camera3CallbackOpsPtr& callback_ops,
      base::OnceCallback<void(int32_t)>& callback) {
    callback_ops_ = std::move(callback_ops);
    std::move(callback).Run(0);
  }

  void ConfigureFakeStreams(
      arc::mojom::Camera3StreamConfigurationPtr& config,
      base::OnceCallback<
          void(int32_t, arc::mojom::Camera3StreamConfigurationPtr)>& callback) {
    ASSERT_EQ(1u, config->streams.size());
    ASSERT_EQ(static_cast<uint32_t>(kDefaultCaptureFormat.frame_size.width()),
              config->streams[0]->width);
    ASSERT_EQ(static_cast<uint32_t>(kDefaultCaptureFormat.frame_size.height()),
              config->streams[0]->height);
    ASSERT_EQ(arc::mojom::HalPixelFormat::HAL_PIXEL_FORMAT_YCbCr_420_888,
              config->streams[0]->format);
    config->streams[0]->usage = 0;
    config->streams[0]->max_buffers = 1;
    std::move(callback).Run(0, std::move(config));
  }

  void ConstructFakeRequestSettings(
      arc::mojom::Camera3RequestTemplate type,
      base::OnceCallback<void(arc::mojom::CameraMetadataPtr)>& callback) {
    ASSERT_EQ(arc::mojom::Camera3RequestTemplate::CAMERA3_TEMPLATE_PREVIEW,
              type);
    arc::mojom::CameraMetadataPtr fake_settings =
        arc::mojom::CameraMetadata::New();
    fake_settings->entry_count = 1;
    fake_settings->entry_capacity = 1;
    fake_settings->entries = std::vector<arc::mojom::CameraMetadataEntryPtr>();
    std::move(callback).Run(std::move(fake_settings));
  }

  void RegisterBuffer(uint64_t buffer_id,
                      arc::mojom::Camera3DeviceOps::BufferType type,
                      std::vector<mojo::ScopedHandle>& fds,
                      uint32_t drm_format,
                      arc::mojom::HalPixelFormat hal_pixel_format,
                      uint32_t width,
                      uint32_t height,
                      const std::vector<uint32_t>& strides,
                      const std::vector<uint32_t>& offsets,
                      base::OnceCallback<void(int32_t)>& callback) {
    std::move(callback).Run(0);
  }

  void ProcessCaptureRequest(arc::mojom::Camera3CaptureRequestPtr& request,
                             base::OnceCallback<void(int32_t)>& callback) {
    std::move(callback).Run(0);

    arc::mojom::Camera3NotifyMsgPtr msg = arc::mojom::Camera3NotifyMsg::New();
    msg->type = arc::mojom::Camera3MsgType::CAMERA3_MSG_SHUTTER;
    msg->message = arc::mojom::Camera3NotifyMsgMessage::New();
    arc::mojom::Camera3ShutterMsgPtr shutter_msg =
        arc::mojom::Camera3ShutterMsg::New();
    shutter_msg->timestamp = base::TimeTicks::Now().ToInternalValue();
    msg->message->set_shutter(std::move(shutter_msg));
    callback_ops_->Notify(std::move(msg));

    arc::mojom::Camera3CaptureResultPtr result =
        arc::mojom::Camera3CaptureResult::New();
    result->frame_number = request->frame_number;
    result->result = arc::mojom::CameraMetadata::New();
    result->output_buffers = std::move(request->output_buffers);
    result->partial_result = 1;
    callback_ops_->ProcessCaptureResult(std::move(result));
  }

  void CloseMockCameraDevice(base::OnceCallback<void(int32_t)>& callback) {
    if (mock_camera_device_binding_.is_bound()) {
      mock_camera_device_binding_.Close();
    }
    callback_ops_.reset();
    std::move(callback).Run(0);
  }

  void SetUpExpectationUntilInitialized() {
    EXPECT_CALL(mock_camera_module_, DoGetCameraInfo(0, _))
        .Times(1)
        .WillOnce(Invoke(this, &CameraDeviceDelegateTest::GetFakeCameraInfo));
    EXPECT_CALL(mock_camera_module_, DoOpenDevice(0, _, _))
        .Times(1)
        .WillOnce(
            Invoke(this, &CameraDeviceDelegateTest::OpenMockCameraDevice));
    EXPECT_CALL(mock_camera_device_, DoInitialize(_, _))
        .Times(1)
        .WillOnce(Invoke(
            this, &CameraDeviceDelegateTest::InitializeMockCameraDevice));
  }

  void SetUpExpectationUntilStreamConfigured() {
    SetUpExpectationUntilInitialized();
    EXPECT_CALL(mock_camera_device_, DoConfigureStreams(_, _))
        .Times(1)
        .WillOnce(
            Invoke(this, &CameraDeviceDelegateTest::ConfigureFakeStreams));
  }

  void SetUpExpectationUntilCapturing(
      unittest_internal::MockVideoCaptureClient* mock_client) {
    SetUpExpectationUntilStreamConfigured();
    EXPECT_CALL(mock_camera_device_, DoConstructDefaultRequestSettings(_, _))
        .Times(1)
        .WillOnce(Invoke(
            this, &CameraDeviceDelegateTest::ConstructFakeRequestSettings));
    EXPECT_CALL(*mock_client, OnStarted()).Times(1);
  }

  void SetUpExpectationForCaptureLoop() {
    EXPECT_CALL(mock_camera_device_,
                DoRegisterBuffer(_, _, _, _, _, _, _, _, _, _))
        .Times(AtLeast(1))
        .WillOnce(Invoke(this, &CameraDeviceDelegateTest::RegisterBuffer))
        .WillRepeatedly(
            Invoke(this, &CameraDeviceDelegateTest::RegisterBuffer));
    EXPECT_CALL(mock_camera_device_, DoProcessCaptureRequest(_, _))
        .Times(AtLeast(1))
        .WillOnce(
            Invoke(this, &CameraDeviceDelegateTest::ProcessCaptureRequest))
        .WillRepeatedly(
            Invoke(this, &CameraDeviceDelegateTest::ProcessCaptureRequest));
  }

  void SetUpExpectationForClose() {
    EXPECT_CALL(mock_camera_device_, DoClose(_))
        .Times(1)
        .WillOnce(
            Invoke(this, &CameraDeviceDelegateTest::CloseMockCameraDevice));
  }

  void WaitForDeviceToClose() {
    base::WaitableEvent device_closed(
        base::WaitableEvent::ResetPolicy::MANUAL,
        base::WaitableEvent::InitialState::NOT_SIGNALED);
    device_delegate_thread_.task_runner()->PostTask(
        FROM_HERE, base::Bind(&CameraDeviceDelegate::StopAndDeAllocate,
                              camera_device_delegate_->GetWeakPtr(),
                              base::Bind(
                                  [](base::WaitableEvent* device_closed) {
                                    device_closed->Signal();
                                  },
                                  base::Unretained(&device_closed))));
    base::TimeDelta kWaitTimeoutSecs = base::TimeDelta::FromSeconds(3);
    EXPECT_TRUE(device_closed.TimedWait(kWaitTimeoutSecs));
    EXPECT_EQ(CameraDeviceContext::State::kStopped, GetState());
  }

  void ResetCaptureClient() {
    mock_client_ =
        base::MakeUnique<unittest_internal::MockVideoCaptureClient>();
  }

  void ResetDevice() {
    ASSERT_TRUE(device_delegate_thread_.IsRunning());
    ASSERT_TRUE(camera_device_delegate_);
    device_delegate_thread_.Stop();
    camera_device_delegate_.reset();
  }

  void DoLoop() {
    run_loop_.reset(new base::RunLoop());
    run_loop_->Run();
  }

  void QuitRunLoop() {
    VLOG(2) << "quit!";
    if (run_loop_) {
      run_loop_->Quit();
    }
  }

  CameraDeviceContext::State GetState() {
    if (camera_device_delegate_->device_context_) {
      return camera_device_delegate_->device_context_->GetState();
    } else {
      // No device context means the VCD is either not started yet or already
      // stopped.
      return CameraDeviceContext::State::kStopped;
    }
  }

 protected:
  scoped_refptr<CameraHalDelegate> camera_hal_delegate_;
  std::unique_ptr<CameraDeviceDelegate> camera_device_delegate_;

  testing::StrictMock<unittest_internal::MockCameraModule> mock_camera_module_;

  testing::StrictMock<MockCameraDevice> mock_camera_device_;
  mojo::Binding<arc::mojom::Camera3DeviceOps> mock_camera_device_binding_;
  arc::mojom::Camera3CallbackOpsPtr callback_ops_;

  base::Thread device_delegate_thread_;

  std::unique_ptr<VideoCaptureDevice::Client> mock_client_;

 private:
  base::test::ScopedTaskEnvironment scoped_task_environment_;
  base::Thread hal_delegate_thread_;
  std::unique_ptr<base::RunLoop> run_loop_;
  DISALLOW_COPY_AND_ASSIGN(CameraDeviceDelegateTest);
};

// Test the complete capture flow: initialize, configure stream, capture one
// frame, and close the device.
TEST_F(CameraDeviceDelegateTest, AllocateCaptureAndStop) {
  AllocateDeviceWithDescriptor(kDefaultDescriptor);

  VideoCaptureParams params;
  params.requested_format = kDefaultCaptureFormat;

  auto* mock_client =
      reinterpret_cast<unittest_internal::MockVideoCaptureClient*>(
          mock_client_.get());
  mock_client->SetFrameCb(BindToCurrentLoop(base::BindOnce(
      &CameraDeviceDelegateTest::QuitRunLoop, base::Unretained(this))));
  mock_client->SetQuitCb(BindToCurrentLoop(base::BindOnce(
      &CameraDeviceDelegateTest::QuitRunLoop, base::Unretained(this))));
  SetUpExpectationUntilCapturing(mock_client);
  SetUpExpectationForCaptureLoop();

  device_delegate_thread_.task_runner()->PostTask(
      FROM_HERE, base::Bind(&CameraDeviceDelegate::AllocateAndStart,
                            camera_device_delegate_->GetWeakPtr(), params,
                            base::Passed(&mock_client_)));

  // Wait until a frame is received.  MockVideoCaptureClient calls QuitRunLoop()
  // to stop the run loop.
  DoLoop();
  EXPECT_EQ(CameraDeviceContext::State::kCapturing, GetState());

  SetUpExpectationForClose();

  WaitForDeviceToClose();

  ResetDevice();
}

// Test that the camera device delegate closes properly when StopAndDeAllocate
// is called right after the device is initialized.
TEST_F(CameraDeviceDelegateTest, StopAfterInitialized) {
  AllocateDeviceWithDescriptor(kDefaultDescriptor);

  VideoCaptureParams params;
  params.requested_format = kDefaultCaptureFormat;

  auto* mock_client =
      reinterpret_cast<unittest_internal::MockVideoCaptureClient*>(
          mock_client_.get());
  mock_client->SetQuitCb(BindToCurrentLoop(base::BindOnce(
      &CameraDeviceDelegateTest::QuitRunLoop, base::Unretained(this))));
  SetUpExpectationUntilInitialized();

  device_delegate_thread_.task_runner()->PostTask(
      FROM_HERE, base::Bind(&CameraDeviceDelegate::AllocateAndStart,
                            camera_device_delegate_->GetWeakPtr(), params,
                            base::Passed(&mock_client_)));

  EXPECT_CALL(mock_camera_device_, DoConfigureStreams(_, _))
      .Times(1)
      .WillOnce(Invoke(
          [this](arc::mojom::Camera3StreamConfigurationPtr& config,
                 base::OnceCallback<void(
                     int32_t, arc::mojom::Camera3StreamConfigurationPtr)>&
                     callback) {
            EXPECT_EQ(CameraDeviceContext::State::kInitialized,
                      this->GetState());
            this->QuitRunLoop();
            std::move(callback).Run(
                0, arc::mojom::Camera3StreamConfiguration::New());
          }));

  // Wait until the QuitRunLoop call in |mock_camera_device_->ConfigureStreams|.
  DoLoop();

  SetUpExpectationForClose();

  WaitForDeviceToClose();

  ResetDevice();
}

// Test that the camera device delegate closes properly when StopAndDeAllocate
// is called right after the stream is configured.
TEST_F(CameraDeviceDelegateTest, StopAfterStreamConfigured) {
  AllocateDeviceWithDescriptor(kDefaultDescriptor);

  VideoCaptureParams params;
  params.requested_format = kDefaultCaptureFormat;

  auto* mock_client =
      reinterpret_cast<unittest_internal::MockVideoCaptureClient*>(
          mock_client_.get());
  mock_client->SetQuitCb(BindToCurrentLoop(base::BindOnce(
      &CameraDeviceDelegateTest::QuitRunLoop, base::Unretained(this))));
  SetUpExpectationUntilStreamConfigured();

  device_delegate_thread_.task_runner()->PostTask(
      FROM_HERE, base::Bind(&CameraDeviceDelegate::AllocateAndStart,
                            camera_device_delegate_->GetWeakPtr(), params,
                            base::Passed(&mock_client_)));

  EXPECT_CALL(mock_camera_device_, DoConstructDefaultRequestSettings(_, _))
      .Times(1)
      .WillOnce(
          Invoke([this](arc::mojom::Camera3RequestTemplate type,
                        base::OnceCallback<void(arc::mojom::CameraMetadataPtr)>&
                            callback) {
            EXPECT_EQ(CameraDeviceContext::State::kStreamConfigured,
                      this->GetState());
            this->QuitRunLoop();
            std::move(callback).Run(arc::mojom::CameraMetadataPtr());
          }));

  // Wait until the QuitRunLoop call in |mock_camera_device_->ConfigureStreams|.
  DoLoop();

  SetUpExpectationForClose();

  WaitForDeviceToClose();

  ResetDevice();
}

}  // namespace media
