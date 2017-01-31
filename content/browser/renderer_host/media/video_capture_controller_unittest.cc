// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Unit test for VideoCaptureController.

#include "content/browser/renderer_host/media/video_capture_controller.h"

#include <stdint.h>
#include <string.h>

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/browser/renderer_host/media/media_stream_provider.h"
#include "content/browser/renderer_host/media/video_capture_controller_event_handler.h"
#include "content/browser/renderer_host/media/video_capture_gpu_jpeg_decoder.h"
#include "content/browser/renderer_host/media/video_capture_manager.h"
#include "content/browser/renderer_host/media/video_frame_receiver_on_io_thread.h"
#include "content/common/media/media_stream_options.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "media/base/video_frame_metadata.h"
#include "media/base/video_util.h"
#include "media/capture/video/video_capture_buffer_pool_impl.h"
#include "media/capture/video/video_capture_buffer_tracker_factory_impl.h"
#include "media/capture/video/video_capture_device_client.h"
#include "media/capture/video_capture_types.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::InSequence;
using ::testing::Mock;
using ::testing::SaveArg;

namespace content {

std::unique_ptr<media::VideoCaptureJpegDecoder> CreateGpuJpegDecoder(
    const media::VideoCaptureJpegDecoder::DecodeDoneCB& decode_done_cb) {
  return base::MakeUnique<content::VideoCaptureGpuJpegDecoder>(decode_done_cb);
}

class MockVideoCaptureControllerEventHandler
    : public VideoCaptureControllerEventHandler {
 public:
  explicit MockVideoCaptureControllerEventHandler(
      VideoCaptureController* controller)
      : controller_(controller), resource_utilization_(-1.0) {}
  ~MockVideoCaptureControllerEventHandler() override {}

  // These mock methods are delegated to by our fake implementation of
  // VideoCaptureControllerEventHandler, to be used in EXPECT_CALL().
  MOCK_METHOD1(DoBufferCreated, void(VideoCaptureControllerID));
  MOCK_METHOD1(DoBufferDestroyed, void(VideoCaptureControllerID));
  MOCK_METHOD2(DoBufferReady, void(VideoCaptureControllerID, const gfx::Size&));
  MOCK_METHOD1(DoEnded, void(VideoCaptureControllerID));
  MOCK_METHOD1(DoError, void(VideoCaptureControllerID));

  void OnError(VideoCaptureControllerID id) override { DoError(id); }
  void OnBufferCreated(VideoCaptureControllerID id,
                       mojo::ScopedSharedBufferHandle handle,
                       int length,
                       int buffer_id) override {
    DoBufferCreated(id);
  }
  void OnBufferDestroyed(VideoCaptureControllerID id, int buffer_id) override {
    DoBufferDestroyed(id);
  }
  void OnBufferReady(VideoCaptureControllerID id,
                     int buffer_id,
                     const scoped_refptr<media::VideoFrame>& frame) override {
    EXPECT_EQ(expected_pixel_format_, frame->format());
    base::TimeTicks reference_time;
    EXPECT_TRUE(frame->metadata()->GetTimeTicks(
        media::VideoFrameMetadata::REFERENCE_TIME, &reference_time));
    DoBufferReady(id, frame->coded_size());
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(&VideoCaptureController::ReturnBuffer,
                              base::Unretained(controller_), id, this,
                              buffer_id, resource_utilization_));
  }
  void OnEnded(VideoCaptureControllerID id) override {
    DoEnded(id);
    // OnEnded() must respond by (eventually) unregistering the client.
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::Bind(base::IgnoreResult(&VideoCaptureController::RemoveClient),
                   base::Unretained(controller_), id, this));
  }

  VideoCaptureController* controller_;
  media::VideoPixelFormat expected_pixel_format_ = media::PIXEL_FORMAT_I420;
  double resource_utilization_;
};

class MockConsumerFeedbackObserver
    : public media::VideoFrameConsumerFeedbackObserver {
 public:
  MOCK_METHOD2(OnUtilizationReport,
               void(int frame_feedback_id, double utilization));
};

class MockFrameBufferPool : public media::FrameBufferPool {
 public:
  MOCK_METHOD1(SetBufferHold, void(int buffer_id));
  MOCK_METHOD1(ReleaseBufferHold, void(int buffer_id));
};

