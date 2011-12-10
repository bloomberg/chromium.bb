// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/environment.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/process_util.h"
#include "base/sync_socket.h"
#include "content/browser/browser_thread_impl.h"
#include "content/browser/mock_resource_context.h"
#include "content/browser/renderer_host/media/audio_renderer_host.h"
#include "content/browser/renderer_host/media/mock_media_observer.h"
#include "content/common/media/audio_messages.h"
#include "ipc/ipc_message_utils.h"
#include "media/audio/audio_manager.h"
#include "media/audio/fake_audio_output_stream.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::AtLeast;
using ::testing::DoAll;
using ::testing::InSequence;
using ::testing::InvokeWithoutArgs;
using ::testing::Return;
using ::testing::SaveArg;
using ::testing::SetArgumentPointee;
using content::BrowserThread;

using content::BrowserThreadImpl;

static const int kStreamId = 50;

static bool IsRunningHeadless() {
  scoped_ptr<base::Environment> env(base::Environment::Create());
  if (env->HasVar("CHROME_HEADLESS"))
    return true;
  return false;
}

class MockAudioRendererHost : public AudioRendererHost {
 public:
  MockAudioRendererHost(const content::ResourceContext* resource_context)
      : AudioRendererHost(resource_context),
        shared_memory_length_(0) {
  }

  virtual ~MockAudioRendererHost() {
  }

  // A list of mock methods.
  MOCK_METHOD2(OnRequestPacket,
               void(int stream_id, AudioBuffersState buffers_state));
  MOCK_METHOD2(OnStreamCreated,
               void(int stream_id, int length));
  MOCK_METHOD2(OnLowLatencyStreamCreated,
               void(int stream_id, int length));
  MOCK_METHOD1(OnStreamPlaying, void(int stream_id));
  MOCK_METHOD1(OnStreamPaused, void(int stream_id));
  MOCK_METHOD1(OnStreamError, void(int stream_id));
  MOCK_METHOD2(OnStreamVolume, void(int stream_id, double volume));

  base::SharedMemory* shared_memory() { return shared_memory_.get(); }
  uint32 shared_memory_length() { return shared_memory_length_; }

  base::SyncSocket* sync_socket() { return sync_socket_.get(); }

 private:
  // This method is used to dispatch IPC messages to the renderer. We intercept
  // these messages here and dispatch to our mock methods to verify the
  // conversation between this object and the renderer.
  virtual bool Send(IPC::Message* message) {
    CHECK(message);

    // In this method we dispatch the messages to the according handlers as if
    // we are the renderer.
    bool handled = true;
    IPC_BEGIN_MESSAGE_MAP(MockAudioRendererHost, *message)
      IPC_MESSAGE_HANDLER(AudioMsg_RequestPacket, OnRequestPacket)
      IPC_MESSAGE_HANDLER(AudioMsg_NotifyStreamCreated, OnStreamCreated)
      IPC_MESSAGE_HANDLER(AudioMsg_NotifyLowLatencyStreamCreated,
                          OnLowLatencyStreamCreated)
      IPC_MESSAGE_HANDLER(AudioMsg_NotifyStreamStateChanged,
                          OnStreamStateChanged)
      IPC_MESSAGE_HANDLER(AudioMsg_NotifyStreamVolume, OnStreamVolume)
      IPC_MESSAGE_UNHANDLED(handled = false)
    IPC_END_MESSAGE_MAP()
    EXPECT_TRUE(handled);

    delete message;
    return true;
  }

  // These handler methods do minimal things and delegate to the mock methods.
  void OnRequestPacket(const IPC::Message& msg, int stream_id,
                       AudioBuffersState buffers_state) {
    OnRequestPacket(stream_id, buffers_state);
  }

  void OnStreamCreated(const IPC::Message& msg, int stream_id,
                       base::SharedMemoryHandle handle, uint32 length) {
    // Maps the shared memory.
    shared_memory_.reset(new base::SharedMemory(handle, false));
    ASSERT_TRUE(shared_memory_->Map(length));
    ASSERT_TRUE(shared_memory_->memory());
    shared_memory_length_ = length;

    // And then delegate the call to the mock method.
    OnStreamCreated(stream_id, length);
  }

