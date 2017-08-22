// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/chromeos/stream_buffer_manager.h"

#include <map>
#include <memory>
#include <utility>
#include <vector>

#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/test/scoped_task_environment.h"
#include "base/threading/thread.h"
#include "base/threading/thread_task_runner_handle.h"
#include "media/capture/video/chromeos/camera_device_context.h"
#include "media/capture/video/chromeos/camera_device_delegate.h"
#include "media/capture/video/chromeos/mock_video_capture_client.h"
#include "media/capture/video/chromeos/mojo/arc_camera3.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::A;
using testing::AtLeast;
using testing::Invoke;
using testing::InvokeWithoutArgs;

namespace media {

namespace {

class MockStreamCaptureInterface : public StreamCaptureInterface {
 public:
  void RegisterBuffer(uint64_t buffer_id,
                      arc::mojom::Camera3DeviceOps::BufferType type,
                      uint32_t drm_format,
                      arc::mojom::HalPixelFormat hal_pixel_format,
                      uint32_t width,
                      uint32_t height,
                      std::vector<StreamCaptureInterface::Plane> planes,
                      base::OnceCallback<void(int32_t)> callback) {
    DoRegisterBuffer(buffer_id, type, drm_format, hal_pixel_format, width,
                     height, planes, callback);
  }
  MOCK_METHOD8(DoRegisterBuffer,
               void(uint64_t buffer_id,
                    arc::mojom::Camera3DeviceOps::BufferType type,
                    uint32_t drm_format,
                    arc::mojom::HalPixelFormat hal_pixel_format,
                    uint32_t width,
                    uint32_t height,
                    const std::vector<StreamCaptureInterface::Plane>& planes,
                    base::OnceCallback<void(int32_t)>& callback));

  void ProcessCaptureRequest(arc::mojom::Camera3CaptureRequestPtr request,
                             base::OnceCallback<void(int32_t)> callback) {
    DoProcessCaptureRequest(request, callback);
  }
  MOCK_METHOD2(DoProcessCaptureRequest,
               void(arc::mojom::Camera3CaptureRequestPtr& request,
                    base::OnceCallback<void(int32_t)>& callback));
};

const VideoCaptureFormat kDefaultCaptureFormat(gfx::Size(1280, 720),
                                               30.0,
                                               PIXEL_FORMAT_NV12);

}  // namespace

class StreamBufferManagerTest : public ::testing::Test {
 public:
  void SetUp() override {
    quit_ = false;
    arc::mojom::Camera3CallbackOpsRequest callback_ops_request =
        mojo::MakeRequest(&mock_callback_ops_);
    device_context_ = base::MakeUnique<CameraDeviceContext>(
        base::MakeUnique<unittest_internal::MockVideoCaptureClient>());

    stream_buffer_manager_ = base::MakeUnique<StreamBufferManager>(
        std::move(callback_ops_request),
        base::MakeUnique<MockStreamCaptureInterface>(), device_context_.get(),
        base::ThreadTaskRunnerHandle::Get());
  }

  void TearDown() override { stream_buffer_manager_.reset(); }

  void DoLoop() {
    run_loop_.reset(new base::RunLoop());
    run_loop_->Run();
  }

  void QuitCaptureLoop() {
    quit_ = true;
    if (run_loop_) {
      run_loop_->Quit();
    }
  }

  void RegisterBuffer(uint64_t buffer_id,
                      arc::mojom::Camera3DeviceOps::BufferType type,
                      uint32_t drm_format,
                      arc::mojom::HalPixelFormat hal_pixel_format,
                      uint32_t width,
                      uint32_t height,
                      const std::vector<StreamCaptureInterface::Plane>& planes,
                      base::OnceCallback<void(int32_t)>& callback) {
    if (quit_) {
      return;
    }
    std::move(callback).Run(0);
  }

  void ProcessCaptureRequest(arc::mojom::Camera3CaptureRequestPtr& request,
                             base::OnceCallback<void(int32_t)>& callback) {
    if (quit_) {
      return;
    }
    std::move(callback).Run(0);
    mock_callback_ops_->Notify(PrepareShutterNotifyMessage(
        request->frame_number, base::TimeTicks::Now().ToInternalValue()));
    mock_callback_ops_->ProcessCaptureResult(PrepareCapturedResult(
        request->frame_number, arc::mojom::CameraMetadata::New(), 1,
        std::move(request->output_buffers)));
  }