// Test class.
class VideoCaptureControllerTest
    : public testing::Test,
      public testing::WithParamInterface<media::VideoPixelFormat> {
 public:
  VideoCaptureControllerTest() {}
  ~VideoCaptureControllerTest() override {}

 protected:
  static const int kPoolSize = 3;

  void SetUp() override {
    scoped_refptr<media::VideoCaptureBufferPool> buffer_pool(
        new media::VideoCaptureBufferPoolImpl(
            base::MakeUnique<media::VideoCaptureBufferTrackerFactoryImpl>(),
            kPoolSize));
    controller_.reset(new VideoCaptureController());
    device_client_.reset(new media::VideoCaptureDeviceClient(
        base::MakeUnique<VideoFrameReceiverOnIOThread>(
            controller_->GetWeakPtrForIOThread()),
        buffer_pool,
        base::Bind(
            &CreateGpuJpegDecoder,
            base::Bind(&media::VideoFrameReceiver::OnIncomingCapturedVideoFrame,
                       controller_->GetWeakPtrForIOThread()))));
    auto frame_receiver_observer = base::MakeUnique<MockFrameBufferPool>();
    mock_frame_receiver_observer_ = frame_receiver_observer.get();
    controller_->SetFrameBufferPool(std::move(frame_receiver_observer));
    auto consumer_feedback_observer =
        base::MakeUnique<MockConsumerFeedbackObserver>();
    mock_consumer_feedback_observer_ = consumer_feedback_observer.get();
    controller_->SetConsumerFeedbackObserver(
        std::move(consumer_feedback_observer));
    client_a_.reset(
        new MockVideoCaptureControllerEventHandler(controller_.get()));
    client_b_.reset(
        new MockVideoCaptureControllerEventHandler(controller_.get()));
  }

  void TearDown() override { base::RunLoop().RunUntilIdle(); }

  TestBrowserThreadBundle bundle_;
  std::unique_ptr<MockVideoCaptureControllerEventHandler> client_a_;
  std::unique_ptr<MockVideoCaptureControllerEventHandler> client_b_;
  std::unique_ptr<VideoCaptureController> controller_;
  std::unique_ptr<media::VideoCaptureDevice::Client> device_client_;
  MockFrameBufferPool* mock_frame_receiver_observer_;
  MockConsumerFeedbackObserver* mock_consumer_feedback_observer_;
  const float arbitrary_frame_rate_ = 10.0f;
  const base::TimeTicks arbitrary_reference_time_ = base::TimeTicks();
  const base::TimeDelta arbitrary_timestamp_ = base::TimeDelta();

 private:
  DISALLOW_COPY_AND_ASSIGN(VideoCaptureControllerTest);
};

