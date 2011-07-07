// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/process_util.h"
#include "content/common/media/audio_messages.h"
#include "content/renderer/media/audio_renderer_impl.h"
#include "media/base/data_buffer.h"
#include "media/base/mock_callback.h"
#include "media/base/mock_filter_host.h"
#include "media/base/mock_filters.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::Return;

// Class we would be tesing. The only difference between it and "real" one
// is that test class does not open sockets and launch audio thread.
class TestAudioRendererImpl : public AudioRendererImpl {
 public:
  explicit TestAudioRendererImpl(AudioMessageFilter* filter)
      : AudioRendererImpl(filter) {
  }
 private:
  virtual void CreateSocket(base::SyncSocket::Handle socket_handle) {}
  virtual void CreateAudioThread() {}
};

class AudioRendererImplTest : public ::testing::Test {
 public:
  static const int kRouteId = 0;
  static const int kSize = 1024;

  static void SetUpTestCase() {
    // Set low latency mode, as it soon would be on by default.
    AudioRendererImpl::set_latency_type(AudioRendererImpl::kLowLatency);
  }

  AudioRendererImplTest() {
    message_loop_.reset(new MessageLoop(MessageLoop::TYPE_IO));

    // TODO(scherkus): use gmock with AudioMessageFilter to verify
    // AudioRendererImpl calls or doesn't call Send().
    filter_ = new AudioMessageFilter(kRouteId);
    filter_->message_loop_ = message_loop_.get();

    // Create temporary shared memory.
    CHECK(shared_mem_.CreateAnonymous(kSize));

    // Setup expectations for initialization.
    decoder_ = new media::MockAudioDecoder();

    ON_CALL(*decoder_, config())
        .WillByDefault(Return(media::AudioDecoderConfig(16,
                                                        CHANNEL_LAYOUT_MONO,
                                                        44100)));

    // Create and initialize audio renderer.
    renderer_ = new TestAudioRendererImpl(filter_);
    renderer_->set_host(&host_);
    renderer_->Initialize(decoder_, media::NewExpectedCallback());

    // Run pending tasks and simulate responding with a created audio stream.
    message_loop_->RunAllPending();

    // Duplicate the shared memory handle so both the test and the callee can
    // close their copy.
    base::SharedMemoryHandle duplicated_handle;
    EXPECT_TRUE(shared_mem_.ShareToProcess(base::GetCurrentProcessHandle(),
                                           &duplicated_handle));
    CallOnCreated(duplicated_handle);
  }

  virtual ~AudioRendererImplTest() {
  }

  void CallOnCreated(base::SharedMemoryHandle handle) {
    if (renderer_->latency_type() == AudioRendererImpl::kHighLatency) {
      renderer_->OnCreated(handle, kSize);
    } else {
      renderer_->OnLowLatencyCreated(handle, 0, kSize);
    }
  }

 protected:
  // Fixtures.
  scoped_ptr<MessageLoop> message_loop_;
  scoped_refptr<AudioMessageFilter> filter_;
  base::SharedMemory shared_mem_;
  media::MockFilterHost host_;
  scoped_refptr<media::MockAudioDecoder> decoder_;
  scoped_refptr<AudioRendererImpl> renderer_;

 private:
  DISALLOW_COPY_AND_ASSIGN(AudioRendererImplTest);
};

TEST_F(AudioRendererImplTest, SetPlaybackRate) {
  // Execute SetPlaybackRate() codepath to create an IPC message.

  // Toggle play/pause to generate some IPC messages.
  renderer_->SetPlaybackRate(0.0f);
  renderer_->SetPlaybackRate(1.0f);
  renderer_->SetPlaybackRate(0.0f);

  renderer_->Stop(media::NewExpectedCallback());
  message_loop_->RunAllPending();
}

TEST_F(AudioRendererImplTest, SetVolume) {
  // Execute SetVolume() codepath to create an IPC message.
  renderer_->SetVolume(0.5f);
  renderer_->Stop(media::NewExpectedCallback());
  message_loop_->RunAllPending();
}

TEST_F(AudioRendererImplTest, Stop) {
  // Execute Stop() codepath to create an IPC message.
  renderer_->Stop(media::NewExpectedCallback());
  message_loop_->RunAllPending();

  // Run AudioMessageFilter::Delegate methods, which can be executed after being
  // stopped.  AudioRendererImpl shouldn't create any messages.
  if (renderer_->latency_type() == AudioRendererImpl::kHighLatency) {
    renderer_->OnRequestPacket(AudioBuffersState(kSize, 0));
  }
  renderer_->OnStateChanged(kAudioStreamError);
  renderer_->OnStateChanged(kAudioStreamPlaying);
  renderer_->OnStateChanged(kAudioStreamPaused);
  CallOnCreated(shared_mem_.handle());
  renderer_->OnVolume(0.5);

  // It's possible that the upstream decoder replies right after being stopped.
  scoped_refptr<media::Buffer> buffer(new media::DataBuffer(kSize));
  renderer_->ConsumeAudioSamples(buffer);
}

TEST_F(AudioRendererImplTest, DestroyedMessageLoop_SetPlaybackRate) {
  // Kill the message loop and verify SetPlaybackRate() still works.
  message_loop_.reset();
  renderer_->SetPlaybackRate(0.0f);
  renderer_->SetPlaybackRate(1.0f);
  renderer_->SetPlaybackRate(0.0f);
  renderer_->Stop(media::NewExpectedCallback());
}

TEST_F(AudioRendererImplTest, DestroyedMessageLoop_SetVolume) {
  // Kill the message loop and verify SetVolume() still works.
  message_loop_.reset();
  renderer_->SetVolume(0.5f);
  renderer_->Stop(media::NewExpectedCallback());
}

TEST_F(AudioRendererImplTest, DestroyedMessageLoop_ConsumeAudioSamples) {
  // Kill the message loop and verify OnReadComplete() still works.
  message_loop_.reset();
  scoped_refptr<media::Buffer> buffer(new media::DataBuffer(kSize));
  renderer_->ConsumeAudioSamples(buffer);
  renderer_->Stop(media::NewExpectedCallback());
}