  MockStreamCaptureInterface* GetMockCaptureInterface() {
    EXPECT_NE(nullptr, stream_buffer_manager_.get());
    return reinterpret_cast<MockStreamCaptureInterface*>(
        stream_buffer_manager_->capture_interface_.get());
  }

  unittest_internal::MockVideoCaptureClient* GetMockVideoCaptureClient() {
    EXPECT_NE(nullptr, device_context_);
    return reinterpret_cast<unittest_internal::MockVideoCaptureClient*>(
        device_context_->client_.get());
  }

  std::map<uint32_t, StreamBufferManager::CaptureResult>& GetPartialResults() {
    EXPECT_NE(nullptr, stream_buffer_manager_.get());
    return stream_buffer_manager_->partial_results_;
  }

  arc::mojom::Camera3StreamPtr PrepareCaptureStream(uint32_t max_buffers) {
    auto stream = arc::mojom::Camera3Stream::New();
    stream->id = 0;
    stream->stream_type = arc::mojom::Camera3StreamType::CAMERA3_STREAM_OUTPUT;
    stream->width = kDefaultCaptureFormat.frame_size.width();
    stream->height = kDefaultCaptureFormat.frame_size.height();
    stream->format = arc::mojom::HalPixelFormat::HAL_PIXEL_FORMAT_YCbCr_420_888;
    stream->usage = 0;
    stream->max_buffers = max_buffers;
    stream->data_space = 0;
    stream->rotation =
        arc::mojom::Camera3StreamRotation::CAMERA3_STREAM_ROTATION_0;
    return stream;
  }

  arc::mojom::Camera3NotifyMsgPtr PrepareErrorNotifyMessage(
      uint32_t frame_number,
      arc::mojom::Camera3ErrorMsgCode error_code) {
    auto error_msg = arc::mojom::Camera3ErrorMsg::New();
    error_msg->frame_number = frame_number;
    // There is only the preview stream.
    error_msg->error_stream_id = 1;
    error_msg->error_code = error_code;
    auto notify_msg = arc::mojom::Camera3NotifyMsg::New();
    notify_msg->message = arc::mojom::Camera3NotifyMsgMessage::New();
    notify_msg->type = arc::mojom::Camera3MsgType::CAMERA3_MSG_ERROR;
    notify_msg->message->set_error(std::move(error_msg));
    return notify_msg;
  }

  arc::mojom::Camera3NotifyMsgPtr PrepareShutterNotifyMessage(
      uint32_t frame_number,
      uint64_t timestamp) {
    auto shutter_msg = arc::mojom::Camera3ShutterMsg::New();
    shutter_msg->frame_number = frame_number;
    shutter_msg->timestamp = timestamp;
    auto notify_msg = arc::mojom::Camera3NotifyMsg::New();
    notify_msg->message = arc::mojom::Camera3NotifyMsgMessage::New();
    notify_msg->type = arc::mojom::Camera3MsgType::CAMERA3_MSG_SHUTTER;
    notify_msg->message->set_shutter(std::move(shutter_msg));
    return notify_msg;
  }

  arc::mojom::Camera3CaptureResultPtr PrepareCapturedResult(
      uint32_t frame_number,
      arc::mojom::CameraMetadataPtr result_metadata,
      uint32_t partial_result,
      std::vector<arc::mojom::Camera3StreamBufferPtr> output_buffers) {
    auto result = arc::mojom::Camera3CaptureResult::New();
    result->frame_number = frame_number;
    result->result = std::move(result_metadata);
    if (output_buffers.size()) {
      result->output_buffers = std::move(output_buffers);
    }
    result->partial_result = partial_result;
    return result;
  }

 protected:
  std::unique_ptr<StreamBufferManager> stream_buffer_manager_;
  arc::mojom::Camera3CallbackOpsPtr mock_callback_ops_;
  std::unique_ptr<CameraDeviceContext> device_context_;
  arc::mojom::Camera3StreamPtr stream;