// A simple test of VideoCaptureController's ability to add, remove, and keep
// track of clients.
TEST_F(VideoCaptureControllerTest, AddAndRemoveClients) {
  media::VideoCaptureParams session_100;
  session_100.requested_format = media::VideoCaptureFormat(
      gfx::Size(320, 240), 30, media::PIXEL_FORMAT_I420);
  media::VideoCaptureParams session_200 = session_100;

  media::VideoCaptureParams session_300 = session_100;

  media::VideoCaptureParams session_400 = session_100;

  // Intentionally use the same route ID for two of the clients: the device_ids
  // are a per-VideoCaptureHost namespace, and can overlap across hosts.
  const VideoCaptureControllerID client_a_route_1(44);
  const VideoCaptureControllerID client_a_route_2(30);
  const VideoCaptureControllerID client_b_route_1(30);
  const VideoCaptureControllerID client_b_route_2(1);

  // Clients in controller: []
  ASSERT_EQ(0, controller_->GetClientCount())
      << "Client count should initially be zero.";
  controller_->AddClient(client_a_route_1, client_a_.get(), 100, session_100);
  // Clients in controller: [A/1]
  ASSERT_EQ(1, controller_->GetClientCount())
      << "Adding client A/1 should bump client count.";
  controller_->AddClient(client_a_route_2, client_a_.get(), 200, session_200);
  // Clients in controller: [A/1, A/2]
  ASSERT_EQ(2, controller_->GetClientCount())
      << "Adding client A/2 should bump client count.";
  controller_->AddClient(client_b_route_1, client_b_.get(), 300, session_300);
  // Clients in controller: [A/1, A/2, B/1]
  ASSERT_EQ(3, controller_->GetClientCount())
      << "Adding client B/1 should bump client count.";
  ASSERT_EQ(200, controller_->RemoveClient(client_a_route_2, client_a_.get()))
      << "Removing client A/1 should return its session_id.";
  // Clients in controller: [A/1, B/1]
  ASSERT_EQ(2, controller_->GetClientCount());
  ASSERT_EQ(static_cast<int>(kInvalidMediaCaptureSessionId),
            controller_->RemoveClient(client_a_route_2, client_a_.get()))
      << "Removing a nonexistant client should fail.";
  // Clients in controller: [A/1, B/1]
  ASSERT_EQ(2, controller_->GetClientCount());
  ASSERT_EQ(300, controller_->RemoveClient(client_b_route_1, client_b_.get()))
      << "Removing client B/1 should return its session_id.";
  // Clients in controller: [A/1]
  ASSERT_EQ(1, controller_->GetClientCount());
  controller_->AddClient(client_b_route_2, client_b_.get(), 400, session_400);
  // Clients in controller: [A/1, B/2]

  EXPECT_CALL(*client_a_, DoEnded(client_a_route_1)).Times(1);
  controller_->StopSession(100);  // Session 100 == client A/1
  Mock::VerifyAndClearExpectations(client_a_.get());
  ASSERT_EQ(2, controller_->GetClientCount())
      << "Client should be closed but still exist after StopSession.";
  // Clients in controller: [A/1 (closed, removal pending), B/2]
  base::RunLoop().RunUntilIdle();
  // Clients in controller: [B/2]
  ASSERT_EQ(1, controller_->GetClientCount())
      << "Client A/1 should be deleted by now.";
  controller_->StopSession(200);  // Session 200 does not exist anymore
  // Clients in controller: [B/2]
  ASSERT_EQ(1, controller_->GetClientCount())
      << "Stopping non-existant session 200 should be a no-op.";
  controller_->StopSession(256);  // Session 256 never existed.
  // Clients in controller: [B/2]
  ASSERT_EQ(1, controller_->GetClientCount())
      << "Stopping non-existant session 256 should be a no-op.";
  ASSERT_EQ(static_cast<int>(kInvalidMediaCaptureSessionId),
            controller_->RemoveClient(client_a_route_1, client_a_.get()))
      << "Removing already-removed client A/1 should fail.";
  // Clients in controller: [B/2]
  ASSERT_EQ(1, controller_->GetClientCount())
      << "Removing non-existant session 200 should be a no-op.";
  ASSERT_EQ(400, controller_->RemoveClient(client_b_route_2, client_b_.get()))
      << "Removing client B/2 should return its session_id.";
  // Clients in controller: []
  ASSERT_EQ(0, controller_->GetClientCount())
      << "Client count should return to zero after all clients are gone.";
}

