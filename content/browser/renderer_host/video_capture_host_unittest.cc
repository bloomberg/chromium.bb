// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <list>
#include <string>

#include "base/file_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/process_util.h"
#include "base/stringprintf.h"
#include "content/browser/browser_thread.h"
#include "content/browser/media_stream/video_capture_manager.h"
#include "content/browser/renderer_host/video_capture_host.h"
#include "content/common/video_capture_messages.h"
#include "media/video/capture/video_capture_types.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::AtLeast;
using ::testing::AnyNumber;
using ::testing::DoAll;
using ::testing::InSequence;
using ::testing::Mock;
using ::testing::Return;

// IPC::Msg.routing_id.
static const int32 kRouteId = 200;
// Id used to identify the capture session between renderer and
// video_capture_host.
static const int kDeviceId = 1;
// Id of a video capture device
static const media::VideoCaptureSessionId kTestFakeDeviceId =
    media_stream::VideoCaptureManager::kStartOpenSessionId;

// Define to enable test where video is dumped to file.
// #define DUMP_VIDEO

// Define to use a real video capture device.
// #define TEST_REAL_CAPTURE_DEVICE

// Simple class used for dumping video to a file. This can be used for
// verifying the output.
class DumpVideo {
 public:
  DumpVideo() : expected_size_(0) {}
  void StartDump(int width, int height) {
    // Create an FilePath that works on all platforms. There should
    // be a better way.
    FilePath file_name =
        FilePath::FromWStringHack(StringPrintf(L"dump_w%d_h%d.yuv", width,
                                               height));
    file_.reset(file_util::OpenFile(file_name, "wb"));
    expected_size_ = width * height * 3 / 2;
  }
  void NewVideoFrame(const void* buffer) {
    if (file_.get() != NULL) {
      fwrite(buffer, expected_size_, 1, file_.get());
    }
  }

 private:
  file_util::ScopedFILE file_;
  int expected_size_;
};

class MockVideoCaptureHost : public VideoCaptureHost {
 public:
  MockVideoCaptureHost() : return_buffers_(false), dump_video_(false) {}
  virtual ~MockVideoCaptureHost() {}

  // A list of mock methods.
  MOCK_METHOD3(OnBufferFilled,
               void(int routing_id, int device_id, TransportDIB::Handle));
  MOCK_METHOD3(OnStateChanged,
               void(int routing_id, int device_id,
                    media::VideoCapture::State state));
  MOCK_METHOD2(OnDeviceInfo, void(int routing_id, int device_id));

  // Use class DumpVideo to write I420 video to file.
  void SetDumpVideo(bool enable) {
    dump_video_ = enable;
  }
  void SetReturnReceviedDibs(bool enable) {
    return_buffers_ = enable;
  }

  // Return Dibs we currently have received.
  void ReturnReceivedDibs(int device_id)  {
    TransportDIB::Handle handle = GetReceivedDib();
    while (TransportDIB::is_valid_handle(handle)) {
      IPC::Message msg;
      msg.set_routing_id(kRouteId);
      this->OnReceiveEmptyBuffer(msg, device_id, handle);
      handle = GetReceivedDib();
    }
  }
  TransportDIB::Handle GetReceivedDib() {
    if (filled_dib_.empty())
      return TransportDIB::DefaultHandleValue();
    TransportDIB::Handle h = filled_dib_.front();
    filled_dib_.pop_front();

    return h;
  }

 private:
  // This method is used to dispatch IPC messages to the renderer. We intercept
  // these messages here and dispatch to our mock methods to verify the
  // conversation between this object and the renderer.
  virtual bool Send(IPC::Message* message) {
    CHECK(message);

    // In this method we dispatch the messages to the according handlers as if
    // we are the renderer.
    bool handled = true;
    IPC_BEGIN_MESSAGE_MAP(MockVideoCaptureHost, *message)
      IPC_MESSAGE_HANDLER(VideoCaptureMsg_BufferReady, OnBufferFilled)
      IPC_MESSAGE_HANDLER(VideoCaptureMsg_StateChanged, OnStateChanged)
      IPC_MESSAGE_HANDLER(VideoCaptureMsg_DeviceInfo, OnDeviceInfo)
      IPC_MESSAGE_UNHANDLED(handled = false)
    IPC_END_MESSAGE_MAP()
    EXPECT_TRUE(handled);

    delete message;
    return true;
  }