 private:
  std::unique_ptr<base::RunLoop> run_loop_;
  bool quit_;
  base::test::ScopedTaskEnvironment scoped_test_environment_;
};

// A basic sanity test to capture one frame with the capture loop.
TEST_F(StreamBufferManagerTest, SimpleCaptureTest) {
  GetMockVideoCaptureClient()->SetFrameCb(base::BindOnce(
      &StreamBufferManagerTest::QuitCaptureLoop, base::Unretained(this)));
  EXPECT_CALL(*GetMockCaptureInterface(),
              DoRegisterBuffer(0, arc::mojom::Camera3DeviceOps::BufferType::SHM,
                               _, _, _, _, _, _))
      .Times(AtLeast(1))
      .WillOnce(Invoke(this, &StreamBufferManagerTest::RegisterBuffer));
  EXPECT_CALL(*GetMockCaptureInterface(), DoProcessCaptureRequest(_, _))
      .Times(1)
      .WillOnce(Invoke(this, &StreamBufferManagerTest::ProcessCaptureRequest));

  stream_buffer_manager_->SetUpStreamAndBuffers(
      kDefaultCaptureFormat, /* partial_result_count */ 1,
      PrepareCaptureStream(/* max_buffers */ 1));
  stream_buffer_manager_->StartCapture(arc::mojom::CameraMetadata::New());

  // Wait until a captured frame is received by MockVideoCaptureClient.
  DoLoop();
}

// Test that the StreamBufferManager submits a captured result only after all
// partial metadata are received.
TEST_F(StreamBufferManagerTest, PartialResultTest) {
  GetMockVideoCaptureClient()->SetFrameCb(base::BindOnce(
      [](StreamBufferManagerTest* test) {
        EXPECT_EQ(1u, test->GetPartialResults().size());
        // Make sure all the three partial metadata are received before the
        // captured result is submitted.
        EXPECT_EQ(
            3u, test->GetPartialResults()[0].partial_metadata_received.size());
        test->QuitCaptureLoop();
      },
      base::Unretained(this)));
  EXPECT_CALL(*GetMockCaptureInterface(),
              DoRegisterBuffer(0, arc::mojom::Camera3DeviceOps::BufferType::SHM,
                               _, _, _, _, _, _))
      .Times(AtLeast(1))
      .WillOnce(Invoke(this, &StreamBufferManagerTest::RegisterBuffer));
  EXPECT_CALL(*GetMockCaptureInterface(), DoProcessCaptureRequest(_, _))
      .Times(1)
      .WillOnce(Invoke([this](arc::mojom::Camera3CaptureRequestPtr& request,
                              base::OnceCallback<void(int32_t)>& callback) {
        std::move(callback).Run(0);
        mock_callback_ops_->Notify(PrepareShutterNotifyMessage(
            request->frame_number, base::TimeTicks::Now().ToInternalValue()));
        mock_callback_ops_->ProcessCaptureResult(PrepareCapturedResult(
            request->frame_number, arc::mojom::CameraMetadata::New(), 1,
            std::move(request->output_buffers)));

        mock_callback_ops_->ProcessCaptureResult(PrepareCapturedResult(
            request->frame_number, arc::mojom::CameraMetadata::New(), 2,
            std::vector<arc::mojom::Camera3StreamBufferPtr>()));

        mock_callback_ops_->ProcessCaptureResult(PrepareCapturedResult(
            request->frame_number, arc::mojom::CameraMetadata::New(), 3,
            std::vector<arc::mojom::Camera3StreamBufferPtr>()));
      }));

  stream_buffer_manager_->SetUpStreamAndBuffers(
      kDefaultCaptureFormat, /* partial_result_count */ 3,
      PrepareCaptureStream(/* max_buffers */ 1));
  stream_buffer_manager_->StartCapture(arc::mojom::CameraMetadata::New());

  // Wait until a captured frame is received by MockVideoCaptureClient.
  DoLoop();
}

// Test that the capture loop is stopped and no frame is submitted when a device
// error happens.
TEST_F(StreamBufferManagerTest, DeviceErrorTest) {
  GetMockVideoCaptureClient()->SetFrameCb(base::BindOnce(
      [](StreamBufferManagerTest* test) {
        ADD_FAILURE() << "No frame should be submitted";
        test->QuitCaptureLoop();
      },
      base::Unretained(this)));
  EXPECT_CALL(*GetMockVideoCaptureClient(), OnError(_, _))
      .Times(1)
      .WillOnce(
          InvokeWithoutArgs(this, &StreamBufferManagerTest::QuitCaptureLoop));
  EXPECT_CALL(*GetMockCaptureInterface(),
              DoRegisterBuffer(0, arc::mojom::Camera3DeviceOps::BufferType::SHM,
                               _, _, _, _, _, _))
      .Times(1)
      .WillOnce(Invoke(this, &StreamBufferManagerTest::RegisterBuffer));
  EXPECT_CALL(*GetMockCaptureInterface(), DoProcessCaptureRequest(_, _))
      .Times(1)
      .WillOnce(Invoke([this](arc::mojom::Camera3CaptureRequestPtr& request,
                              base::OnceCallback<void(int32_t)>& callback) {
        std::move(callback).Run(0);
        mock_callback_ops_->Notify(PrepareErrorNotifyMessage(
            request->frame_number,
            arc::mojom::Camera3ErrorMsgCode::CAMERA3_MSG_ERROR_DEVICE));
      }));

  stream_buffer_manager_->SetUpStreamAndBuffers(
      kDefaultCaptureFormat, /* partial_result_count */ 1,
      PrepareCaptureStream(/* max_buffers */ 1));
  stream_buffer_manager_->StartCapture(arc::mojom::CameraMetadata::New());

  // Wait until the MockVideoCaptureClient is deleted.
  DoLoop();
}

// Test that upon request error the erroneous frame is dropped, and the capture
// loop continues.
TEST_F(StreamBufferManagerTest, RequestErrorTest) {
  GetMockVideoCaptureClient()->SetFrameCb(base::BindOnce(
      [](StreamBufferManagerTest* test) {
        // Frame 0 should be dropped, and the frame callback should be called
        // with frame 1.
        EXPECT_EQ(test->GetPartialResults().end(),
                  test->GetPartialResults().find(0));
        EXPECT_NE(test->GetPartialResults().end(),
                  test->GetPartialResults().find(1));
        test->QuitCaptureLoop();
      },
      base::Unretained(this)));
  EXPECT_CALL(*GetMockCaptureInterface(),
              DoRegisterBuffer(0, arc::mojom::Camera3DeviceOps::BufferType::SHM,
                               _, _, _, _, _, _))
      .Times(AtLeast(2))
      .WillOnce(Invoke(this, &StreamBufferManagerTest::RegisterBuffer))
      .WillOnce(Invoke(this, &StreamBufferManagerTest::RegisterBuffer));
  EXPECT_CALL(*GetMockCaptureInterface(), DoProcessCaptureRequest(_, _))
      .Times(2)
      .WillOnce(Invoke([this](arc::mojom::Camera3CaptureRequestPtr& request,
                              base::OnceCallback<void(int32_t)>& callback) {
        std::move(callback).Run(0);
        mock_callback_ops_->Notify(PrepareErrorNotifyMessage(
            request->frame_number,
            arc::mojom::Camera3ErrorMsgCode::CAMERA3_MSG_ERROR_REQUEST));
        request->output_buffers[0]->status =
            arc::mojom::Camera3BufferStatus::CAMERA3_BUFFER_STATUS_ERROR;
        mock_callback_ops_->ProcessCaptureResult(PrepareCapturedResult(
            request->frame_number, arc::mojom::CameraMetadata::New(), 1,
            std::move(request->output_buffers)));
      }))
      .WillOnce(Invoke(this, &StreamBufferManagerTest::ProcessCaptureRequest));

  stream_buffer_manager_->SetUpStreamAndBuffers(
      kDefaultCaptureFormat, /* partial_result_count */ 1,
      PrepareCaptureStream(/* max_buffers */ 1));
  stream_buffer_manager_->StartCapture(arc::mojom::CameraMetadata::New());

  // Wait until the MockVideoCaptureClient is deleted.
  DoLoop();
}

// Test that upon result error the captured buffer is submitted despite of the
// missing result metadata, and the capture loop continues.
TEST_F(StreamBufferManagerTest, ResultErrorTest) {
  GetMockVideoCaptureClient()->SetFrameCb(base::BindOnce(
      [](StreamBufferManagerTest* test) {
        // Frame 0 should be submitted.
        EXPECT_NE(test->GetPartialResults().end(),
                  test->GetPartialResults().find(0));
        test->QuitCaptureLoop();
      },
      base::Unretained(this)));
  EXPECT_CALL(*GetMockCaptureInterface(),
              DoRegisterBuffer(0, arc::mojom::Camera3DeviceOps::BufferType::SHM,
                               _, _, _, _, _, _))
      .Times(AtLeast(1))
      .WillOnce(Invoke(this, &StreamBufferManagerTest::RegisterBuffer));
  EXPECT_CALL(*GetMockCaptureInterface(), DoProcessCaptureRequest(_, _))
      .Times(1)
      .WillOnce(Invoke([this](arc::mojom::Camera3CaptureRequestPtr& request,
                              base::OnceCallback<void(int32_t)>& callback) {
        std::move(callback).Run(0);
        mock_callback_ops_->Notify(PrepareShutterNotifyMessage(
            request->frame_number, base::TimeTicks::Now().ToInternalValue()));
        mock_callback_ops_->ProcessCaptureResult(PrepareCapturedResult(
            request->frame_number, arc::mojom::CameraMetadata::New(), 1,
            std::move(request->output_buffers)));
        // Send a result error notify without sending the second partial result.
        // StreamBufferManager should submit the buffer when it receives the
        // result error.
        mock_callback_ops_->Notify(PrepareErrorNotifyMessage(
            request->frame_number,
            arc::mojom::Camera3ErrorMsgCode::CAMERA3_MSG_ERROR_RESULT));
      }))
      .WillOnce(Invoke(this, &StreamBufferManagerTest::ProcessCaptureRequest));

  stream_buffer_manager_->SetUpStreamAndBuffers(
      kDefaultCaptureFormat, /* partial_result_count */ 2,
      PrepareCaptureStream(/* max_buffers */ 1));
  stream_buffer_manager_->StartCapture(arc::mojom::CameraMetadata::New());

  // Wait until the MockVideoCaptureClient is deleted.
  DoLoop();
}

// Test that upon buffer error the erroneous buffer is dropped, and the capture
// loop continues.
TEST_F(StreamBufferManagerTest, BufferErrorTest) {
  GetMockVideoCaptureClient()->SetFrameCb(base::BindOnce(
      [](StreamBufferManagerTest* test) {
        // Frame 0 should be dropped, and the frame callback should be called
        // with frame 1.
        EXPECT_EQ(test->GetPartialResults().end(),
                  test->GetPartialResults().find(0));
        EXPECT_NE(test->GetPartialResults().end(),
                  test->GetPartialResults().find(1));
        test->QuitCaptureLoop();
      },
      base::Unretained(this)));
  EXPECT_CALL(*GetMockCaptureInterface(),
              DoRegisterBuffer(0, arc::mojom::Camera3DeviceOps::BufferType::SHM,
                               _, _, _, _, _, _))
      .Times(AtLeast(2))
      .WillOnce(Invoke(this, &StreamBufferManagerTest::RegisterBuffer))
      .WillOnce(Invoke(this, &StreamBufferManagerTest::RegisterBuffer));
  EXPECT_CALL(*GetMockCaptureInterface(), DoProcessCaptureRequest(_, _))
      .Times(2)
      .WillOnce(Invoke([this](arc::mojom::Camera3CaptureRequestPtr& request,
                              base::OnceCallback<void(int32_t)>& callback) {
        std::move(callback).Run(0);
        mock_callback_ops_->Notify(PrepareShutterNotifyMessage(
            request->frame_number, base::TimeTicks::Now().ToInternalValue()));
        mock_callback_ops_->Notify(PrepareErrorNotifyMessage(
            request->frame_number,
            arc::mojom::Camera3ErrorMsgCode::CAMERA3_MSG_ERROR_BUFFER));
        request->output_buffers[0]->status =
            arc::mojom::Camera3BufferStatus::CAMERA3_BUFFER_STATUS_ERROR;
        mock_callback_ops_->ProcessCaptureResult(PrepareCapturedResult(
            request->frame_number, arc::mojom::CameraMetadata::New(), 1,
            std::move(request->output_buffers)));
      }))
      .WillOnce(Invoke(this, &StreamBufferManagerTest::ProcessCaptureRequest));

  stream_buffer_manager_->SetUpStreamAndBuffers(
      kDefaultCaptureFormat, /* partial_result_count */ 1,
      PrepareCaptureStream(/* max_buffers */ 1));
  stream_buffer_manager_->StartCapture(arc::mojom::CameraMetadata::New());

  // Wait until the MockVideoCaptureClient is deleted.
  DoLoop();
}

}  // namespace media
