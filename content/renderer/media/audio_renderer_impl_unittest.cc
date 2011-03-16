// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/process_util.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/render_messages_params.h"
#include "content/renderer/media/audio_renderer_impl.h"
#include "media/base/data_buffer.h"
#include "media/base/media_format.h"
#include "media/base/mock_callback.h"
#include "media/base/mock_filter_host.h"
#include "media/base/mock_filters.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::ReturnRef;

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
    CHECK(shared_mem_.CreateAnonymous(kSize));

    // Setup expectations for initialization.
    decoder_ = new media::MockAudioDecoder();

    // Associate media format with decoder
    decoder_media_format_.SetAsInteger(media::MediaFormat::kChannels, 2);
    decoder_media_format_.SetAsInteger(media::MediaFormat::kSampleRate, 48000);
    decoder_media_format_.SetAsInteger(media::MediaFormat::kSampleBits, 16);
    EXPECT_CALL(*decoder_, media_format())
        .WillRepeatedly(ReturnRef(decoder_media_format_));

    // Create and initialize audio renderer.
    renderer_ = new AudioRendererImpl(filter_);
    renderer_->set_host(&host_);
    renderer_->Initialize(decoder_, media::NewExpectedCallback());

    // Run pending tasks and simulate responding with a created audio stream.
    message_loop_->RunAllPending();

    // Duplicate the shared memory handle so both the test and the callee can
    // close their copy.
    base::SharedMemoryHandle duplicated_handle;
    EXPECT_TRUE(shared_mem_.ShareToProcess(base::GetCurrentProcessHandle(),
                                           &duplicated_handle));

    renderer_->OnCreated(duplicated_handle, kSize);
  }

  virtual ~AudioRendererImplTest() {
  }

 protected:
  // Fixtures.
  scoped_ptr<MessageLoop> message_loop_;
  scoped_refptr<AudioMessageFilter> filter_;
  base::SharedMemory shared_mem_;
  media::MockFilterHost host_;
  scoped_refptr<media::MockAudioDecoder> decoder_;
  scoped_refptr<AudioRendererImpl> renderer_;
  media::MediaFormat decoder_media_format_;

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
  // Declare some state messages.
  const ViewMsg_AudioStreamState_Params kError(
      ViewMsg_AudioStreamState_Params::kError);
  const ViewMsg_AudioStreamState_Params kPlaying(
      ViewMsg_AudioStreamState_Params::kPlaying);
  const ViewMsg_AudioStreamState_Params kPaused(
      ViewMsg_AudioStreamState_Params::kPaused);

  // Execute Stop() codepath to create an IPC message.
  renderer_->Stop(media::NewExpectedCallback());
  message_loop_->RunAllPending();

  // Run AudioMessageFilter::Delegate methods, which can be executed after being
  // stopped.  AudioRendererImpl shouldn't create any messages.
  renderer_->OnRequestPacket(AudioBuffersState(kSize, 0));
  renderer_->OnStateChanged(kError);
  renderer_->OnStateChanged(kPlaying);
  renderer_->OnStateChanged(kPaused);
  renderer_->OnCreated(shared_mem_.handle(), kSize);
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
