// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/render_messages.h"
#include "chrome/renderer/media/audio_renderer_impl.h"
#include "media/base/data_buffer.h"
#include "media/base/mock_filter_host.h"
#include "media/base/mock_filters.h"
#include "testing/gtest/include/gtest/gtest.h"

class AudioRendererImplTest : public ::testing::Test {
 public:
  static const int kRouteId = 0;
  static const int kSize = 1024;

  AudioRendererImplTest() {
    message_loop_.reset(new MessageLoop(MessageLoop::TYPE_IO));

    // TODO(scherkus): use gmock with AudioMessageFilter to verify
    // AudioRendererImpl calls or doesn't call Send().
    filter_ = new AudioMessageFilter(kRouteId);
    filter_->message_loop_ = message_loop_.get();

    // Create temporary shared memory.
    CHECK(shared_mem_.Create(L"", false, false, kSize));

    // Setup expectations for initialization.
    EXPECT_CALL(callback_, OnFilterCallback());
    EXPECT_CALL(callback_, OnCallbackDestroyed());
    decoder_ = new media::MockAudioDecoder(2, 48000, 16);

    // Create and initialize audio renderer.
    renderer_ = new AudioRendererImpl(filter_);
    renderer_->set_host(&host_);
    renderer_->set_message_loop(message_loop_.get());
    renderer_->Initialize(decoder_, callback_.NewCallback());

    // Run pending tasks and simulate responding with a created audio stream.
    message_loop_->RunAllPending();
    renderer_->OnCreated(shared_mem_.handle(), kSize);
  }

  virtual ~AudioRendererImplTest() {
  }

 protected:
  // Fixtures.
  scoped_ptr<MessageLoop> message_loop_;
  scoped_refptr<AudioMessageFilter> filter_;
  base::SharedMemory shared_mem_;
  media::MockFilterHost host_;
  media::MockFilterCallback callback_;
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

  message_loop_->RunAllPending();
  renderer_->Stop();
}

TEST_F(AudioRendererImplTest, SetVolume) {
  // Execute SetVolume() codepath to create an IPC message.
  renderer_->SetVolume(0.5f);
  message_loop_->RunAllPending();
  renderer_->Stop();
}

TEST_F(AudioRendererImplTest, Stop) {
  // Declare some state messages.
  const ViewMsg_AudioStreamState kError =
      { ViewMsg_AudioStreamState::kError };
  const ViewMsg_AudioStreamState kPlaying =
      { ViewMsg_AudioStreamState::kPlaying };
  const ViewMsg_AudioStreamState kPaused =
      { ViewMsg_AudioStreamState::kPaused };

  // Execute Stop() codepath to create an IPC message.
  renderer_->Stop();
  message_loop_->RunAllPending();

  // Run AudioMessageFilter::Delegate methods, which can be executed after being
  // stopped.  AudioRendererImpl shouldn't create any messages.
  renderer_->OnRequestPacket(kSize, base::Time());
  renderer_->OnStateChanged(kError);
  renderer_->OnStateChanged(kPlaying);
  renderer_->OnStateChanged(kPaused);
  renderer_->OnCreated(shared_mem_.handle(), kSize);
  renderer_->OnVolume(0.5);

  // It's possible that the upstream decoder replies right after being stopped.
  scoped_refptr<media::Buffer> buffer = new media::DataBuffer(kSize);
  renderer_->OnReadComplete(buffer);
}

TEST_F(AudioRendererImplTest, DestroyedMessageLoop_SetPlaybackRate) {
  // Kill the message loop and verify SetPlaybackRate() still works.
  message_loop_.reset();
  renderer_->SetPlaybackRate(0.0f);
  renderer_->SetPlaybackRate(1.0f);
  renderer_->SetPlaybackRate(0.0f);
  renderer_->Stop();
}

TEST_F(AudioRendererImplTest, DestroyedMessageLoop_SetVolume) {
  // Kill the message loop and verify SetVolume() still works.
  message_loop_.reset();
  renderer_->SetVolume(0.5f);
  renderer_->Stop();
}

TEST_F(AudioRendererImplTest, DestroyedMessageLoop_OnReadComplete) {
  // Kill the message loop and verify OnReadComplete() still works.
  message_loop_.reset();
  scoped_refptr<media::Buffer> buffer = new media::DataBuffer(kSize);
  renderer_->OnReadComplete(buffer);
  renderer_->Stop();
}
