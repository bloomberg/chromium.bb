// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Unit test for VideoCaptureController.

#include <string>

#include "base/bind.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/process_util.h"
#include "content/browser/browser_thread_impl.h"
#include "content/browser/renderer_host/media/media_stream_provider.h"
#include "content/browser/renderer_host/media/video_capture_controller.h"
#include "content/browser/renderer_host/media/video_capture_manager.h"
#include "content/common/media/media_stream_options.h"
#include "media/video/capture/fake_video_capture_device.h"
#include "media/video/capture/video_capture_device.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::AtLeast;
using ::testing::InSequence;
using ::testing::Return;

namespace content {

enum { kDeviceId = 1 };

ACTION_P4(StopCapture, controller, controller_id, controller_handler,
          message_loop) {
  message_loop->PostTask(FROM_HERE,
      base::Bind(&VideoCaptureController::StopCapture,
                 controller, controller_id, controller_handler));
  message_loop->PostTask(FROM_HERE, MessageLoop::QuitClosure());
}

ACTION_P3(StopSession, controller, session_id, message_loop) {
  message_loop->PostTask(FROM_HERE,
      base::Bind(&VideoCaptureController::StopSession,
                 controller, session_id));
  message_loop->PostTask(FROM_HERE, MessageLoop::QuitClosure());
}

class MockVideoCaptureControllerEventHandler
    : public VideoCaptureControllerEventHandler {
 public:
  MockVideoCaptureControllerEventHandler(VideoCaptureController* controller,
                                         MessageLoop* message_loop)
      : controller_(controller),
        message_loop_(message_loop),
        controller_id_(kDeviceId),
        process_handle_(base::kNullProcessHandle) {
  }
  virtual ~MockVideoCaptureControllerEventHandler() {}

  MOCK_METHOD1(DoBufferCreated, void(const VideoCaptureControllerID&));
  MOCK_METHOD1(DoBufferReady, void(const VideoCaptureControllerID&));
  MOCK_METHOD1(DoFrameInfo, void(const VideoCaptureControllerID&));
  MOCK_METHOD1(DoPaused, void(const VideoCaptureControllerID&));

  virtual void OnError(const VideoCaptureControllerID& id) OVERRIDE {}
  virtual void OnBufferCreated(const VideoCaptureControllerID& id,
                               base::SharedMemoryHandle handle,
                               int length, int buffer_id) OVERRIDE {
    EXPECT_EQ(id, controller_id_);
    DoBufferCreated(id);
  }
  virtual void OnBufferReady(const VideoCaptureControllerID& id,
                             int buffer_id,
                             base::Time timestamp) OVERRIDE {
    EXPECT_EQ(id, controller_id_);
    DoBufferReady(id);
    message_loop_->PostTask(FROM_HERE,
        base::Bind(&VideoCaptureController::ReturnBuffer,
                   controller_, controller_id_, this, buffer_id));
  }
  virtual void OnFrameInfo(const VideoCaptureControllerID& id,
                           int width,
                           int height,
                           int frame_per_second) OVERRIDE {
    EXPECT_EQ(id, controller_id_);
    DoFrameInfo(id);
  }
  virtual void OnPaused(const VideoCaptureControllerID& id) OVERRIDE {
    EXPECT_EQ(id, controller_id_);
    DoPaused(id);
  }

  scoped_refptr<VideoCaptureController> controller_;
  MessageLoop* message_loop_;
  VideoCaptureControllerID controller_id_;
  base::ProcessHandle process_handle_;
};

class MockVideoCaptureManager : public VideoCaptureManager {
 public:
  MockVideoCaptureManager()
      : video_session_id_(kStartOpenSessionId) {}

  void Init() {
    device_name_.unique_id = "/dev/video0";
    device_name_.device_name = "fake_device_0";

    video_capture_device_.reset(
        media::FakeVideoCaptureDevice::Create(device_name_));
    ASSERT_TRUE(video_capture_device_.get() != NULL);
  }

  MOCK_METHOD3(StartCapture, void(int, int,
      media::VideoCaptureDevice::EventHandler*));
  MOCK_METHOD1(StopCapture, void(const media::VideoCaptureSessionId&));

  void Start(const media::VideoCaptureParams& capture_params,
             media::VideoCaptureDevice::EventHandler* vc_receiver) OVERRIDE {
    StartCapture(capture_params.width, capture_params.height, vc_receiver);
    video_capture_device_->Allocate(capture_params.width, capture_params.height,
                                    capture_params.frame_per_second,
                                    vc_receiver);
    video_capture_device_->Start();
  }

  void Stop(const media::VideoCaptureSessionId& capture_session_id,
            base::Closure stopped_cb) OVERRIDE {
    StopCapture(capture_session_id);
    video_capture_device_->Stop();
    video_capture_device_->DeAllocate();
  }

  int video_session_id_;
  media::VideoCaptureDevice::Name device_name_;
  scoped_ptr<media::VideoCaptureDevice> video_capture_device_;

