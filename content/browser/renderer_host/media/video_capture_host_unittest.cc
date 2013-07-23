// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <string>

#include "base/bind.h"
#include "base/file_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "content/browser/browser_thread_impl.h"
#include "content/browser/renderer_host/media/media_stream_manager.h"
#include "content/browser/renderer_host/media/video_capture_host.h"
#include "content/browser/renderer_host/media/video_capture_manager.h"
#include "content/common/media/video_capture_messages.h"
#include "content/public/test/mock_resource_context.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "media/audio/audio_manager.h"
#include "media/video/capture/video_capture_types.h"
#include "net/url_request/url_request_context.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::AtLeast;
using ::testing::AnyNumber;
using ::testing::DoAll;
using ::testing::InSequence;
using ::testing::Mock;
using ::testing::Return;

namespace content {

// Id used to identify the capture session between renderer and
// video_capture_host.
static const int kDeviceId = 1;
// Id of a video capture device
static const media::VideoCaptureSessionId kTestFakeDeviceId =
    VideoCaptureManager::kStartOpenSessionId;

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
    base::FilePath file_name = base::FilePath(base::StringPrintf(
        FILE_PATH_LITERAL("dump_w%d_h%d.yuv"), width, height));
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
  MockVideoCaptureHost(MediaStreamManager* manager)
      : VideoCaptureHost(manager),
        return_buffers_(false),
        dump_video_(false) {}

  // A list of mock methods.
  MOCK_METHOD4(OnNewBufferCreated,
               void(int device_id, base::SharedMemoryHandle handle,
                    int length, int buffer_id));
  MOCK_METHOD3(OnBufferFilled,
               void(int device_id, int buffer_id, base::Time timestamp));
  MOCK_METHOD2(OnStateChanged, void(int device_id, VideoCaptureState state));
  MOCK_METHOD1(OnDeviceInfo, void(int device_id));

  // Use class DumpVideo to write I420 video to file.
  void SetDumpVideo(bool enable) {
    dump_video_ = enable;
  }

  void SetReturnReceviedDibs(bool enable) {
    return_buffers_ = enable;
  }

  // Return Dibs we currently have received.
  void ReturnReceivedDibs(int device_id)  {
    int handle = GetReceivedDib();
    while (handle) {
      this->OnReceiveEmptyBuffer(device_id, handle);
      handle = GetReceivedDib();
    }
  }

  int GetReceivedDib() {
    if (filled_dib_.empty())
      return 0;
    std::map<int, base::SharedMemory*>::iterator it = filled_dib_.begin();
    int h = it->first;
    delete it->second;
    filled_dib_.erase(it);

    return h;
  }

 private:
  virtual ~MockVideoCaptureHost() {
    STLDeleteContainerPairSecondPointers(filled_dib_.begin(),
                                         filled_dib_.end());
  }

  // This method is used to dispatch IPC messages to the renderer. We intercept
  // these messages here and dispatch to our mock methods to verify the
  // conversation between this object and the renderer.
  virtual bool Send(IPC::Message* message) OVERRIDE {
    CHECK(message);

    // In this method we dispatch the messages to the according handlers as if
    // we are the renderer.
    bool handled = true;
    IPC_BEGIN_MESSAGE_MAP(MockVideoCaptureHost, *message)
      IPC_MESSAGE_HANDLER(VideoCaptureMsg_NewBuffer, OnNewBufferCreatedDispatch)
      IPC_MESSAGE_HANDLER(VideoCaptureMsg_BufferReady, OnBufferFilledDispatch)
      IPC_MESSAGE_HANDLER(VideoCaptureMsg_StateChanged, OnStateChangedDispatch)
      IPC_MESSAGE_HANDLER(VideoCaptureMsg_DeviceInfo, OnDeviceInfoDispatch)
      IPC_MESSAGE_UNHANDLED(handled = false)
    IPC_END_MESSAGE_MAP()
    EXPECT_TRUE(handled);

    delete message;
    return true;
  }