  void OnLowLatencyStreamCreated(const IPC::Message& msg, int stream_id,
                               base::SharedMemoryHandle handle,
#if defined(OS_WIN)
                               base::SyncSocket::Handle socket_handle,
#else
                               base::FileDescriptor socket_descriptor,
#endif
                               uint32 length) {
    // Maps the shared memory.
    shared_memory_.reset(new base::SharedMemory(handle, false));
    CHECK(shared_memory_->Map(length));
    CHECK(shared_memory_->memory());
    shared_memory_length_ = length;

    // Create the SyncSocket using the handle.
    base::SyncSocket::Handle sync_socket_handle;
#if defined(OS_WIN)
    sync_socket_handle = socket_handle;
#else
    sync_socket_handle = socket_descriptor.fd;
#endif
    sync_socket_.reset(new base::SyncSocket(sync_socket_handle));

    // And then delegate the call to the mock method.
    OnLowLatencyStreamCreated(stream_id, length);
  }

  void OnStreamStateChanged(const IPC::Message& msg, int stream_id,
                            AudioStreamState state) {
    if (state == kAudioStreamPlaying) {
      OnStreamPlaying(stream_id);
    } else if (state == kAudioStreamPaused) {
      OnStreamPaused(stream_id);
    } else if (state == kAudioStreamError) {
      OnStreamError(stream_id);
    } else {
      FAIL() << "Unknown stream state";
    }
  }

  void OnStreamVolume(const IPC::Message& msg, int stream_id, double volume) {
    OnStreamVolume(stream_id, volume);
  }

  scoped_ptr<base::SharedMemory> shared_memory_;
  scoped_ptr<base::SyncSocket> sync_socket_;
  uint32 shared_memory_length_;

  DISALLOW_COPY_AND_ASSIGN(MockAudioRendererHost);
};

ACTION_P(QuitMessageLoop, message_loop) {
  message_loop->PostTask(FROM_HERE, MessageLoop::QuitClosure());
}

class AudioRendererHostTest : public testing::Test {
 public:
  AudioRendererHostTest()
      : mock_stream_(true) {
  }

 protected:
  virtual void SetUp() {
    // Create a message loop so AudioRendererHost can use it.
    message_loop_.reset(new MessageLoop(MessageLoop::TYPE_IO));

    // Claim to be on both the UI and IO threads to pass all the DCHECKS.
    io_thread_.reset(new BrowserThreadImpl(BrowserThread::IO,
                                           message_loop_.get()));
    ui_thread_.reset(new BrowserThreadImpl(BrowserThread::UI,
                                           message_loop_.get()));

    observer_.reset(new MockMediaObserver());
    content::MockResourceContext* context =
        content::MockResourceContext::GetInstance();
    context->set_media_observer(observer_.get());
    host_ = new MockAudioRendererHost(context);

    // Simulate IPC channel connected.
    host_->OnChannelConnected(base::GetCurrentProcId());
  }

  virtual void TearDown() {
    // Simulate closing the IPC channel.
    host_->OnChannelClosing();

    // Release the reference to the mock object.  The object will be destructed
    // on message_loop_.
    host_ = NULL;

    // We need to continue running message_loop_ to complete all destructions.
    SyncWithAudioThread();

    io_thread_.reset();
    ui_thread_.reset();
  }

  void Create() {
    EXPECT_CALL(*observer_,
                OnSetAudioStreamStatus(_, kStreamId, "created"));
    EXPECT_CALL(*observer_, OnDeleteAudioStream(_, kStreamId));

    InSequence s;
    // 1. We will first receive a OnStreamCreated() signal.
    EXPECT_CALL(*host_, OnStreamCreated(kStreamId, _));

    // 2. First packet request will arrive.
    EXPECT_CALL(*host_, OnRequestPacket(kStreamId, _))
        .WillOnce(QuitMessageLoop(message_loop_.get()));

    AudioParameters params;
    if (mock_stream_)
      params.format = AudioParameters::AUDIO_MOCK;
    else
      params.format = AudioParameters::AUDIO_PCM_LINEAR;
    params.channels = 2;
    params.sample_rate = AudioParameters::kAudioCDSampleRate;
    params.bits_per_sample = 16;
    params.samples_per_packet = 0;

    // Send a create stream message to the audio output stream and wait until
    // we receive the created message.
    host_->OnCreateStream(kStreamId, params, false);
    message_loop_->Run();
  }

