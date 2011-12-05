// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/bind.h"
#include "base/message_loop.h"
#include "content/browser/browser_thread_impl.h"
#include "content/browser/mock_resource_context.h"
#include "content/browser/renderer_host/media/media_stream_dispatcher_host.h"
#include "content/browser/renderer_host/media/media_stream_manager.h"
#include "content/browser/renderer_host/media/video_capture_manager.h"
#include "content/browser/resource_context.h"
#include "content/common/media/media_stream_messages.h"
#include "content/common/media/media_stream_options.h"
#include "ipc/ipc_message_macros.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::DeleteArg;
using ::testing::DoAll;
using ::testing::Return;
using content::BrowserThread;

using content::BrowserThreadImpl;

const int kProcessId = 5;
const int kRenderId = 6;
const int kPageRequestId = 7;

namespace media_stream {

class MockMediaStreamDispatcherHost : public MediaStreamDispatcherHost {
 public:
  MockMediaStreamDispatcherHost(content::ResourceContext* resource_context,
                                MessageLoop* message_loop)
      : MediaStreamDispatcherHost(resource_context, kProcessId),
        message_loop_(message_loop) {}
  virtual ~MockMediaStreamDispatcherHost() {}

  // A list of mock methods.
  MOCK_METHOD4(OnStreamGenerated,
               void(int routing_id, int request_id, int audio_array_size,
                    int video_array_size));
  MOCK_METHOD2(OnStreamGenerationFailed, void(int routing_id, int request_id));
  MOCK_METHOD2(OnAudioDeviceFailed, void(int routing_id, int index));
  MOCK_METHOD2(OnVideoDeviceFailed, void(int routing_id, int index));

  // Accessor to private functions.
  void OnGenerateStream(int page_request_id, const StreamOptions& components) {
    MediaStreamDispatcherHost::OnGenerateStream(kRenderId,
                                                page_request_id,
                                                components,
                                                std::string());
  }
  void OnStopGeneratedStream(const std::string& label) {
    MediaStreamDispatcherHost::OnStopGeneratedStream(kRenderId, label);
  }

  // Return the number of streams that have been opened or is being open.
  size_t NumberOfStreams() {
    return streams_.size();
  }

  std::string label_;
  StreamDeviceInfoArray audio_devices_;
  StreamDeviceInfoArray video_devices_;

 private:
  // This method is used to dispatch IPC messages to the renderer. We intercept
  // these messages here and dispatch to our mock methods to verify the
  // conversation between this object and the renderer.
  virtual bool Send(IPC::Message* message) {
    CHECK(message);

    // In this method we dispatch the messages to the according handlers as if
    // we are the renderer.
    bool handled = true;
    IPC_BEGIN_MESSAGE_MAP(MockMediaStreamDispatcherHost, *message)
      IPC_MESSAGE_HANDLER(MediaStreamMsg_StreamGenerated, OnStreamGenerated)
      IPC_MESSAGE_HANDLER(MediaStreamMsg_StreamGenerationFailed,
                          OnStreamGenerationFailed)
      IPC_MESSAGE_HANDLER(MediaStreamHostMsg_VideoDeviceFailed,
                          OnVideoDeviceFailed)
      IPC_MESSAGE_HANDLER(MediaStreamHostMsg_AudioDeviceFailed,
                          OnAudioDeviceFailed)
      IPC_MESSAGE_UNHANDLED(handled = false)
    IPC_END_MESSAGE_MAP()
    EXPECT_TRUE(handled);

    delete message;
    return true;
  }

  // These handler methods do minimal things and delegate to the mock methods.
  void OnStreamGenerated(
      const IPC::Message& msg,
      int request_id,
      std::string label,
      StreamDeviceInfoArray audio_device_list,
      StreamDeviceInfoArray video_device_list) {
    OnStreamGenerated(msg.routing_id(), request_id, audio_device_list.size(),
        video_device_list.size());
    // Notify that the event have occured.
    message_loop_->PostTask(FROM_HERE, new MessageLoop::QuitTask());
    label_ = label;
    audio_devices_ = audio_device_list;
    video_devices_ = video_device_list;
  }