// This test will connect and disconnect several clients while simulating an
// active capture device being started and generating frames. It runs on one
// thread and is intended to behave deterministically.
TEST_P(VideoCaptureControllerTest, NormalCaptureMultipleClients) {
  media::VideoCaptureParams session_100;
  const media::VideoPixelFormat format = GetParam();
  client_a_->expected_pixel_format_ = format;
  client_b_->expected_pixel_format_ = format;

  session_100.requested_format =
      media::VideoCaptureFormat(gfx::Size(320, 240), 30, format);

  media::VideoCaptureParams session_200 = session_100;

  media::VideoCaptureParams session_300 = session_100;

  media::VideoCaptureParams session_1 = session_100;

  media::VideoCaptureFormat device_format(gfx::Size(444, 200), 25, format);

  const VideoCaptureControllerID client_a_route_1(0xa1a1a1a1);
  const VideoCaptureControllerID client_a_route_2(0xa2a2a2a2);
  const VideoCaptureControllerID client_b_route_1(0xb1b1b1b1);
  const VideoCaptureControllerID client_b_route_2(0xb2b2b2b2);

  controller_->AddClient(client_a_route_1, client_a_.get(), 100, session_100);
  controller_->AddClient(client_b_route_1, client_b_.get(), 300, session_300);
  controller_->AddClient(client_a_route_2, client_a_.get(), 200, session_200);
  ASSERT_EQ(3, controller_->GetClientCount());

  // Now, simulate an incoming captured buffer from the capture device. As a
  // side effect this will cause the first buffer to be shared with clients.
  uint8_t buffer_no = 1;
  const int arbitrary_frame_feedback_id = 101;
  ASSERT_EQ(0.0, device_client_->GetBufferPoolUtilization());
  media::VideoCaptureDevice::Client::Buffer buffer =
      device_client_->ReserveOutputBuffer(
          device_format.frame_size, device_format.pixel_format,
          device_format.pixel_storage, arbitrary_frame_feedback_id);
  ASSERT_TRUE(buffer.is_valid());
  auto buffer_access = buffer.handle_provider()->GetHandleForInProcessAccess();
  ASSERT_EQ(1.0 / kPoolSize, device_client_->GetBufferPoolUtilization());
  memset(buffer_access->data(), buffer_no++, buffer_access->mapped_size());
  {
    InSequence s;
    EXPECT_CALL(*client_a_, DoBufferCreated(client_a_route_1)).Times(1);
    EXPECT_CALL(*client_a_,
                DoBufferReady(client_a_route_1, device_format.frame_size))
        .Times(1);
  }
  {
    InSequence s;
    EXPECT_CALL(*client_b_, DoBufferCreated(client_b_route_1)).Times(1);
    EXPECT_CALL(*client_b_,
                DoBufferReady(client_b_route_1, device_format.frame_size))
        .Times(1);
  }
  {
    InSequence s;
    EXPECT_CALL(*client_a_, DoBufferCreated(client_a_route_2)).Times(1);
    EXPECT_CALL(*client_a_,
                DoBufferReady(client_a_route_2, device_format.frame_size))
        .Times(1);
  }
  client_a_->resource_utilization_ = 0.5;
  client_b_->resource_utilization_ = -1.0;
  {
    InSequence s;
    EXPECT_CALL(*mock_frame_receiver_observer_, SetBufferHold(buffer.id()))
        .Times(1);
    // Expect VideoCaptureController to call the load observer with a
    // resource utilization of 0.5 (the largest of all reported values).
    EXPECT_CALL(*mock_consumer_feedback_observer_,
                OnUtilizationReport(arbitrary_frame_feedback_id, 0.5))
        .Times(1);
    EXPECT_CALL(*mock_frame_receiver_observer_, ReleaseBufferHold(buffer.id()))
        .Times(1);
  }

  device_client_->OnIncomingCapturedBuffer(std::move(buffer), device_format,
                                           arbitrary_reference_time_,
                                           arbitrary_timestamp_);

  base::RunLoop().RunUntilIdle();
  Mock::VerifyAndClearExpectations(client_a_.get());
  Mock::VerifyAndClearExpectations(client_b_.get());
  Mock::VerifyAndClearExpectations(mock_consumer_feedback_observer_);
  Mock::VerifyAndClearExpectations(mock_frame_receiver_observer_);

  // Second buffer which ought to use the same shared memory buffer. In this
  // case pretend that the Buffer pointer is held by the device for a long
  // delay. This shouldn't affect anything.
  const int arbitrary_frame_feedback_id_2 = 102;
  media::VideoCaptureDevice::Client::Buffer buffer2 =
      device_client_->ReserveOutputBuffer(
          device_format.frame_size, device_format.pixel_format,
          device_format.pixel_storage, arbitrary_frame_feedback_id_2);
  ASSERT_TRUE(buffer2.is_valid());
  auto buffer2_access =
      buffer2.handle_provider()->GetHandleForInProcessAccess();
  memset(buffer2_access->data(), buffer_no++, buffer2_access->mapped_size());
  client_a_->resource_utilization_ = 0.5;
  client_b_->resource_utilization_ = 3.14;
  // Expect VideoCaptureController to call the load observer with a
  // resource utilization of 3.14 (the largest of all reported values).
  {
    InSequence s;
    EXPECT_CALL(*mock_frame_receiver_observer_, SetBufferHold(buffer2.id()))
        .Times(1);
    // Expect VideoCaptureController to call the load observer with a
    // resource utilization of 3.14 (the largest of all reported values).
    EXPECT_CALL(*mock_consumer_feedback_observer_,
                OnUtilizationReport(arbitrary_frame_feedback_id_2, 3.14))
        .Times(1);
    EXPECT_CALL(*mock_frame_receiver_observer_, ReleaseBufferHold(buffer2.id()))
        .Times(1);
  }

  device_client_->OnIncomingCapturedBuffer(std::move(buffer2), device_format,
                                           arbitrary_reference_time_,
                                           arbitrary_timestamp_);

  // The buffer should be delivered to the clients in any order.
  {
    InSequence s;
    EXPECT_CALL(*client_a_, DoBufferCreated(client_a_route_1)).Times(1);
    EXPECT_CALL(*client_a_,
                DoBufferReady(client_a_route_1, device_format.frame_size))
        .Times(1);
  }
  {
    InSequence s;
    EXPECT_CALL(*client_b_, DoBufferCreated(client_b_route_1)).Times(1);
    EXPECT_CALL(*client_b_,
                DoBufferReady(client_b_route_1, device_format.frame_size))
        .Times(1);
  }
  {
    InSequence s;
    EXPECT_CALL(*client_a_, DoBufferCreated(client_a_route_2)).Times(1);
    EXPECT_CALL(*client_a_,
                DoBufferReady(client_a_route_2, device_format.frame_size))
        .Times(1);
  }
  base::RunLoop().RunUntilIdle();
  Mock::VerifyAndClearExpectations(client_a_.get());
  Mock::VerifyAndClearExpectations(client_b_.get());
  Mock::VerifyAndClearExpectations(mock_consumer_feedback_observer_);
  Mock::VerifyAndClearExpectations(mock_frame_receiver_observer_);

  // Add a fourth client now that some buffers have come through.
  controller_->AddClient(client_b_route_2, client_b_.get(), 1, session_1);
  Mock::VerifyAndClearExpectations(client_b_.get());

  // Third, fourth, and fifth buffers. Pretend they all arrive at the same time.
  for (int i = 0; i < kPoolSize; i++) {
    const int arbitrary_frame_feedback_id = 200 + i;
    media::VideoCaptureDevice::Client::Buffer buffer =
        device_client_->ReserveOutputBuffer(
            device_format.frame_size, device_format.pixel_format,
            device_format.pixel_storage, arbitrary_frame_feedback_id);
    ASSERT_TRUE(buffer.is_valid());
    auto buffer_access =
        buffer.handle_provider()->GetHandleForInProcessAccess();
    memset(buffer_access->data(), buffer_no++, buffer_access->mapped_size());
    device_client_->OnIncomingCapturedBuffer(std::move(buffer), device_format,
                                             arbitrary_reference_time_,
                                             arbitrary_timestamp_);
  }
  // ReserveOutputBuffer ought to fail now, because the pool is depleted.
  ASSERT_FALSE(device_client_
                   ->ReserveOutputBuffer(
                       device_format.frame_size, device_format.pixel_format,
                       device_format.pixel_storage, arbitrary_frame_feedback_id)
                   .is_valid());

  // The new client needs to be notified of the creation of |kPoolSize| buffers;
  // the old clients only |kPoolSize - 2|.
  EXPECT_CALL(*client_b_, DoBufferCreated(client_b_route_2)).Times(kPoolSize);
  EXPECT_CALL(*client_b_,
              DoBufferReady(client_b_route_2, device_format.frame_size))
      .Times(kPoolSize);
  EXPECT_CALL(*client_a_, DoBufferCreated(client_a_route_1))
      .Times(kPoolSize - 2);
  EXPECT_CALL(*client_a_,
              DoBufferReady(client_a_route_1, device_format.frame_size))
      .Times(kPoolSize);
  EXPECT_CALL(*client_a_, DoBufferCreated(client_a_route_2))
      .Times(kPoolSize - 2);
  EXPECT_CALL(*client_a_,
              DoBufferReady(client_a_route_2, device_format.frame_size))
      .Times(kPoolSize);
  EXPECT_CALL(*client_b_, DoBufferCreated(client_b_route_1))
      .Times(kPoolSize - 2);
  EXPECT_CALL(*client_b_,
              DoBufferReady(client_b_route_1, device_format.frame_size))
      .Times(kPoolSize);
  base::RunLoop().RunUntilIdle();
  Mock::VerifyAndClearExpectations(client_a_.get());
  Mock::VerifyAndClearExpectations(client_b_.get());

  // Now test the interaction of client shutdown and buffer delivery.
  // Kill A1 via renderer disconnect (synchronous).
  controller_->RemoveClient(client_a_route_1, client_a_.get());
  // Kill B1 via session close (posts a task to disconnect).
  EXPECT_CALL(*client_b_, DoEnded(client_b_route_1)).Times(1);
  controller_->StopSession(300);
  // Queue up another buffer.
  media::VideoCaptureDevice::Client::Buffer buffer3 =
      device_client_->ReserveOutputBuffer(
          device_format.frame_size, device_format.pixel_format,
          device_format.pixel_storage, arbitrary_frame_feedback_id);
  ASSERT_TRUE(buffer3.is_valid());
  auto buffer3_access =
      buffer3.handle_provider()->GetHandleForInProcessAccess();
  memset(buffer3_access->data(), buffer_no++, buffer3_access->mapped_size());
  device_client_->OnIncomingCapturedBuffer(std::move(buffer3), device_format,
                                           arbitrary_reference_time_,
                                           arbitrary_timestamp_);

  media::VideoCaptureDevice::Client::Buffer buffer4 =
      device_client_->ReserveOutputBuffer(
          device_format.frame_size, device_format.pixel_format,
          device_format.pixel_storage, arbitrary_frame_feedback_id);
  {
    // Kill A2 via session close (posts a task to disconnect, but A2 must not
    // be sent either of these two buffers).
    EXPECT_CALL(*client_a_, DoEnded(client_a_route_2)).Times(1);
    controller_->StopSession(200);
  }
  ASSERT_TRUE(buffer4.is_valid());
  auto buffer4_access =
      buffer4.handle_provider()->GetHandleForInProcessAccess();
  memset(buffer4_access->data(), buffer_no++, buffer4_access->mapped_size());
  device_client_->OnIncomingCapturedBuffer(std::move(buffer4), device_format,
                                           arbitrary_reference_time_,
                                           arbitrary_timestamp_);
  // B2 is the only client left, and is the only one that should
  // get the buffer.
  EXPECT_CALL(*client_b_,
              DoBufferReady(client_b_route_2, device_format.frame_size))
      .Times(2);
  base::RunLoop().RunUntilIdle();
  Mock::VerifyAndClearExpectations(client_a_.get());
  Mock::VerifyAndClearExpectations(client_b_.get());
}