  // These handler methods do minimal things and delegate to the mock methods.
  void OnBufferFilled(const IPC::Message& msg, int device_id,
                      TransportDIB::Handle  dib_handle) {
    if (dump_video_) {
      TransportDIB* dib = TransportDIB::Map(dib_handle);
      ASSERT_TRUE(dib != NULL);
      dumper_.NewVideoFrame(dib->memory());
    }

    OnBufferFilled(msg.routing_id(), device_id, dib_handle);
    if (return_buffers_) {
      VideoCaptureHost::OnReceiveEmptyBuffer(msg, device_id, dib_handle);
    } else {
      filled_dib_.push_back(dib_handle);
    }
  }

  void OnStateChanged(const IPC::Message& msg, int device_id,
                      media::VideoCapture::State state) {
    OnStateChanged(msg.routing_id(), device_id, state);
  }

  void OnDeviceInfo(const IPC::Message& msg, int device_id,
                      media::VideoCaptureParams params) {
    if (dump_video_) {
      dumper_.StartDump(params.width, params.height);
    }
    OnDeviceInfo(msg.routing_id(), device_id);
  }

  std::list<TransportDIB::Handle> filled_dib_;
  bool return_buffers_;
  bool dump_video_;
  DumpVideo dumper_;
};

ACTION_P(ExitMessageLoop, message_loop) {
  message_loop->PostTask(FROM_HERE, new MessageLoop::QuitTask());
}

class VideoCaptureHostTest : public testing::Test {
 public:
  VideoCaptureHostTest() {}

 protected:
  virtual void SetUp() {
    // Setup the VideoCaptureManager to use fake video capture device.
#ifndef TEST_REAL_CAPTURE_DEVICE
    media_stream::VideoCaptureManager* manager =
        media_stream::VideoCaptureManager::Get();
    manager->UseFakeDevice();
#endif
    // Create a message loop so VideoCaptureHostTest can use it.
    message_loop_.reset(new MessageLoop(MessageLoop::TYPE_IO));
    io_thread_.reset(new BrowserThread(BrowserThread::IO, message_loop_.get()));
    host_ = new MockVideoCaptureHost();

    // Simulate IPC channel connected.
    host_->OnChannelConnected(base::GetCurrentProcId());
  }

  virtual void TearDown() {
    // Verifies and removes the expectations on host_ and
    // returns true iff successful.
    Mock::VerifyAndClearExpectations(host_);

    EXPECT_CALL(*host_, OnStateChanged(kRouteId, kDeviceId,
                                       media::VideoCapture::kStopped))
        .Times(AnyNumber());

    // Simulate closing the IPC channel.
    host_->OnChannelClosing();

    // Release the reference to the mock object. The object will be destructed
    // on message_loop_.
    host_ = NULL;

    // We need to continue running message_loop_ to complete all destructions.
    SyncWithVideoCaptureManagerThread();

    io_thread_.reset();
  }

  // Called on the VideoCaptureManager thread.
  static void PostQuitMessageLoop(MessageLoop* message_loop) {
    message_loop->PostTask(FROM_HERE, new MessageLoop::QuitTask());
  }

  // Called on the main thread.
  static void PostQuitOnVideoCaptureManagerThread(MessageLoop* message_loop) {
    media_stream::VideoCaptureManager::Get()->GetMessageLoop()->PostTask(
        FROM_HERE, NewRunnableFunction(&PostQuitMessageLoop, message_loop));
  }

  // SyncWithVideoCaptureManagerThread() waits until all pending tasks on the
  // video_capture_manager thread are executed while also processing pending
  // task in message_loop_ on the current thread. It is used to synchronize
  // with the video capture manager thread when we are stopping a video
  // capture device.
  void SyncWithVideoCaptureManagerThread() {
    message_loop_->PostTask(
        FROM_HERE, NewRunnableFunction(&PostQuitOnVideoCaptureManagerThread,
                                       message_loop_.get()));
    message_loop_->Run();
  }

  void StartCapture() {
    InSequence s;
    // 1. First - get info about the new resolution
    EXPECT_CALL(*host_, OnDeviceInfo(kRouteId, kDeviceId));

    // 2. Change state to started
    EXPECT_CALL(*host_, OnStateChanged(kRouteId, kDeviceId,
                                       media::VideoCapture::kStarted));

    // 3. First filled buffer will arrive.
    EXPECT_CALL(*host_, OnBufferFilled(kRouteId, kDeviceId, _))
        .Times(AnyNumber())
        .WillOnce(ExitMessageLoop(message_loop_.get()));

    IPC::Message msg;
    msg.set_routing_id(kRouteId);

    media::VideoCaptureParams params;
    params.width = 352;
    params.height = 288;
    params.frame_per_second = 30;
    params.session_id = kTestFakeDeviceId;
    host_->OnStartCapture(msg, kDeviceId, params);
    message_loop_->Run();
  }