  void OnStreamGenerationFailed(const IPC::Message& msg, int request_id) {
    OnStreamGenerationFailed(msg.routing_id(), request_id);
    message_loop_->PostTask(FROM_HERE, new MessageLoop::QuitTask());
    label_= "";
  }

  void OnAudioDeviceFailed(const IPC::Message& msg,
                           std::string label,
                           int index) {
    OnAudioDeviceFailed(msg.routing_id(), index);
    audio_devices_.erase(audio_devices_.begin() + index);
    message_loop_->PostTask(FROM_HERE, new MessageLoop::QuitTask());
  }

  void OnVideoDeviceFailed(const IPC::Message& msg,
                           std::string label,
                           int index) {
    OnVideoDeviceFailed(msg.routing_id(), index);
    video_devices_.erase(video_devices_.begin() + index);
    message_loop_->PostTask(FROM_HERE, new MessageLoop::QuitTask());
  }

  MessageLoop* message_loop_;
};

class MediaStreamDispatcherHostTest : public testing::Test {
 public:
  void WaitForResult() {
    message_loop_->Run();
  }

 protected:
  virtual void SetUp() {
    message_loop_.reset(new MessageLoop(MessageLoop::TYPE_IO));
    // ResourceContext must be created on UI thread.
    ui_thread_.reset(new BrowserThreadImpl(BrowserThread::UI,
                                           message_loop_.get()));
    // MediaStreamManager must be created and called on IO thread.
    io_thread_.reset(new BrowserThreadImpl(BrowserThread::IO,
                                           message_loop_.get()));

    // Create a MediaStreamManager instance and hand over pointer to
    // ResourceContext.
    media_stream_manager_.reset(new MediaStreamManager());
    // Make sure we use fake devices to avoid long delays.
    media_stream_manager_->UseFakeDevice();
    content::MockResourceContext::GetInstance()->set_media_stream_manager(
        media_stream_manager_.get());

    host_ = new MockMediaStreamDispatcherHost(
        content::MockResourceContext::GetInstance(), message_loop_.get());
  }

  virtual void TearDown() {
    content::MockResourceContext::GetInstance()->set_media_stream_manager(NULL);

    // Needed to make sure the manager finishes all tasks on its own thread.
    SyncWithVideoCaptureManagerThread();
  }

  // Called on the VideoCaptureManager thread.
  static void PostQuitMessageLoop(MessageLoop* message_loop) {
    message_loop->PostTask(FROM_HERE, new MessageLoop::QuitTask());
  }

  // Called on the main thread.
  static void PostQuitOnVideoCaptureManagerThread(
      MessageLoop* message_loop,
      media_stream::MediaStreamManager* media_stream_manager) {
    media_stream_manager->video_capture_manager()->GetMessageLoop()->
        PostTask(FROM_HERE,
                 base::Bind(&PostQuitMessageLoop, message_loop));
  }

  // SyncWithVideoCaptureManagerThread() waits until all pending tasks on the
  // video_capture_manager thread are executed while also processing pending
  // task in message_loop_ on the current thread. It is used to synchronize
  // with the video capture manager thread when we are stopping a video
  // capture device.
  void SyncWithVideoCaptureManagerThread() {
    message_loop_->PostTask(
        FROM_HERE,
        base::Bind(&PostQuitOnVideoCaptureManagerThread,
                   message_loop_.get(), media_stream_manager_.get()));
    message_loop_->Run();
  }

