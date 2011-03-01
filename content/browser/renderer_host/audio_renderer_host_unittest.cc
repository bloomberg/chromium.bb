// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/environment.h"
#include "base/message_loop.h"
#include "base/process_util.h"
#include "base/scoped_ptr.h"
#include "base/sync_socket.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/render_messages_params.h"
#include "content/browser/browser_thread.h"
#include "content/browser/renderer_host/audio_renderer_host.h"
#include "ipc/ipc_message_utils.h"
#include "media/audio/audio_manager.h"
#include "media/audio/fake_audio_output_stream.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::DoAll;
using ::testing::InSequence;
using ::testing::InvokeWithoutArgs;
using ::testing::Return;
using ::testing::SaveArg;
using ::testing::SetArgumentPointee;

static const int kInvalidId = -1;
static const int kRouteId = 200;
static const int kStreamId = 50;

static bool IsRunningHeadless() {
  scoped_ptr<base::Environment> env(base::Environment::Create());
  if (env->HasVar("CHROME_HEADLESS"))
    return true;
  return false;
}

class MockAudioRendererHost : public AudioRendererHost {
 public:
  MockAudioRendererHost() : shared_memory_length_(0) {
  }

  virtual ~MockAudioRendererHost() {
  }

  // A list of mock methods.
  MOCK_METHOD3(OnRequestPacket,
               void(int routing_id, int stream_id,
                    AudioBuffersState buffers_state));
  MOCK_METHOD3(OnStreamCreated,
               void(int routing_id, int stream_id, int length));
  MOCK_METHOD3(OnLowLatencyStreamCreated,
               void(int routing_id, int stream_id, int length));
  MOCK_METHOD2(OnStreamPlaying, void(int routing_id, int stream_id));
  MOCK_METHOD2(OnStreamPaused, void(int routing_id, int stream_id));
  MOCK_METHOD2(OnStreamError, void(int routing_id, int stream_id));
  MOCK_METHOD3(OnStreamVolume,
               void(int routing_id, int stream_id, double volume));

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
      IPC_MESSAGE_HANDLER(ViewMsg_RequestAudioPacket, OnRequestPacket)
      IPC_MESSAGE_HANDLER(ViewMsg_NotifyAudioStreamCreated, OnStreamCreated)
      IPC_MESSAGE_HANDLER(ViewMsg_NotifyLowLatencyAudioStreamCreated,
                          OnLowLatencyStreamCreated)
      IPC_MESSAGE_HANDLER(ViewMsg_NotifyAudioStreamStateChanged,
                          OnStreamStateChanged)
      IPC_MESSAGE_HANDLER(ViewMsg_NotifyAudioStreamVolume, OnStreamVolume)
      IPC_MESSAGE_UNHANDLED(handled = false)
    IPC_END_MESSAGE_MAP()
    EXPECT_TRUE(handled);

    delete message;
    return true;
  }

  // These handler methods do minimal things and delegate to the mock methods.
  void OnRequestPacket(const IPC::Message& msg, int stream_id,
                       AudioBuffersState buffers_state) {
    OnRequestPacket(msg.routing_id(), stream_id, buffers_state);
  }

  void OnStreamCreated(const IPC::Message& msg, int stream_id,
                       base::SharedMemoryHandle handle, uint32 length) {
    // Maps the shared memory.
    shared_memory_.reset(new base::SharedMemory(handle, false));
    ASSERT_TRUE(shared_memory_->Map(length));
    ASSERT_TRUE(shared_memory_->memory());
    shared_memory_length_ = length;

    // And then delegate the call to the mock method.
    OnStreamCreated(msg.routing_id(), stream_id, length);
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
    OnLowLatencyStreamCreated(msg.routing_id(), stream_id, length);
  }

  void OnStreamStateChanged(const IPC::Message& msg, int stream_id,
                            const ViewMsg_AudioStreamState_Params& params) {
    if (params.state == ViewMsg_AudioStreamState_Params::kPlaying) {
      OnStreamPlaying(msg.routing_id(), stream_id);
    } else if (params.state == ViewMsg_AudioStreamState_Params::kPaused) {
      OnStreamPaused(msg.routing_id(), stream_id);
    } else if (params.state == ViewMsg_AudioStreamState_Params::kError) {
      OnStreamError(msg.routing_id(), stream_id);
    } else {
      FAIL() << "Unknown stream state";
    }
  }

  void OnStreamVolume(const IPC::Message& msg, int stream_id, double volume) {
    OnStreamVolume(msg.routing_id(), stream_id, volume);
  }

  scoped_ptr<base::SharedMemory> shared_memory_;
  scoped_ptr<base::SyncSocket> sync_socket_;
  uint32 shared_memory_length_;

  DISALLOW_COPY_AND_ASSIGN(MockAudioRendererHost);
};