  void CaptureAndDumpVideo(int width, int heigt, int frame_rate) {
    InSequence s;
    // 1. First - get info about the new resolution
    EXPECT_CALL(*host_, OnDeviceInfo(kRouteId, kDeviceId));

    // 2. Change state to started
    EXPECT_CALL(*host_, OnStateChanged(kRouteId, kDeviceId,
                                       media::VideoCapture::kStarted));

    // 3. First filled buffer will arrive.
    EXPECT_CALL(*host_, OnBufferFilled(kRouteId, kDeviceId, _))
        .Times(AnyNumber())
        .WillOnce(ExitMessageLoop(message_loop_.get()));

    IPC::Message msg;
    msg.set_routing_id(kRouteId);

    media::VideoCaptureParams params;
    params.width = width;
    params.height = heigt;
    params.frame_per_second = frame_rate;
    params.session_id = kTestFakeDeviceId;
    host_->SetDumpVideo(true);
    host_->OnStartCapture(msg, kDeviceId, params);
    message_loop_->Run();
  }

  void StopCapture() {
    EXPECT_CALL(*host_, OnStateChanged(kRouteId, kDeviceId,
                                       media::VideoCapture::kStopped))
        .Times(AtLeast(1));

    IPC::Message msg;
    msg.set_routing_id(kRouteId);
    host_->OnStopCapture(msg, kDeviceId);
    host_->SetReturnReceviedDibs(true);
    host_->ReturnReceivedDibs(kDeviceId);

    SyncWithVideoCaptureManagerThread();
    host_->SetReturnReceviedDibs(false);
    // Expect the VideoCaptureDevice has been stopped
    EXPECT_EQ(0u, host_->entries_.size());
  }

  void NotifyPacketReady() {
    EXPECT_CALL(*host_, OnBufferFilled(kRouteId, kDeviceId, _))
        .Times(AnyNumber())
        .WillOnce(ExitMessageLoop(message_loop_.get()))
        .RetiresOnSaturation();
    message_loop_->Run();
  }

  void ReturnReceivedPackets() {
    host_->ReturnReceivedDibs(kDeviceId);
  }

  void SimulateError() {
    // Expect a change state to error state  sent through IPC.
    EXPECT_CALL(*host_, OnStateChanged(kRouteId, kDeviceId,
                                       media::VideoCapture::kError))
        .Times(1);
    VideoCaptureController::ControllerId id(kRouteId, kDeviceId);
    host_->OnError(id);
    SyncWithVideoCaptureManagerThread();
  }

  scoped_refptr<MockVideoCaptureHost> host_;
 private:
  scoped_ptr<MessageLoop> message_loop_;
  scoped_ptr<BrowserThread> io_thread_;

  DISALLOW_COPY_AND_ASSIGN(VideoCaptureHostTest);
};

TEST_F(VideoCaptureHostTest, StartCapture) {
  StartCapture();
}

TEST_F(VideoCaptureHostTest, StartCapturePlayStop) {
  StartCapture();
  NotifyPacketReady();
  NotifyPacketReady();
  ReturnReceivedPackets();
  StopCapture();
}

TEST_F(VideoCaptureHostTest, StartCaptureErrorStop) {
  StartCapture();
  SimulateError();
  StopCapture();
}

TEST_F(VideoCaptureHostTest, StartCaptureError) {
  EXPECT_CALL(*host_, OnStateChanged(kRouteId, kDeviceId,
                                     media::VideoCapture::kStopped))
      .Times(0);
  StartCapture();
  NotifyPacketReady();
  SimulateError();
  base::PlatformThread::Sleep(200);
}

#ifdef DUMP_VIDEO
TEST_F(VideoCaptureHostTest, CaptureAndDumpVideoVga) {
  CaptureAndDumpVideo(640, 480, 30);
}
TEST_F(VideoCaptureHostTest, CaptureAndDump720P) {
  CaptureAndDumpVideo(1280, 720, 30);
}
#endif