  scoped_refptr<MockMediaStreamDispatcherHost> host_;
  scoped_ptr<MessageLoop> message_loop_;
  scoped_ptr<BrowserThreadImpl> ui_thread_;
  scoped_ptr<BrowserThreadImpl> io_thread_;
  scoped_ptr<MediaStreamManager> media_stream_manager_;
};

TEST_F(MediaStreamDispatcherHostTest, GenerateStream) {
  StreamOptions options(false, StreamOptions::kFacingUser);

  EXPECT_CALL(*host_, OnStreamGenerated(kRenderId, kPageRequestId, 0, 1));
  host_->OnGenerateStream(kPageRequestId, options);

  WaitForResult();

  std::string label =  host_->label_;

  EXPECT_EQ(host_->audio_devices_.size(), 0u);
  EXPECT_EQ(host_->video_devices_.size(), 1u);
  EXPECT_EQ(host_->NumberOfStreams(), 1u);

  host_->OnStopGeneratedStream(label);
  EXPECT_EQ(host_->NumberOfStreams(), 0u);
}

TEST_F(MediaStreamDispatcherHostTest, GenerateThreeStreams) {
  // This test opens three video capture devices. Two fake devices exists and it
  // is expected the last call to |Open()| will open the first device again, but
  // with a different label.
  StreamOptions options(false, StreamOptions::kFacingUser);

  // Generate first stream.
  EXPECT_CALL(*host_, OnStreamGenerated(kRenderId, kPageRequestId, 0, 1));
  host_->OnGenerateStream(kPageRequestId, options);

  WaitForResult();

  // Check the latest generated stream.
  EXPECT_EQ(host_->audio_devices_.size(), 0u);
  EXPECT_EQ(host_->video_devices_.size(), 1u);
  std::string label1 = host_->label_;
  std::string device_id1 = host_->video_devices_.front().device_id;

  // Check that we now have one opened streams.
  EXPECT_EQ(host_->NumberOfStreams(), 1u);

  // Generate second stream.
  EXPECT_CALL(*host_, OnStreamGenerated(kRenderId, kPageRequestId + 1, 0, 1));
  host_->OnGenerateStream(kPageRequestId+1, options);

  WaitForResult();

  // Check the latest generated stream.
  EXPECT_EQ(host_->audio_devices_.size(), 0u);
  EXPECT_EQ(host_->video_devices_.size(), 1u);
  std::string label2 = host_->label_;
  std::string device_id2 = host_->video_devices_.front().device_id;
  EXPECT_NE(device_id1, device_id2);
  EXPECT_NE(label1, label2);

  // Check that we now have two opened streams.
  EXPECT_EQ(host_->NumberOfStreams(), 2u);

  // Generate third stream.
  EXPECT_CALL(*host_, OnStreamGenerated(kRenderId, kPageRequestId + 2, 0, 1));
  host_->OnGenerateStream(kPageRequestId+2, options);

  WaitForResult();

  // Check the latest generated stream.
  EXPECT_EQ(host_->audio_devices_.size(), 0u);
  EXPECT_EQ(host_->video_devices_.size(), 1u);
  std::string label3 = host_->label_;
  std::string device_id3 = host_->video_devices_.front().device_id;
  EXPECT_EQ(device_id1, device_id3);
  EXPECT_NE(device_id2, device_id3);
  EXPECT_NE(label1, label3);
  EXPECT_NE(label2, label3);

  // Check that we now have three opened streams.
  EXPECT_EQ(host_->NumberOfStreams(), 3u);

  host_->OnStopGeneratedStream(label1);
  host_->OnStopGeneratedStream(label2);
  host_->OnStopGeneratedStream(label3);
  EXPECT_EQ(host_->NumberOfStreams(), 0u);
}

TEST_F(MediaStreamDispatcherHostTest, FailDevice) {
  StreamOptions options(false, StreamOptions::kFacingUser);

  EXPECT_CALL(*host_, OnStreamGenerated(kRenderId, kPageRequestId, 0, 1));
  host_->OnGenerateStream(kPageRequestId, options);
  WaitForResult();
  std::string label =  host_->label_;

  EXPECT_EQ(host_->audio_devices_.size(), 0u);
  EXPECT_EQ(host_->video_devices_.size(), 1u);
  EXPECT_EQ(host_->NumberOfStreams(), 1u);

  EXPECT_CALL(*host_, OnVideoDeviceFailed(kRenderId, 0));
  int session_id = host_->video_devices_[0].session_id;
  content::MockResourceContext::GetInstance()->media_stream_manager()->
      video_capture_manager()->Error(session_id);
  WaitForResult();
  EXPECT_EQ(host_->video_devices_.size(), 0u);
  EXPECT_EQ(host_->NumberOfStreams(), 1u);

  // TODO(perkj): test audio device failure?

  host_->OnStopGeneratedStream(label);
  EXPECT_EQ(host_->NumberOfStreams(), 0u);
}

};  // namespace media_stream
