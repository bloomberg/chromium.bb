// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/copresence/handlers/audio/audio_directive_handler.h"

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "components/copresence/mediums/audio/audio_player.h"
#include "components/copresence/mediums/audio/audio_recorder.h"
#include "components/copresence/test/audio_test_support.h"
#include "media/base/audio_bus.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Le;

namespace copresence {

class TestAudioPlayer : public AudioPlayer {
 public:
  TestAudioPlayer() {}
  virtual ~TestAudioPlayer() {}

  // AudioPlayer overrides:
  virtual void Initialize() OVERRIDE {}
  virtual void Play(
      const scoped_refptr<media::AudioBusRefCounted>& /* samples */) OVERRIDE {
    set_is_playing(true);
  }
  virtual void Stop() OVERRIDE { set_is_playing(false); }
  virtual void Finalize() OVERRIDE { delete this; }

 private:
  DISALLOW_COPY_AND_ASSIGN(TestAudioPlayer);
};

class TestAudioRecorder : public AudioRecorder {
 public:
  TestAudioRecorder() : AudioRecorder(AudioRecorder::DecodeSamplesCallback()) {}
  virtual ~TestAudioRecorder() {}

  // AudioRecorder overrides:
  virtual void Initialize() OVERRIDE {}
  virtual void Record() OVERRIDE { set_is_recording(true); }
  virtual void Stop() OVERRIDE { set_is_recording(false); }
  virtual void Finalize() OVERRIDE { delete this; }

 private:
  DISALLOW_COPY_AND_ASSIGN(TestAudioRecorder);
};

class AudioDirectiveHandlerTest : public testing::Test {
 public:
  AudioDirectiveHandlerTest()
      : directive_handler_(new AudioDirectiveHandler(
            AudioRecorder::DecodeSamplesCallback(),
            base::Bind(&AudioDirectiveHandlerTest::EncodeToken,
                       base::Unretained(this)))) {
    directive_handler_->set_player_audible_for_testing(new TestAudioPlayer());
    directive_handler_->set_player_inaudible_for_testing(new TestAudioPlayer());
    directive_handler_->set_recorder_for_testing(new TestAudioRecorder());
  }
  virtual ~AudioDirectiveHandlerTest() {}

  void DirectiveAdded() {}

 protected:
  void EncodeToken(const std::string& token,
                   bool audible,
                   const AudioDirectiveHandler::SamplesCallback& callback) {
    callback.Run(
        token, audible, CreateRandomAudioRefCounted(0x1337, 1, 0x7331));
  }

  copresence::TokenInstruction CreateTransmitInstruction(
      const std::string& token,
      bool audible) {
    copresence::TokenInstruction instruction;
    instruction.set_token_instruction_type(copresence::TRANSMIT);
    instruction.set_token_id(token);
    instruction.set_medium(audible ? AUDIO_AUDIBLE_DTMF
                                   : AUDIO_ULTRASOUND_PASSBAND);
    return instruction;
  }

  copresence::TokenInstruction CreateReceiveInstruction() {
    copresence::TokenInstruction instruction;
    instruction.set_token_instruction_type(copresence::RECEIVE);
    return instruction;
  }

  // This order is important. We want the message loop to get created before
  // our the audio directive handler since the directive list ctor (invoked
  // from the directive handler ctor) will post tasks.
  base::MessageLoop message_loop_;
  scoped_ptr<AudioDirectiveHandler> directive_handler_;

 private:
  DISALLOW_COPY_AND_ASSIGN(AudioDirectiveHandlerTest);
};

TEST_F(AudioDirectiveHandlerTest, Basic) {
  const base::TimeDelta kTtl = base::TimeDelta::FromMilliseconds(9999);
  directive_handler_->AddInstruction(
      CreateTransmitInstruction("token", true), "op_id1", kTtl);
  directive_handler_->AddInstruction(
      CreateTransmitInstruction("token", false), "op_id1", kTtl);
  directive_handler_->AddInstruction(
      CreateTransmitInstruction("token", false), "op_id2", kTtl);
  directive_handler_->AddInstruction(
      CreateReceiveInstruction(), "op_id1", kTtl);
  directive_handler_->AddInstruction(
      CreateReceiveInstruction(), "op_id2", kTtl);
  directive_handler_->AddInstruction(
      CreateReceiveInstruction(), "op_id3", kTtl);

  EXPECT_EQ(true, directive_handler_->player_audible_->IsPlaying());
  EXPECT_EQ(true, directive_handler_->player_inaudible_->IsPlaying());
  EXPECT_EQ(true, directive_handler_->recorder_->IsRecording());

  directive_handler_->RemoveInstructions("op_id1");
  EXPECT_EQ(false, directive_handler_->player_audible_->IsPlaying());
  EXPECT_EQ(true, directive_handler_->player_inaudible_->IsPlaying());
  EXPECT_EQ(true, directive_handler_->recorder_->IsRecording());

  directive_handler_->RemoveInstructions("op_id2");
  EXPECT_EQ(false, directive_handler_->player_inaudible_->IsPlaying());
  EXPECT_EQ(true, directive_handler_->recorder_->IsRecording());

  directive_handler_->RemoveInstructions("op_id3");
  EXPECT_EQ(false, directive_handler_->recorder_->IsRecording());
}

// TODO(rkc): Write more tests that check more convoluted sequences of
// transmits/receives.

}  // namespace copresence
