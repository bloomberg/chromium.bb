// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "content/child/child_process.h"
#include "content/renderer/media/video_capture_impl.h"
#include "content/renderer/media/video_capture_impl_manager.h"
#include "content/renderer/media/video_capture_message_filter.h"
#include "media/base/bind_to_current_loop.h"
#include "media/video/capture/mock_video_capture_event_handler.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::DoAll;
using ::testing::SaveArg;
using media::BindToCurrentLoop;
using media::MockVideoCaptureEventHandler;

namespace content {

ACTION_P(RunClosure, closure) {
  closure.Run();
}

class MockVideoCaptureImpl : public VideoCaptureImpl {
 public:
  MockVideoCaptureImpl(media::VideoCaptureSessionId session_id,
                       VideoCaptureMessageFilter* filter,
                       base::Closure destruct_callback)
      : VideoCaptureImpl(session_id, filter),
        destruct_callback_(destruct_callback) {
  }

  virtual ~MockVideoCaptureImpl() {
    destruct_callback_.Run();
  }

 private:
  base::Closure destruct_callback_;

  DISALLOW_COPY_AND_ASSIGN(MockVideoCaptureImpl);
};

class MockVideoCaptureImplManager : public VideoCaptureImplManager {
 public:
  explicit MockVideoCaptureImplManager(
      base::Closure destruct_video_capture_callback)
      : destruct_video_capture_callback_(
          destruct_video_capture_callback) {}

 protected:
  virtual VideoCaptureImpl* CreateVideoCaptureImpl(
      media::VideoCaptureSessionId id,
      VideoCaptureMessageFilter* filter) const OVERRIDE {
    return new MockVideoCaptureImpl(id,
                                    filter,
                                    destruct_video_capture_callback_);
  }

 private:
  base::Closure destruct_video_capture_callback_;

  DISALLOW_COPY_AND_ASSIGN(MockVideoCaptureImplManager);
};

class VideoCaptureImplManagerTest : public ::testing::Test {
 public:
  VideoCaptureImplManagerTest()
      : manager_(BindToCurrentLoop(cleanup_run_loop_.QuitClosure())) {
    params_.requested_format = media::VideoCaptureFormat(
        gfx::Size(176, 144), 30, media::PIXEL_FORMAT_I420);
    child_process_.reset(new ChildProcess());
  }

  void FakeChannelSetup() {
    scoped_refptr<base::MessageLoopProxy> loop =
        child_process_->io_message_loop_proxy();
    if (!loop->BelongsToCurrentThread()) {
      loop->PostTask(
          FROM_HERE,
          base::Bind(
              &VideoCaptureImplManagerTest::FakeChannelSetup,
              base::Unretained(this)));
      return;
    }
    manager_.video_capture_message_filter()->OnFilterAdded(NULL);
  }

 protected:
  base::MessageLoop message_loop_;
  scoped_ptr<ChildProcess> child_process_;
  media::VideoCaptureParams params_;
  base::RunLoop cleanup_run_loop_;
  MockVideoCaptureImplManager manager_;

 private:
  DISALLOW_COPY_AND_ASSIGN(VideoCaptureImplManagerTest);
};

// Multiple clients with the same session id. There is only one
// media::VideoCapture object.
TEST_F(VideoCaptureImplManagerTest, MultipleClients) {
  scoped_ptr<MockVideoCaptureEventHandler> client1(
      new MockVideoCaptureEventHandler);
  scoped_ptr<MockVideoCaptureEventHandler> client2(
      new MockVideoCaptureEventHandler);

  media::VideoCapture* device1 = NULL;
  media::VideoCapture* device2 = NULL;

  scoped_ptr<VideoCaptureHandle> handle1;
  scoped_ptr<VideoCaptureHandle> handle2;
  {
    base::RunLoop run_loop;
    base::Closure quit_closure = BindToCurrentLoop(
        run_loop.QuitClosure());

    EXPECT_CALL(*client1, OnStarted(_)).WillOnce(SaveArg<0>(&device1));
    EXPECT_CALL(*client2, OnStarted(_)).WillOnce(
        DoAll(
            SaveArg<0>(&device2),
            RunClosure(quit_closure)));
    handle1 = manager_.UseDevice(1);
    handle2 = manager_.UseDevice(1);
    handle1->StartCapture(client1.get(), params_);
    handle2->StartCapture(client2.get(), params_);
    FakeChannelSetup();
    run_loop.Run();
  }

  {
    base::RunLoop run_loop;
    base::Closure quit_closure = BindToCurrentLoop(
        run_loop.QuitClosure());

    EXPECT_CALL(*client1, OnStopped(_));
    EXPECT_CALL(*client1, OnRemoved(_));
    EXPECT_CALL(*client2, OnStopped(_));
    EXPECT_CALL(*client2, OnRemoved(_)).WillOnce(
        RunClosure(quit_closure));
    handle1->StopCapture(client1.get());
    handle2->StopCapture(client2.get());
    run_loop.Run();
  }
  EXPECT_TRUE(device1 == device2);

  handle1.reset();
  handle2.reset();
  cleanup_run_loop_.Run();
}

}  // namespace content