  void CreateLowLatency() {
    EXPECT_CALL(*observer_,
                OnSetAudioStreamStatus(_, kStreamId, "created"));
    EXPECT_CALL(*observer_, OnDeleteAudioStream(_, kStreamId));

    InSequence s;
    // We will first receive a OnLowLatencyStreamCreated() signal.
    EXPECT_CALL(*host_,
                OnLowLatencyStreamCreated(kStreamId, _))
        .WillOnce(QuitMessageLoop(message_loop_.get()));

    AudioParameters params;
    if (mock_stream_)
      params.format = AudioParameters::AUDIO_MOCK;
    else
      params.format = AudioParameters::AUDIO_PCM_LINEAR;
    params.channels = 2;
    params.sample_rate = AudioParameters::kAudioCDSampleRate;
    params.bits_per_sample = 16;
    params.samples_per_packet = 0;

    // Send a create stream message to the audio output stream and wait until
    // we receive the created message.
    host_->OnCreateStream(kStreamId, params, true);
    message_loop_->Run();
  }

  void Close() {
    EXPECT_CALL(*observer_,
                OnSetAudioStreamStatus(_, kStreamId, "closed"));

    // Send a message to AudioRendererHost to tell it we want to close the
    // stream.
    host_->OnCloseStream(kStreamId);
    message_loop_->RunAllPending();
  }

  void Play() {
    EXPECT_CALL(*observer_,
                OnSetAudioStreamPlaying(_, kStreamId, true));
    EXPECT_CALL(*host_, OnStreamPlaying(kStreamId))
        .WillOnce(QuitMessageLoop(message_loop_.get()));

    host_->OnPlayStream(kStreamId);
    message_loop_->Run();
  }

  void Pause() {
    EXPECT_CALL(*observer_,
                OnSetAudioStreamPlaying(_, kStreamId, false));
    EXPECT_CALL(*host_, OnStreamPaused(kStreamId))
        .WillOnce(QuitMessageLoop(message_loop_.get()));

    host_->OnPauseStream(kStreamId);
    message_loop_->Run();
  }

  void SetVolume(double volume) {
    EXPECT_CALL(*observer_,
                OnSetAudioStreamVolume(_, kStreamId, volume));

    host_->OnSetVolume(kStreamId, volume);
    message_loop_->RunAllPending();
  }

  void NotifyPacketReady() {
    EXPECT_CALL(*host_, OnRequestPacket(kStreamId, _))
        .WillOnce(QuitMessageLoop(message_loop_.get()));

    memset(host_->shared_memory()->memory(), 0, host_->shared_memory_length());
    host_->OnNotifyPacketReady(kStreamId, host_->shared_memory_length());
    message_loop_->Run();
  }

  void SimulateError() {
    EXPECT_CALL(*observer_,
                OnSetAudioStreamStatus(_, kStreamId, "error"));
    // Find the first AudioOutputController in the AudioRendererHost.
    CHECK(host_->audio_entries_.size())
        << "Calls Create() before calling this method";
    media::AudioOutputController* controller =
        host_->audio_entries_.begin()->second->controller;
    CHECK(controller) << "AudioOutputController not found";

    // Expect an error signal sent through IPC.
    EXPECT_CALL(*host_, OnStreamError(kStreamId));

    // Simulate an error sent from the audio device.
    host_->OnError(controller, 0);
    SyncWithAudioThread();

    // Expect the audio stream record is removed.
    EXPECT_EQ(0u, host_->audio_entries_.size());
  }

  // Called on the audio thread.
  static void PostQuitMessageLoop(MessageLoop* message_loop) {
    message_loop->PostTask(FROM_HERE, MessageLoop::QuitClosure());
  }

  // Called on the main thread.
  static void PostQuitOnAudioThread(MessageLoop* message_loop) {
    AudioManager::GetAudioManager()->GetMessageLoop()->PostTask(
        FROM_HERE, base::Bind(&PostQuitMessageLoop, message_loop));
  }

