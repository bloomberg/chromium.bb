// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop.h"
#include "base/process_util.h"
#include "base/scoped_ptr.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/renderer_host/audio_renderer_host.h"
#include "chrome/common/render_messages.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::DoAll;
using ::testing::InSequence;
using ::testing::Return;
using ::testing::SaveArg;
using ::testing::SetArgumentPointee;

namespace {

const int kInvalidId = -1;
const int kProcessId = 100;
const int kRouteId = 200;
const int kBufferCapacity = 65536;
const int kPacketSize = 16384;

}  // namespace

class MockAudioRendererHost : public AudioRendererHost {
 public:
  MockAudioRendererHost()
      : AudioRendererHost() {
  }

  virtual ~MockAudioRendererHost() {
  }

  // A list of mock methods.
  MOCK_METHOD4(OnRequestPacket,
               void(int routing_id, int stream_id,
                    size_t bytes_in_buffer, int64 message_timestamp));

  MOCK_METHOD3(OnStreamCreated,
               void(int routing_id, int stream_id, int length));

  MOCK_METHOD3(OnStreamStateChanged,
               void(int routing_id, int stream_id,
                    ViewMsg_AudioStreamState state));

  MOCK_METHOD4(OnStreamVolume,
               void(int routing_id, int stream_id, double left, double right));

  base::SharedMemory* shared_memory() { return shared_memory_.get(); }

 protected:
  // This method is used to dispatch IPC messages to the renderer. We intercept
  // these messages here and dispatch to our mock methods to verify the
  // conversation between this object and the renderer.
  virtual void Send(IPC::Message* message) {
    CHECK(message);

    // In this method we dispatch the messages to the according handlers as if
    // we are the renderer.
    bool handled = true;
    IPC_BEGIN_MESSAGE_MAP(MockAudioRendererHost, *message)
      IPC_MESSAGE_HANDLER(ViewMsg_RequestAudioPacket, OnRequestPacket)
      IPC_MESSAGE_HANDLER(ViewMsg_NotifyAudioStreamCreated, OnStreamCreated)
      IPC_MESSAGE_HANDLER(ViewMsg_NotifyAudioStreamStateChanged,
                          OnStreamStateChanged)
      IPC_MESSAGE_HANDLER(ViewMsg_NotifyAudioStreamVolume, OnStreamVolume)
      IPC_MESSAGE_UNHANDLED(handled = false)
    IPC_END_MESSAGE_MAP()
    EXPECT_TRUE(handled);

    delete message;
  }

 private:
  // These handler methods do minimal things and delegate to the mock methods.
  void OnRequestPacket(const IPC::Message& msg, int stream_id,
                       size_t bytes_in_buffer, int64 message_timestamp) {
    OnRequestPacket(msg.routing_id(), stream_id, bytes_in_buffer,
                    message_timestamp);
  }

  void OnStreamCreated(const IPC::Message& msg, int stream_id,
                       base::SharedMemoryHandle handle, int length) {
    // Maps the shared memory.
    shared_memory_.reset(new base::SharedMemory(handle, true));
    CHECK(shared_memory_->Map(length));
    CHECK(shared_memory_->memory());

    // And then delegate the call to the mock method.
    OnStreamCreated(msg.routing_id(), stream_id, length);
  }

  void OnStreamStateChanged(const IPC::Message& msg, int stream_id,
                            ViewMsg_AudioStreamState state) {
    OnStreamStateChanged(msg.routing_id(), stream_id, state);
  }

  void OnStreamVolume(const IPC::Message& msg, int stream_id,
                      double left, double right) {
    OnStreamVolume(msg.routing_id(), stream_id, left, right);
  }

  scoped_ptr<base::SharedMemory> shared_memory_;

  DISALLOW_COPY_AND_ASSIGN(MockAudioRendererHost);
};

class AudioRendererHostTest : public testing::Test {
 public:
  AudioRendererHostTest()
      : current_stream_id_(0) {
  }

 protected:
  virtual void SetUp() {
    // Create a message loop so AudioRendererHost can use it.
    message_loop_.reset(new MessageLoop(MessageLoop::TYPE_IO));
    io_thread_.reset(new ChromeThread(ChromeThread::IO, message_loop_.get()));
    host_ = new MockAudioRendererHost();
  }