INSTANTIATE_TEST_CASE_P(,
                        VideoCaptureControllerTest,
                        ::testing::Values(media::PIXEL_FORMAT_I420,
                                          media::PIXEL_FORMAT_Y16));

// Exercises the OnError() codepath of VideoCaptureController, and tests the
// behavior of various operations after the error state has been signalled.
TEST_F(VideoCaptureControllerTest, ErrorBeforeDeviceCreation) {
  media::VideoCaptureParams session_100;
  session_100.requested_format = media::VideoCaptureFormat(
      gfx::Size(320, 240), 30, media::PIXEL_FORMAT_I420);

  media::VideoCaptureParams session_200 = session_100;

  const gfx::Size capture_resolution(320, 240);

  const VideoCaptureControllerID route_id(0x99);

  // Start with one client.
  controller_->AddClient(route_id, client_a_.get(), 100, session_100);
  device_client_->OnError(FROM_HERE, "Test Error");
  EXPECT_CALL(*client_a_, DoError(route_id)).Times(1);
  base::RunLoop().RunUntilIdle();
  Mock::VerifyAndClearExpectations(client_a_.get());

  // Second client connects after the error state. It also should get told of
  // the error.
  EXPECT_CALL(*client_b_, DoError(route_id)).Times(1);
  controller_->AddClient(route_id, client_b_.get(), 200, session_200);
  base::RunLoop().RunUntilIdle();
  Mock::VerifyAndClearExpectations(client_b_.get());

  media::VideoCaptureFormat device_format(
      capture_resolution, arbitrary_frame_rate_, media::PIXEL_FORMAT_I420,
      media::PIXEL_STORAGE_CPU);
  const int arbitrary_frame_feedback_id = 101;
  media::VideoCaptureDevice::Client::Buffer buffer =
      device_client_->ReserveOutputBuffer(
          device_format.frame_size, device_format.pixel_format,
          device_format.pixel_storage, arbitrary_frame_feedback_id);
  device_client_->OnIncomingCapturedBuffer(std::move(buffer), device_format,
                                           arbitrary_reference_time_,
                                           arbitrary_timestamp_);

  base::RunLoop().RunUntilIdle();
}

