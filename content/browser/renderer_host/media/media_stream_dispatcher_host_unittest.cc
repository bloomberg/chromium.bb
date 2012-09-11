// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/bind.h"
#include "base/message_loop.h"
#include "content/browser/browser_thread_impl.h"
#include "content/browser/renderer_host/media/media_stream_dispatcher_host.h"
#include "content/browser/renderer_host/media/media_stream_manager.h"
#include "content/browser/renderer_host/media/mock_media_observer.h"
#include "content/browser/renderer_host/media/video_capture_manager.h"
#include "content/common/media/media_stream_messages.h"
#include "content/common/media/media_stream_options.h"
#include "content/public/test/mock_resource_context.h"
#include "content/test/test_content_browser_client.h"
#include "content/test/test_content_client.h"
#include "ipc/ipc_message_macros.h"
#include "media/audio/audio_manager.h"
#include "net/url_request/url_request_context.h"
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

class MockMediaStreamDispatcherHost : public MediaStreamDispatcherHost,
                                      public content::TestContentBrowserClient {
 public:
  MockMediaStreamDispatcherHost(MessageLoop* message_loop,
                                MediaStreamManager* manager)
      : MediaStreamDispatcherHost(kProcessId),
        message_loop_(message_loop),
        manager_(manager) {}

  // A list of mock methods.
  MOCK_METHOD4(OnStreamGenerated,
               void(int routing_id, int request_id, int audio_array_size,
                    int video_array_size));
  MOCK_METHOD2(OnStreamGenerationFailed, void(int routing_id, int request_id));
  MOCK_METHOD2(OnAudioDeviceFailed, void(int routing_id, int index));
  MOCK_METHOD2(OnVideoDeviceFailed, void(int routing_id, int index));
  MOCK_METHOD0(GetMediaObserver, content::MediaObserver*());

  // Accessor to private functions.
  void OnGenerateStream(int page_request_id, const StreamOptions& components) {
    MediaStreamDispatcherHost::OnGenerateStream(kRenderId,
                                                page_request_id,
                                                components,
                                                GURL());
  }
  void OnGenerateStreamForDevice(int page_request_id,
                                 const StreamOptions& components,
                                 const std::string& device_id) {
    MediaStreamDispatcherHost::OnGenerateStreamForDevice(kRenderId,
                                                         page_request_id,
                                                         components,
                                                         device_id,
                                                         GURL());
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
  virtual ~MockMediaStreamDispatcherHost() {}

  // This method is used to dispatch IPC messages to the renderer. We intercept
  // these messages here and dispatch to our mock methods to verify the
  // conversation between this object and the renderer.
  virtual bool Send(IPC::Message* message) OVERRIDE {
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

  // Use our own MediaStreamManager.
  virtual MediaStreamManager* GetManager() OVERRIDE {
    return manager_;
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
    message_loop_->PostTask(FROM_HERE, MessageLoop::QuitClosure());
    label_ = label;
    audio_devices_ = audio_device_list;
    video_devices_ = video_device_list;
  }

  void OnStreamGenerationFailed(const IPC::Message& msg, int request_id) {
    OnStreamGenerationFailed(msg.routing_id(), request_id);
    message_loop_->PostTask(FROM_HERE, MessageLoop::QuitClosure());
    label_= "";
  }

  void OnAudioDeviceFailed(const IPC::Message& msg,
                           std::string label,
                           int index) {
    OnAudioDeviceFailed(msg.routing_id(), index);
    audio_devices_.erase(audio_devices_.begin() + index);
    message_loop_->PostTask(FROM_HERE, MessageLoop::QuitClosure());
  }

  void OnVideoDeviceFailed(const IPC::Message& msg,
                           std::string label,
                           int index) {
    OnVideoDeviceFailed(msg.routing_id(), index);
    video_devices_.erase(video_devices_.begin() + index);
    message_loop_->PostTask(FROM_HERE, MessageLoop::QuitClosure());
  }

  MessageLoop* message_loop_;
  MediaStreamManager* manager_;
};

class MediaStreamDispatcherHostTest : public testing::Test {
 public:
  MediaStreamDispatcherHostTest() : old_client_(NULL),
                                    old_browser_client_(NULL) {}
  virtual ~MediaStreamDispatcherHostTest() {}

  void WaitForResult() {
    message_loop_->Run();
  }

 protected:
  virtual void SetUp() OVERRIDE {
    // MediaStreamManager must be created and called on IO thread.
    message_loop_.reset(new MessageLoop(MessageLoop::TYPE_IO));
    io_thread_.reset(new BrowserThreadImpl(BrowserThread::IO,
                                           message_loop_.get()));

    // Create our own media observer.
    media_observer_.reset(new MockMediaObserver());

    // Create our own MediaStreamManager.
    audio_manager_.reset(media::AudioManager::Create());
    media_stream_manager_.reset(
        new media_stream::MediaStreamManager(audio_manager_.get()));
    // Make sure we use fake devices to avoid long delays.
    media_stream_manager_->UseFakeDevice();

    host_ = new MockMediaStreamDispatcherHost(message_loop_.get(),
                                              media_stream_manager_.get());

    // Use the fake content client and browser.
    old_client_ = content::GetContentClient();
    old_browser_client_ = content::GetContentClient()->browser();
    content_client_.reset(new TestContentClient);
    content::SetContentClient(content_client_.get());
    content_client_->set_browser_for_testing(host_);
  }

  virtual void TearDown() OVERRIDE {
    message_loop_->RunAllPending();

    // Recover the old browser client and content client.
    content::GetContentClient()->set_browser_for_testing(old_browser_client_);
    content::SetContentClient(old_client_);
    content_client_.reset();

    // Delete the IO message loop to delete the device thread,
    // AudioInputDeviceManager and VideoCaptureManager.
    message_loop_.reset();
  }

  scoped_refptr<MockMediaStreamDispatcherHost> host_;
  scoped_ptr<MessageLoop> message_loop_;
  scoped_ptr<BrowserThreadImpl> io_thread_;
  scoped_ptr<media::AudioManager> audio_manager_;
  scoped_ptr<MediaStreamManager> media_stream_manager_;
  content::ContentClient* old_client_;
  content::ContentBrowserClient* old_browser_client_;
  scoped_ptr<content::ContentClient> content_client_;
  scoped_ptr<MockMediaObserver> media_observer_;
};

TEST_F(MediaStreamDispatcherHostTest, GenerateStream) {
  StreamOptions options(false, true);

  EXPECT_CALL(*host_, GetMediaObserver())
      .WillRepeatedly(Return(media_observer_.get()));
  EXPECT_CALL(*host_, OnStreamGenerated(kRenderId, kPageRequestId, 0, 1));
  host_->OnGenerateStream(kPageRequestId, options);

  EXPECT_CALL(*media_observer_.get(), OnCaptureDevicesOpened(_, _, _));
  EXPECT_CALL(*media_observer_.get(), OnCaptureDevicesClosed(_, _, _));

  WaitForResult();

  std::string label =  host_->label_;

  EXPECT_EQ(host_->audio_devices_.size(), 0u);
  EXPECT_EQ(host_->video_devices_.size(), 1u);
  EXPECT_EQ(host_->NumberOfStreams(), 1u);

  host_->OnStopGeneratedStream(label);
  EXPECT_EQ(host_->NumberOfStreams(), 0u);
}

TEST_F(MediaStreamDispatcherHostTest, GenerateStreamForDevice) {
  static const char kDeviceId[] = "/dev/video0";

  StreamOptions options(content::MEDIA_NO_SERVICE,
                        content::MEDIA_DEVICE_VIDEO_CAPTURE);

  EXPECT_CALL(*host_, GetMediaObserver())
      .WillRepeatedly(Return(media_observer_.get()));
  EXPECT_CALL(*host_, OnStreamGenerated(kRenderId, kPageRequestId, 0, 1));
  host_->OnGenerateStreamForDevice(kPageRequestId, options, kDeviceId);

  EXPECT_CALL(*media_observer_.get(), OnCaptureDevicesOpened(_, _, _));
  EXPECT_CALL(*media_observer_.get(), OnCaptureDevicesClosed(_, _, _));

  WaitForResult();

  std::string label = host_->label_;

  EXPECT_EQ(0u, host_->audio_devices_.size());
  EXPECT_EQ(1u, host_->video_devices_.size());
  EXPECT_EQ(1u, host_->NumberOfStreams());

  host_->OnStopGeneratedStream(label);
  EXPECT_EQ(0u, host_->NumberOfStreams());
}

TEST_F(MediaStreamDispatcherHostTest, GenerateThreeStreams) {
  // This test opens three video capture devices. Two fake devices exists and it
  // is expected the last call to |Open()| will open the first device again, but
  // with a different label.
  StreamOptions options(false, true);

  // Generate first stream.
  EXPECT_CALL(*host_, GetMediaObserver())
      .WillRepeatedly(Return(media_observer_.get()));
  EXPECT_CALL(*host_, OnStreamGenerated(kRenderId, kPageRequestId, 0, 1));
  host_->OnGenerateStream(kPageRequestId, options);

  EXPECT_CALL(*media_observer_.get(), OnCaptureDevicesOpened(_, _, _));

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

  EXPECT_CALL(*media_observer_.get(), OnCaptureDevicesOpened(_, _, _));

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

  EXPECT_CALL(*media_observer_.get(), OnCaptureDevicesOpened(_, _, _));
  EXPECT_CALL(*media_observer_.get(), OnCaptureDevicesClosed(_, _, _))
      .Times(3);

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
  StreamOptions options(false, true);

  EXPECT_CALL(*host_, GetMediaObserver())
      .WillRepeatedly(Return(media_observer_.get()));
  EXPECT_CALL(*host_, OnStreamGenerated(kRenderId, kPageRequestId, 0, 1));
  host_->OnGenerateStream(kPageRequestId, options);
  EXPECT_CALL(*media_observer_.get(), OnCaptureDevicesOpened(_, _, _));
  WaitForResult();
  std::string label =  host_->label_;

  EXPECT_EQ(host_->audio_devices_.size(), 0u);
  EXPECT_EQ(host_->video_devices_.size(), 1u);
  EXPECT_EQ(host_->NumberOfStreams(), 1u);

  EXPECT_CALL(*host_, OnVideoDeviceFailed(kRenderId, 0));
  int session_id = host_->video_devices_[0].session_id;
  media_stream_manager_->video_capture_manager()->Error(session_id);
  WaitForResult();
  EXPECT_EQ(host_->video_devices_.size(), 0u);
  EXPECT_EQ(host_->NumberOfStreams(), 1u);

  // TODO(perkj): test audio device failure?

  EXPECT_CALL(*media_observer_.get(), OnCaptureDevicesClosed(_, _, _));
  host_->OnStopGeneratedStream(label);
  EXPECT_EQ(host_->NumberOfStreams(), 0u);
}

TEST_F(MediaStreamDispatcherHostTest, CancelPendingStreamsOnChannelClosing) {
  StreamOptions options(false, true);

  EXPECT_CALL(*host_, GetMediaObserver())
      .WillRepeatedly(Return(media_observer_.get()));

  // Create multiple GenerateStream requests.
  size_t streams = 5;
  for (size_t i = 1; i <= streams; ++i) {
    host_->OnGenerateStream(kPageRequestId + i, options);
    EXPECT_EQ(host_->NumberOfStreams(), i);
  }

  // Calling OnChannelClosing() to cancel all the pending requests.
  host_->OnChannelClosing();

  // Streams should have been cleaned up.
  EXPECT_EQ(host_->NumberOfStreams(), 0u);
}

TEST_F(MediaStreamDispatcherHostTest, StopGeneratedStreamsOnChannelClosing) {
  StreamOptions options(false, true);

  EXPECT_CALL(*host_, GetMediaObserver())
      .WillRepeatedly(Return(media_observer_.get()));

  // Create first group of streams.
  size_t generated_streams = 3;
  for (size_t i = 0; i < generated_streams; ++i) {
    EXPECT_CALL(*host_, OnStreamGenerated(kRenderId, kPageRequestId + i, 0, 1));
    EXPECT_CALL(*media_observer_.get(), OnCaptureDevicesOpened(_, _, _));
    host_->OnGenerateStream(kPageRequestId + i, options);

    // Wait until the stream is generated.
    WaitForResult();
  }
  EXPECT_EQ(host_->NumberOfStreams(), generated_streams);

  // Calling OnChannelClosing() to cancel all the pending/generated streams.
  EXPECT_CALL(*media_observer_.get(), OnCaptureDevicesClosed(_, _, _))
      .Times(3);
  host_->OnChannelClosing();

  // Streams should have been cleaned up.
  EXPECT_EQ(host_->NumberOfStreams(), 0u);
}

};  // namespace media_stream