  virtual void TearDown() {
    // This task post a task to message_loop_ to do internal destruction on
    // message_loop_.
    host_->Destroy();

    // Release the reference to the mock object.
    host_ = NULL;

    // We need to continue running message_loop_ to complete all destructions.
    message_loop_->RunAllPending();

    io_thread_.reset();
  }

  AudioRendererHost::IPCAudioSource* CreateAudioStream(
      AudioManager::Format format) {
    InSequence s;

    // 1. We will first receive a OnStreamCreated() signal.
    EXPECT_CALL(*host_,
                OnStreamCreated(kRouteId, current_stream_id_, kPacketSize));

    // 2. First packet request will arrive. This request is sent by
    //    IPCAudioSource::CreateIPCAudioSource to start buffering.
    EXPECT_CALL(*host_, OnRequestPacket(kRouteId, current_stream_id_, 0, _));

    AudioRendererHost::IPCAudioSource* source =
        AudioRendererHost::IPCAudioSource::CreateIPCAudioSource(
            host_,
            kProcessId,
            kRouteId,
            current_stream_id_,
            base::GetCurrentProcessHandle(),
            format,
            2,
            AudioManager::kAudioCDSampleRate,
            16,
            kPacketSize,
            kBufferCapacity);
    EXPECT_TRUE(source);
    EXPECT_EQ(kProcessId, source->process_id());
    EXPECT_EQ(kRouteId, source->route_id());
    EXPECT_EQ(current_stream_id_, source->stream_id());
    return source;
  }

  AudioRendererHost::IPCAudioSource* CreateRealStream() {
    return CreateAudioStream(AudioManager::AUDIO_PCM_LINEAR);
  }

  AudioRendererHost::IPCAudioSource* CreateMockStream() {
    return CreateAudioStream(AudioManager::AUDIO_MOCK);
  }

  int current_stream_id_;
  scoped_refptr<MockAudioRendererHost> host_;
  scoped_ptr<MessageLoop> message_loop_;

 private:
  scoped_ptr<ChromeThread> io_thread_;
  DISALLOW_COPY_AND_ASSIGN(AudioRendererHostTest);
};

TEST_F(AudioRendererHostTest, MockStreamDataConversation) {
  scoped_ptr<AudioRendererHost::IPCAudioSource> source(CreateMockStream());

  // We will receive packet requests until the buffer is full. We first send
  // three packets of 16KB, then we send packets of 1KB until the buffer is
  // full. Then there will no more packet requests.
  InSequence s;
  EXPECT_CALL(*host_,
      OnRequestPacket(kRouteId, current_stream_id_, kPacketSize, _));
  EXPECT_CALL(*host_,
    OnRequestPacket(kRouteId, current_stream_id_, 2 * kPacketSize, _));

  const int kStep = kPacketSize / 4;
  EXPECT_CALL(*host_,
      OnRequestPacket(kRouteId, current_stream_id_,
                      2 * kPacketSize + kStep, _));
  EXPECT_CALL(*host_,
      OnRequestPacket(kRouteId, current_stream_id_,
                      2 * kPacketSize + 2 * kStep, _));
  EXPECT_CALL(*host_,
      OnRequestPacket(kRouteId, current_stream_id_,
                      2 * kPacketSize + 3 * kStep, _));
  EXPECT_CALL(*host_,
      OnRequestPacket(kRouteId, current_stream_id_, 3 * kPacketSize, _));
  ViewMsg_AudioStreamState state;
  EXPECT_CALL(*host_, OnStreamStateChanged(kRouteId, current_stream_id_, _))
      .WillOnce(SaveArg<2>(&state));

  source->NotifyPacketReady(kPacketSize);
  source->NotifyPacketReady(kPacketSize);
  source->NotifyPacketReady(kStep);
  source->NotifyPacketReady(kStep);
  source->NotifyPacketReady(kStep);
  source->NotifyPacketReady(kStep);
  source->Play();
  EXPECT_EQ(ViewMsg_AudioStreamState::kPlaying, state.state);
  source->Close();
}
