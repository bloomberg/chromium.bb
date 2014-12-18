// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/copresence/mediums/audio/audio_manager.h"

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "components/copresence/mediums/audio/audio_manager_impl.h"
#include "components/copresence/mediums/audio/audio_player.h"
#include "components/copresence/mediums/audio/audio_recorder.h"
#include "components/copresence/test/stub_whispernet_client.h"
#include "media/base/audio_bus.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace copresence {

class AudioPlayerStub final : public AudioPlayer {
 public:
  AudioPlayerStub() : is_playing_(false) {}
  ~AudioPlayerStub() override {}

  // AudioPlayer overrides:
  void Initialize() override {}
  void Play(const scoped_refptr<media::AudioBusRefCounted>&) override {
    is_playing_ = true;
  }
  void Stop() override { is_playing_ = false; }
  void Finalize() override { delete this; }

  bool IsPlaying() { return is_playing_; }

 private:
  bool is_playing_;
  DISALLOW_COPY_AND_ASSIGN(AudioPlayerStub);
};

class AudioRecorderStub final : public AudioRecorder {
 public:
  AudioRecorderStub() : is_recording_(false) {}
  ~AudioRecorderStub() override {}

  // AudioRecorder overrides:
  void Initialize(const RecordedSamplesCallback& cb) override { cb_ = cb; }
  void Record() override { is_recording_ = true; }
  void Stop() override { is_recording_ = false; }
  void Finalize() override { delete this; }

  bool IsRecording() { return is_recording_; }

  void TriggerDecodeRequest() {
    if (!cb_.is_null())
      cb_.Run(std::string(0x1337, 'a'));
  }

 private:
  RecordedSamplesCallback cb_;
  bool is_recording_;

  DISALLOW_COPY_AND_ASSIGN(AudioRecorderStub);
};

class AudioManagerTest : public testing::Test {
 public:
  AudioManagerTest()
      : whispernet_client_(new StubWhispernetClient),
        audio_manager_(new AudioManagerImpl()),
        audible_player_(new AudioPlayerStub),
        inaudible_player_(new AudioPlayerStub),
        recorder_(new AudioRecorderStub),
        last_received_decode_type_(AUDIO_TYPE_UNKNOWN) {
    audio_manager_->set_player_for_testing(AUDIBLE, audible_player_);
    audio_manager_->set_player_for_testing(INAUDIBLE, inaudible_player_);
    audio_manager_->set_recorder_for_testing(recorder_);
    audio_manager_->Initialize(
        whispernet_client_.get(),
        base::Bind(&AudioManagerTest::GetTokens, base::Unretained(this)));
  }

  ~AudioManagerTest() override {}

 protected:
  void GetTokens(const std::vector<AudioToken>& tokens) {
    last_received_decode_type_ = AUDIO_TYPE_UNKNOWN;
    for (const auto& token : tokens) {
      if (token.audible && last_received_decode_type_ == INAUDIBLE) {
        last_received_decode_type_ = BOTH;
      } else if (!token.audible && last_received_decode_type_ == AUDIBLE) {
        last_received_decode_type_ = BOTH;
      } else if (token.audible) {
        last_received_decode_type_ = AUDIBLE;
      } else {
        last_received_decode_type_ = INAUDIBLE;
      }
    }
  }

  base::MessageLoop message_loop_;
  // Order is important, |whispernet_client_| needs to get destructed *after*
  // |audio_manager_|.
  scoped_ptr<WhispernetClient> whispernet_client_;
  scoped_ptr<AudioManagerImpl> audio_manager_;

  // These will be deleted by |audio_manager_|'s dtor calling finalize on them.
  AudioPlayerStub* audible_player_;
  AudioPlayerStub* inaudible_player_;
  AudioRecorderStub* recorder_;

  AudioType last_received_decode_type_;

 private:
  DISALLOW_COPY_AND_ASSIGN(AudioManagerTest);
};

TEST_F(AudioManagerTest, EncodeToken) {
  audio_manager_->StartPlaying(AUDIBLE);
  // No token yet, player shouldn't be playing.
  EXPECT_FALSE(audible_player_->IsPlaying());

  audio_manager_->SetToken(INAUDIBLE, "abcd");
  // No *audible* token yet, so player still shouldn't be playing.
  EXPECT_FALSE(audible_player_->IsPlaying());

  audio_manager_->SetToken(AUDIBLE, "abcd");
  EXPECT_TRUE(audible_player_->IsPlaying());
}

TEST_F(AudioManagerTest, Record) {
  recorder_->TriggerDecodeRequest();
  EXPECT_EQ(AUDIO_TYPE_UNKNOWN, last_received_decode_type_);

  audio_manager_->StartRecording(AUDIBLE);
  recorder_->TriggerDecodeRequest();
  EXPECT_EQ(AUDIBLE, last_received_decode_type_);

  audio_manager_->StartRecording(INAUDIBLE);
  recorder_->TriggerDecodeRequest();
  EXPECT_EQ(BOTH, last_received_decode_type_);

  audio_manager_->StopRecording(AUDIBLE);
  recorder_->TriggerDecodeRequest();
  EXPECT_EQ(INAUDIBLE, last_received_decode_type_);
}

}  // namespace copresence
