// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/copresence/handlers/audio/audio_directive_handler.h"

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "components/copresence/test/audio_test_support.h"
#include "media/base/audio_bus.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Le;

namespace copresence {

class MockAudioDirectiveHandler : public AudioDirectiveHandler {
 public:
  MockAudioDirectiveHandler(
      const AudioDirectiveList::EncodeTokenCallback& encode_cb)
      : AudioDirectiveHandler(AudioRecorder::DecodeSamplesCallback(),
                              encode_cb) {}
  virtual ~MockAudioDirectiveHandler() {}

  // Mock out the play/record methods.
  MOCK_METHOD2(PlayAudio,
               void(const scoped_refptr<media::AudioBusRefCounted>&,
                    base::TimeDelta));
  MOCK_METHOD1(RecordAudio, void(base::TimeDelta));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockAudioDirectiveHandler);
};

class AudioDirectiveHandlerTest : public testing::Test {
 public:
  AudioDirectiveHandlerTest()
      : directive_handler_(new MockAudioDirectiveHandler(
            base::Bind(&AudioDirectiveHandlerTest::EncodeToken,
                       base::Unretained(this)))) {}

  virtual ~AudioDirectiveHandlerTest() {}

  void DirectiveAdded() {}

 protected:
  void EncodeToken(const std::string& token,
                   bool audible,
                   const AudioDirectiveList::SamplesCallback& callback) {
    callback.Run(
        token, audible, CreateRandomAudioRefCounted(0x1337, 1, 0x7331));
  }

  copresence::TokenInstruction CreateTransmitInstruction(
      const std::string& token) {
    copresence::TokenInstruction instruction;
    instruction.set_token_instruction_type(copresence::TRANSMIT);
    instruction.set_token_id(token);
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
  scoped_ptr<MockAudioDirectiveHandler> directive_handler_;

 private:
  DISALLOW_COPY_AND_ASSIGN(AudioDirectiveHandlerTest);
};

// TODO(rkc): This test is broken, possibly due to the changes for audible.
TEST_F(AudioDirectiveHandlerTest, DISABLED_Basic) {
  const base::TimeDelta kSmallTtl = base::TimeDelta::FromMilliseconds(0x1337);
  const base::TimeDelta kLargeTtl = base::TimeDelta::FromSeconds(0x7331);

  // Expect to play and record instructions for 'less' than the TTL specified,
  // since by the time that the token would have gotten encoded, we would
  // have (TTL - time_to_encode) left to play on that instruction.
  EXPECT_CALL(*directive_handler_, PlayAudio(_, testing::Le(kLargeTtl)))
      .Times(3);
  directive_handler_->AddInstruction(CreateTransmitInstruction("token1"),
                                     kLargeTtl);
  directive_handler_->AddInstruction(CreateTransmitInstruction("token2"),
                                     kLargeTtl);
  directive_handler_->AddInstruction(CreateTransmitInstruction("token3"),
                                     kSmallTtl);

  EXPECT_CALL(*directive_handler_, RecordAudio(Le(kLargeTtl))).Times(3);
  directive_handler_->AddInstruction(CreateReceiveInstruction(), kLargeTtl);
  directive_handler_->AddInstruction(CreateReceiveInstruction(), kSmallTtl);
  directive_handler_->AddInstruction(CreateReceiveInstruction(), kLargeTtl);
}

// TODO(rkc): When we are keeping track of which token we're currently playing,
// add tests to make sure we don't replay if we get a token with a lower ttl
// than the current active.

}  // namespace copresence