  // SyncWithAudioThread() waits until all pending tasks on the audio thread
  // are executed while also processing pending task in message_loop_ on the
  // current thread. It is used to synchronize with the audio thread when we are
  // closing an audio stream.
  void SyncWithAudioThread() {
    message_loop_->PostTask(
        FROM_HERE, base::Bind(&PostQuitOnAudioThread, message_loop_.get()));
    message_loop_->Run();
  }

  MessageLoop* message_loop() { return message_loop_.get(); }
  MockAudioRendererHost* host() { return host_; }
  void EnableRealDevice() { mock_stream_ = false; }

 private:
  bool mock_stream_;
  scoped_ptr<MockMediaObserver> observer_;
  scoped_refptr<MockAudioRendererHost> host_;
  scoped_ptr<MessageLoop> message_loop_;
  scoped_ptr<BrowserThreadImpl> io_thread_;
  scoped_ptr<BrowserThreadImpl> ui_thread_;

  DISALLOW_COPY_AND_ASSIGN(AudioRendererHostTest);
};

TEST_F(AudioRendererHostTest, CreateAndClose) {
  if (!IsRunningHeadless())
    EnableRealDevice();

  Create();
  Close();
}

TEST_F(AudioRendererHostTest, CreatePlayAndClose) {
  if (!IsRunningHeadless())
    EnableRealDevice();

  Create();
  Play();
  Close();
}

TEST_F(AudioRendererHostTest, CreatePlayPauseAndClose) {
  if (!IsRunningHeadless())
    EnableRealDevice();

  Create();
  Play();
  Pause();
  Close();
}

TEST_F(AudioRendererHostTest, SetVolume) {
  if (!IsRunningHeadless())
    EnableRealDevice();

  Create();
  SetVolume(0.5);
  Play();
  Pause();
  Close();

  // Expect the volume is set.
  if (IsRunningHeadless()) {
    EXPECT_EQ(0.5, FakeAudioOutputStream::GetLastFakeStream()->volume());
  }
}

// Simulate the case where a stream is not properly closed.
TEST_F(AudioRendererHostTest, CreateAndShutdown) {
  if (!IsRunningHeadless())
    EnableRealDevice();

  Create();
}

// Simulate the case where a stream is not properly closed.
TEST_F(AudioRendererHostTest, CreatePlayAndShutdown) {
  if (!IsRunningHeadless())
    EnableRealDevice();

  Create();
  Play();
}

// Simulate the case where a stream is not properly closed.
TEST_F(AudioRendererHostTest, CreatePlayPauseAndShutdown) {
  if (!IsRunningHeadless())
    EnableRealDevice();

  Create();
  Play();
  Pause();
}

TEST_F(AudioRendererHostTest, DataConversationMockStream) {
  Create();

  // Note that we only do notify three times because the buffer capacity is
  // triple of one packet size.
  NotifyPacketReady();
  NotifyPacketReady();
  NotifyPacketReady();
  Close();
}

TEST_F(AudioRendererHostTest, DataConversationRealStream) {
  if (IsRunningHeadless())
    return;
  EnableRealDevice();
  Create();
  Play();

  // If this is a real audio device, the data conversation is not limited
  // to the buffer capacity of AudioOutputController. So we do 5 exchanges
  // before we close the device.
  for (int i = 0; i < 5; ++i) {
    NotifyPacketReady();
  }
  Close();
}

TEST_F(AudioRendererHostTest, SimulateError) {
  if (!IsRunningHeadless())
    EnableRealDevice();

  Create();
  Play();
  SimulateError();
}

// Simulate the case when an error is generated on the browser process,
// the audio device is closed but the render process try to close the
// audio stream again.
TEST_F(AudioRendererHostTest, SimulateErrorAndClose) {
  if (!IsRunningHeadless())
    EnableRealDevice();

  Create();
  Play();
  SimulateError();
  Close();
}

TEST_F(AudioRendererHostTest, CreateLowLatencyAndClose) {
  if (!IsRunningHeadless())
    EnableRealDevice();

  CreateLowLatency();
  Close();
}

// Simulate the case where a stream is not properly closed.
TEST_F(AudioRendererHostTest, CreateLowLatencyAndShutdown) {
  if (!IsRunningHeadless())
    EnableRealDevice();

  CreateLowLatency();
}

// TODO(hclam): Add tests for data conversation in low latency mode.