 private:
  virtual ~MockVideoCaptureManager() {}
  DISALLOW_COPY_AND_ASSIGN(MockVideoCaptureManager);
};

// Test class.
class VideoCaptureControllerTest : public testing::Test {
 public:
  VideoCaptureControllerTest() {}
  virtual ~VideoCaptureControllerTest() {}

 protected:
  virtual void SetUp() OVERRIDE {
    message_loop_.reset(new MessageLoop(MessageLoop::TYPE_IO));
    file_thread_.reset(new BrowserThreadImpl(BrowserThread::FILE,
                                             message_loop_.get()));
    io_thread_.reset(new BrowserThreadImpl(BrowserThread::IO,
                                           message_loop_.get()));

    vcm_ = new MockVideoCaptureManager();
    vcm_->Init();
    controller_ = new VideoCaptureController(vcm_);
    controller_handler_.reset(
        new MockVideoCaptureControllerEventHandler(controller_.get(),
                                                   message_loop_.get()));
  }

  virtual void TearDown() OVERRIDE {}

  scoped_ptr<MessageLoop> message_loop_;
  scoped_ptr<BrowserThreadImpl> file_thread_;
  scoped_ptr<BrowserThreadImpl> io_thread_;
  scoped_refptr<MockVideoCaptureManager> vcm_;
  scoped_ptr<MockVideoCaptureControllerEventHandler> controller_handler_;
  scoped_refptr<VideoCaptureController> controller_;

 private:
  DISALLOW_COPY_AND_ASSIGN(VideoCaptureControllerTest);
};

// Try to start and stop capture.
TEST_F(VideoCaptureControllerTest, StartAndStop) {
  media::VideoCaptureParams capture_params;
  capture_params.session_id = vcm_->video_session_id_;
  capture_params.width = 320;
  capture_params.height = 240;
  capture_params.frame_per_second = 30;

  InSequence s;
  EXPECT_CALL(*vcm_,
              StartCapture(capture_params.width,
                           capture_params.height,
                           controller_.get()))
      .Times(1);
  EXPECT_CALL(*controller_handler_,
              DoFrameInfo(controller_handler_->controller_id_))
      .Times(AtLeast(1));
  EXPECT_CALL(*controller_handler_,
              DoBufferCreated(controller_handler_->controller_id_))
      .Times(AtLeast(1));
  EXPECT_CALL(*controller_handler_,
              DoBufferReady(controller_handler_->controller_id_))
      .Times(AtLeast(1))
      .WillOnce(StopCapture(controller_.get(),
                            controller_handler_->controller_id_,
                            controller_handler_.get(),
                            message_loop_.get()));
  EXPECT_CALL(*vcm_,
              StopCapture(vcm_->video_session_id_))
      .Times(1);

  controller_->StartCapture(controller_handler_->controller_id_,
                            controller_handler_.get(),
                            controller_handler_->process_handle_,
                            capture_params);
  message_loop_->Run();
}

// Try to stop session before stopping capture.
TEST_F(VideoCaptureControllerTest, StopSession) {
  media::VideoCaptureParams capture_params;
  capture_params.session_id = vcm_->video_session_id_;
  capture_params.width = 320;
  capture_params.height = 240;
  capture_params.frame_per_second = 30;

  InSequence s;
  EXPECT_CALL(*vcm_,
              StartCapture(capture_params.width,
                           capture_params.height,
                           controller_.get()))
      .Times(1);
  EXPECT_CALL(*controller_handler_,
              DoFrameInfo(controller_handler_->controller_id_))
      .Times(AtLeast(1));
  EXPECT_CALL(*controller_handler_,
              DoBufferCreated(controller_handler_->controller_id_))
      .Times(AtLeast(1));
  EXPECT_CALL(*controller_handler_,
              DoBufferReady(controller_handler_->controller_id_))
      .Times(AtLeast(1))
      .WillOnce(StopSession(controller_.get(),
                            vcm_->video_session_id_,
                            message_loop_.get()));
  EXPECT_CALL(*controller_handler_,
              DoPaused(controller_handler_->controller_id_))
      .Times(1);

  controller_->StartCapture(controller_handler_->controller_id_,
                            controller_handler_.get(),
                            controller_handler_->process_handle_,
                            capture_params);
  message_loop_->Run();

  // The session is stopped now. There should be no buffer coming from
  // controller.
  EXPECT_CALL(*controller_handler_,
              DoBufferReady(controller_handler_->controller_id_))
      .Times(0);
  message_loop_->PostDelayedTask(
      FROM_HERE, MessageLoop::QuitClosure(), base::TimeDelta::FromSeconds(1));
  message_loop_->Run();

  EXPECT_CALL(*vcm_,
              StopCapture(vcm_->video_session_id_))
      .Times(1);
  controller_->StopCapture(controller_handler_->controller_id_,
                           controller_handler_.get());
}

}  // namespace content