  // These handler methods do minimal things and delegate to the mock methods.
  void OnNewBufferCreatedDispatch(int device_id,
                                  base::SharedMemoryHandle handle,
                                  int length, int buffer_id) {
    OnNewBufferCreated(device_id, handle, length, buffer_id);
    base::SharedMemory* dib = new base::SharedMemory(handle, false);
    dib->Map(length);
    filled_dib_[buffer_id] = dib;
  }

  void OnBufferFilledDispatch(int device_id, int buffer_id,
                              base::Time timestamp) {
    if (dump_video_) {
      base::SharedMemory* dib = filled_dib_[buffer_id];
      ASSERT_TRUE(dib != NULL);
      dumper_.NewVideoFrame(dib->memory());
    }

    OnBufferFilled(device_id, buffer_id, timestamp);
    if (return_buffers_) {
      VideoCaptureHost::OnReceiveEmptyBuffer(device_id, buffer_id);
    }
  }

  void OnStateChangedDispatch(int device_id, VideoCaptureState state) {
    OnStateChanged(device_id, state);
  }

  void OnDeviceInfoDispatch(int device_id,
                            media::VideoCaptureParams params) {
    if (dump_video_) {
      dumper_.StartDump(params.width, params.height);
    }
    OnDeviceInfo(device_id);
  }

  std::map<int, base::SharedMemory*> filled_dib_;
  bool return_buffers_;
  bool dump_video_;
  DumpVideo dumper_;
};

ACTION_P2(ExitMessageLoop, message_loop, quit_closure) {
  message_loop->PostTask(FROM_HERE, quit_closure);
}

class VideoCaptureHostTest : public testing::Test {
 public:
  VideoCaptureHostTest()
      : thread_bundle_(content::TestBrowserThreadBundle::IO_MAINLOOP),
        message_loop_(base::MessageLoopProxy::current()) {
    // Create our own MediaStreamManager.
    audio_manager_.reset(media::AudioManager::Create());
    media_stream_manager_.reset(new MediaStreamManager(audio_manager_.get()));
#ifndef TEST_REAL_CAPTURE_DEVICE
    media_stream_manager_->UseFakeDevice();
#endif

    host_ = new MockVideoCaptureHost(media_stream_manager_.get());

    // Simulate IPC channel connected.
    host_->OnChannelConnected(base::GetCurrentProcId());
  }

  virtual ~VideoCaptureHostTest() {
    // Verifies and removes the expectations on host_ and
    // returns true iff successful.
    Mock::VerifyAndClearExpectations(host_.get());

    EXPECT_CALL(*host_.get(),
                OnStateChanged(kDeviceId, VIDEO_CAPTURE_STATE_STOPPED))
        .Times(AnyNumber());

    // Simulate closing the IPC channel.
    host_->OnChannelClosing();

    // Release the reference to the mock object. The object will be destructed
    // on the current message loop.
    host_ = NULL;

    media_stream_manager_->WillDestroyCurrentMessageLoop();
  }