// Exercises the OnError() codepath of VideoCaptureController, and tests the
// behavior of various operations after the error state has been signalled.
TEST_F(VideoCaptureControllerTest, ErrorAfterDeviceCreation) {
  media::VideoCaptureParams session_100;
  session_100.requested_format = media::VideoCaptureFormat(
      gfx::Size(320, 240), 30, media::PIXEL_FORMAT_I420);

  media::VideoCaptureParams session_200 = session_100;

  const VideoCaptureControllerID route_id(0x99);

  // Start with one client.
  controller_->AddClient(route_id, client_a_.get(), 100, session_100);

  // Start the device. Then, before the first buffer, signal an error and
  // deliver the buffer. The error should be propagated to clients; the buffer
  // should not be.
  base::RunLoop().RunUntilIdle();
  Mock::VerifyAndClearExpectations(client_a_.get());

  media::VideoCaptureFormat device_format(
      gfx::Size(10, 10), arbitrary_frame_rate_, media::PIXEL_FORMAT_I420);
  const int arbitrary_frame_feedback_id = 101;
  media::VideoCaptureDevice::Client::Buffer buffer =
      device_client_->ReserveOutputBuffer(
          device_format.frame_size, device_format.pixel_format,
          device_format.pixel_storage, arbitrary_frame_feedback_id);
  ASSERT_TRUE(buffer.is_valid());

  device_client_->OnError(FROM_HERE, "Test Error");
  device_client_->OnIncomingCapturedBuffer(std::move(buffer), device_format,
                                           arbitrary_reference_time_,
                                           arbitrary_timestamp_);

  EXPECT_CALL(*client_a_, DoError(route_id)).Times(1);
  base::RunLoop().RunUntilIdle();
  Mock::VerifyAndClearExpectations(client_a_.get());

  // Second client connects after the error state. It also should get told of
  // the error.
  EXPECT_CALL(*client_b_, DoError(route_id)).Times(1);
  controller_->AddClient(route_id, client_b_.get(), 200, session_200);
  Mock::VerifyAndClearExpectations(client_b_.get());
}