ACTION_P(QuitMessageLoop, message_loop) {
  message_loop->PostTask(FROM_HERE, new MessageLoop::QuitTask());
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
    io_thread_.reset(new BrowserThread(BrowserThread::IO, message_loop_.get()));
    host_ = new MockAudioRendererHost();

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
  }

  void Create() {
    InSequence s;
    // 1. We will first receive a OnStreamCreated() signal.
    EXPECT_CALL(*host_,
                OnStreamCreated(kRouteId, kStreamId, _));

    // 2. First packet request will arrive.
    EXPECT_CALL(*host_, OnRequestPacket(kRouteId, kStreamId, _))
        .WillOnce(QuitMessageLoop(message_loop_.get()));

    IPC::Message msg;
    msg.set_routing_id(kRouteId);

    ViewHostMsg_Audio_CreateStream_Params params;
    if (mock_stream_)
      params.params.format = AudioParameters::AUDIO_MOCK;
    else
      params.params.format = AudioParameters::AUDIO_PCM_LINEAR;
    params.params.channels = 2;
    params.params.sample_rate = AudioParameters::kAudioCDSampleRate;
    params.params.bits_per_sample = 16;
    params.params.samples_per_packet = 0;

    // Send a create stream message to the audio output stream and wait until
    // we receive the created message.
    host_->OnCreateStream(msg, kStreamId, params, false);
    message_loop_->Run();
  }

  void CreateLowLatency() {
    InSequence s;
    // We will first receive a OnLowLatencyStreamCreated() signal.
    EXPECT_CALL(*host_,
                OnLowLatencyStreamCreated(kRouteId, kStreamId, _))
        .WillOnce(QuitMessageLoop(message_loop_.get()));

    IPC::Message msg;
    msg.set_routing_id(kRouteId);

    ViewHostMsg_Audio_CreateStream_Params params;
    if (mock_stream_)
      params.params.format = AudioParameters::AUDIO_MOCK;
    else
      params.params.format = AudioParameters::AUDIO_PCM_LINEAR;
    params.params.channels = 2;
    params.params.sample_rate = AudioParameters::kAudioCDSampleRate;
    params.params.bits_per_sample = 16;
    params.params.samples_per_packet = 0;

    // Send a create stream message to the audio output stream and wait until
    // we receive the created message.
    host_->OnCreateStream(msg, kStreamId, params, true);
    message_loop_->Run();
  }

  void Close() {
    // Send a message to AudioRendererHost to tell it we want to close the
    // stream.
    IPC::Message msg;
    msg.set_routing_id(kRouteId);
    host_->OnCloseStream(msg, kStreamId);
    message_loop_->RunAllPending();
  }

  void Play() {
    EXPECT_CALL(*host_, OnStreamPlaying(kRouteId, kStreamId))
        .WillOnce(QuitMessageLoop(message_loop_.get()));

    IPC::Message msg;
    msg.set_routing_id(kRouteId);
    host_->OnPlayStream(msg, kStreamId);
    message_loop_->Run();
  }

  void Pause() {
    EXPECT_CALL(*host_, OnStreamPaused(kRouteId, kStreamId))
        .WillOnce(QuitMessageLoop(message_loop_.get()));

    IPC::Message msg;
    msg.set_routing_id(kRouteId);
    host_->OnPauseStream(msg, kStreamId);
    message_loop_->Run();
  }

  void SetVolume(double volume) {
    IPC::Message msg;
    msg.set_routing_id(kRouteId);
    host_->OnSetVolume(msg, kStreamId, volume);
    message_loop_->RunAllPending();
  }

  void NotifyPacketReady() {
    EXPECT_CALL(*host_, OnRequestPacket(kRouteId, kStreamId, _))
        .WillOnce(QuitMessageLoop(message_loop_.get()));

    IPC::Message msg;
    msg.set_routing_id(kRouteId);
    memset(host_->shared_memory()->memory(), 0, host_->shared_memory_length());
    host_->OnNotifyPacketReady(msg, kStreamId,
                               host_->shared_memory_length());
    message_loop_->Run();
  }

  void SimulateError() {
    // Find the first AudioOutputController in the AudioRendererHost.
    CHECK(host_->audio_entries_.size())
        << "Calls Create() before calling this method";
    media::AudioOutputController* controller =
        host_->audio_entries_.begin()->second->controller;
    CHECK(controller) << "AudioOutputController not found";

    // Expect an error signal sent through IPC.
    EXPECT_CALL(*host_, OnStreamError(kRouteId, kStreamId));

    // Simulate an error sent from the audio device.
    host_->OnError(controller, 0);
    SyncWithAudioThread();

    // Expect the audio stream record is removed.
    EXPECT_EQ(0u, host_->audio_entries_.size());
  }

  // Called on the audio thread.
  static void PostQuitMessageLoop(MessageLoop* message_loop) {
    message_loop->PostTask(FROM_HERE, new MessageLoop::QuitTask());
  }

  // Called on the main thread.
  static void PostQuitOnAudioThread(MessageLoop* message_loop) {
    AudioManager::GetAudioManager()->GetMessageLoop()->PostTask(
        FROM_HERE, NewRunnableFunction(&PostQuitMessageLoop, message_loop));
  }

  // SyncWithAudioThread() waits until all pending tasks on the audio thread
  // are executed while also processing pending task in message_loop_ on the
  // current thread. It is used to synchronize with the audio thread when we are
  // closing an audio stream.
  void SyncWithAudioThread() {
    message_loop_->PostTask(
        FROM_HERE, NewRunnableFunction(&PostQuitOnAudioThread,
                                       message_loop_.get()));
    message_loop_->Run();
  }

  MessageLoop* message_loop() { return message_loop_.get(); }
  MockAudioRendererHost* host() { return host_; }
  void EnableRealDevice() { mock_stream_ = false; }

 private:
  bool mock_stream_;
  scoped_refptr<MockAudioRendererHost> host_;
  scoped_ptr<MessageLoop> message_loop_;
  scoped_ptr<BrowserThread> io_thread_;

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