 protected:
  void StartCapture() {
    InSequence s;
    // 1. First - get info about the new resolution
    EXPECT_CALL(*host_.get(), OnDeviceInfo(kDeviceId));

    // 2. Change state to started
    EXPECT_CALL(*host_.get(),
                OnStateChanged(kDeviceId, VIDEO_CAPTURE_STATE_STARTED));

    // 3. Newly created buffers will arrive.
    EXPECT_CALL(*host_.get(), OnNewBufferCreated(kDeviceId, _, _, _))
        .Times(AnyNumber()).WillRepeatedly(Return());

    // 4. First filled buffer will arrive.
    base::RunLoop run_loop;
    EXPECT_CALL(*host_.get(), OnBufferFilled(kDeviceId, _, _))
        .Times(AnyNumber()).WillOnce(ExitMessageLoop(
            message_loop_, run_loop.QuitClosure()));

    media::VideoCaptureParams params;
    params.width = 352;
    params.height = 288;
    params.frame_per_second = 30;
    params.session_id = kTestFakeDeviceId;
    host_->OnStartCapture(kDeviceId, params);
    run_loop.Run();
  }

#ifdef DUMP_VIDEO
  void CaptureAndDumpVideo(int width, int heigt, int frame_rate) {
    InSequence s;
    // 1. First - get info about the new resolution
    EXPECT_CALL(*host_, OnDeviceInfo(kDeviceId));

    // 2. Change state to started
    EXPECT_CALL(*host_, OnStateChanged(kDeviceId, VIDEO_CAPTURE_STATE_STARTED));

    // 3. First filled buffer will arrive.
    base::RunLoop run_loop;
    EXPECT_CALL(*host_, OnBufferFilled(kDeviceId, _, _))
        .Times(AnyNumber())
        .WillOnce(ExitMessageLoop(message_loop_, run_loop.QuitClosure()));

    media::VideoCaptureParams params;
    params.width = width;
    params.height = heigt;
    params.frame_per_second = frame_rate;
    params.session_id = kTestFakeDeviceId;
    host_->SetDumpVideo(true);
    host_->OnStartCapture(kDeviceId, params);
    run_loop.Run();
  }
#endif

  void StopCapture() {
    base::RunLoop run_loop;
    EXPECT_CALL(*host_.get(),
                OnStateChanged(kDeviceId, VIDEO_CAPTURE_STATE_STOPPED))
        .WillOnce(ExitMessageLoop(message_loop_, run_loop.QuitClosure()));

    host_->OnStopCapture(kDeviceId);
    host_->SetReturnReceviedDibs(true);
    host_->ReturnReceivedDibs(kDeviceId);

    run_loop.Run();

    host_->SetReturnReceviedDibs(false);
    // Expect the VideoCaptureDevice has been stopped
    EXPECT_EQ(0u, host_->entries_.size());
  }

  void NotifyPacketReady() {
    base::RunLoop run_loop;
    EXPECT_CALL(*host_.get(), OnBufferFilled(kDeviceId, _, _))
        .Times(AnyNumber()).WillOnce(ExitMessageLoop(
            message_loop_, run_loop.QuitClosure()))
        .RetiresOnSaturation();
    run_loop.Run();
  }

  void ReturnReceivedPackets() {
    host_->ReturnReceivedDibs(kDeviceId);
  }

  void SimulateError() {
    // Expect a change state to error state  sent through IPC.
    EXPECT_CALL(*host_.get(),
                OnStateChanged(kDeviceId, VIDEO_CAPTURE_STATE_ERROR)).Times(1);
    VideoCaptureControllerID id(kDeviceId);
    host_->OnError(id);
    // Wait for the error callback.
    base::RunLoop().RunUntilIdle();
  }

  scoped_refptr<MockVideoCaptureHost> host_;

 private:
  scoped_ptr<media::AudioManager> audio_manager_;
  scoped_ptr<MediaStreamManager> media_stream_manager_;
  content::TestBrowserThreadBundle thread_bundle_;
  scoped_refptr<base::MessageLoopProxy> message_loop_;

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
  EXPECT_CALL(*host_.get(),
              OnStateChanged(kDeviceId, VIDEO_CAPTURE_STATE_STOPPED)).Times(0);
  StartCapture();
  NotifyPacketReady();
  SimulateError();
  base::PlatformThread::Sleep(base::TimeDelta::FromMilliseconds(200));
}

#ifdef DUMP_VIDEO
TEST_F(VideoCaptureHostTest, CaptureAndDumpVideoVga) {
  CaptureAndDumpVideo(640, 480, 30);
}
TEST_F(VideoCaptureHostTest, CaptureAndDump720P) {
  CaptureAndDumpVideo(1280, 720, 30);
}
#endif

}  // namespace content