// Tests that frame feedback provided by consumers is correctly reported back
// to the producing device for a sequence of frames that is longer than the
// number of buffers shared between the device and consumer.
TEST_F(VideoCaptureControllerTest, FrameFeedbackIsReportedForSequenceOfFrames) {
  const int kTestFrameSequenceLength = 10;
  media::VideoCaptureFormat arbitrary_format(
      gfx::Size(320, 240), arbitrary_frame_rate_, media::PIXEL_FORMAT_I420);

  // Register |client_a_| at |controller_|.
  media::VideoCaptureParams session_100;
  session_100.requested_format = arbitrary_format;
  const VideoCaptureControllerID route_id(0x99);
  controller_->AddClient(route_id, client_a_.get(), 100, session_100);
  base::RunLoop().RunUntilIdle();
  Mock::VerifyAndClearExpectations(client_a_.get());

  for (int frame_index = 0; frame_index < kTestFrameSequenceLength;
       frame_index++) {
    const int stub_frame_feedback_id = frame_index;
    const float stub_consumer_utilization =
        static_cast<float>(frame_index) / kTestFrameSequenceLength;

    client_a_->resource_utilization_ = stub_consumer_utilization;

    EXPECT_CALL(*client_a_,
                DoBufferReady(route_id, arbitrary_format.frame_size))
        .Times(1);
    EXPECT_CALL(
        *mock_consumer_feedback_observer_,
        OnUtilizationReport(stub_frame_feedback_id, stub_consumer_utilization))
        .Times(1);

    // Device prepares and pushes a frame.
    // For the first half of the frames we exercise ReserveOutputBuffer() while
    // for the second half we exercise ResurrectLastOutputBuffer().
    // The frame is expected to arrive at |client_a_|.DoBufferReady(), which
    // automatically notifies |controller_| that it has finished consuming it.
    media::VideoCaptureDevice::Client::Buffer buffer;
    if (frame_index < kTestFrameSequenceLength / 2) {
      buffer = device_client_->ReserveOutputBuffer(
          arbitrary_format.frame_size, arbitrary_format.pixel_format,
          arbitrary_format.pixel_storage, stub_frame_feedback_id);
    } else {
      buffer = device_client_->ResurrectLastOutputBuffer(
          arbitrary_format.frame_size, arbitrary_format.pixel_format,
          arbitrary_format.pixel_storage, stub_frame_feedback_id);
    }
    ASSERT_TRUE(buffer.is_valid());
    device_client_->OnIncomingCapturedBuffer(
        std::move(buffer), arbitrary_format, arbitrary_reference_time_,
        arbitrary_timestamp_);

    base::RunLoop().RunUntilIdle();
    Mock::VerifyAndClearExpectations(client_a_.get());
    Mock::VerifyAndClearExpectations(mock_consumer_feedback_observer_);
  }
}

}  // namespace content
