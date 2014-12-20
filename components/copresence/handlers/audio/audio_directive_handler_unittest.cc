// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/timer/mock_timer.h"
#include "components/copresence/handlers/audio/audio_directive_handler_impl.h"
#include "components/copresence/handlers/audio/tick_clock_ref_counted.h"
#include "components/copresence/mediums/audio/audio_manager.h"
#include "components/copresence/proto/data.pb.h"
#include "components/copresence/test/audio_test_support.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace copresence {

namespace {

const Directive CreateDirective(TokenInstructionType type,
                                bool audible,
                                int64 ttl) {
  Directive directive;
  directive.mutable_token_instruction()->set_token_instruction_type(type);
  directive.mutable_token_instruction()->set_token_id("token");
  directive.mutable_token_instruction()->set_medium(audible ?
      AUDIO_AUDIBLE_DTMF : AUDIO_ULTRASOUND_PASSBAND);
  directive.set_ttl_millis(ttl);
  return directive;
}

}  // namespace

class AudioManagerStub final : public AudioManager {
 public:
  AudioManagerStub() {}
  ~AudioManagerStub() override {}

  // AudioManager overrides:
  void Initialize(WhispernetClient* whispernet_client,
                  const TokensCallback& tokens_cb) override {}
  void StartPlaying(AudioType type) override { playing_[type] = true; }
  void StopPlaying(AudioType type) override { playing_[type] = false; }
  void StartRecording(AudioType type) override { recording_[type] = true; }
  void StopRecording(AudioType type) override { recording_[type] = false; }
  void SetToken(AudioType type, const std::string& url_unsafe_token) override {}
  const std::string GetToken(AudioType type) override { return std::string(); }
  bool IsPlayingTokenHeard(AudioType type) override { return false; }

  bool IsRecording(AudioType type) { return recording_[type]; }
  bool IsPlaying(AudioType type) { return playing_[type]; }

 private:
  // Indexed using enum AudioType.
  bool playing_[2];
  bool recording_[2];

  DISALLOW_COPY_AND_ASSIGN(AudioManagerStub);
};

class AudioDirectiveHandlerTest : public testing::Test {
 public:
  AudioDirectiveHandlerTest() {
    manager_ptr_ = new AudioManagerStub;
    timer_ptr_ = new base::MockTimer(false, false);
    clock_ptr_ = new base::SimpleTestTickClock;

    directive_handler_.reset(new AudioDirectiveHandlerImpl(
        base::Bind(&AudioDirectiveHandlerTest::GetDirectiveUpdates,
                   base::Unretained(this)),
        make_scoped_ptr<AudioManager>(manager_ptr_),
        make_scoped_ptr<base::Timer>(timer_ptr_),
        make_scoped_refptr(new TickClockRefCounted(clock_ptr_))));
    directive_handler_->Initialize(nullptr, TokensCallback());
  }
  ~AudioDirectiveHandlerTest() override {}

 protected:
  const std::vector<Directive>& current_directives() {
    return current_directives_;
  }

  bool IsPlaying(AudioType type) { return manager_ptr_->IsPlaying(type); }

  bool IsRecording(AudioType type) { return manager_ptr_->IsRecording(type); }

  // This order is important. We want the message loop to get created before
  // our the audio directive handler since the directive list ctor (invoked
  // from the directive handler ctor) will post tasks.
  base::MessageLoop message_loop_;
  scoped_ptr<AudioDirectiveHandler> directive_handler_;

  std::vector<Directive> current_directives_;

  // Unowned.
  AudioManagerStub* manager_ptr_;
  base::MockTimer* timer_ptr_;
  base::SimpleTestTickClock* clock_ptr_;

 private:
  void GetDirectiveUpdates(const std::vector<Directive>& current_directives) {
    current_directives_ = current_directives;
  }

  DISALLOW_COPY_AND_ASSIGN(AudioDirectiveHandlerTest);
};

TEST_F(AudioDirectiveHandlerTest, Basic) {
  const int64 kTtl = 10;
  directive_handler_->AddInstruction(CreateDirective(TRANSMIT, true, kTtl),
                                     "op_id1");
  directive_handler_->AddInstruction(CreateDirective(TRANSMIT, false, kTtl),
                                     "op_id1");
  directive_handler_->AddInstruction(CreateDirective(TRANSMIT, false, kTtl),
                                     "op_id2");
  directive_handler_->AddInstruction(CreateDirective(RECEIVE, false, kTtl),
                                     "op_id1");
  directive_handler_->AddInstruction(CreateDirective(RECEIVE, true, kTtl),
                                     "op_id2");
  directive_handler_->AddInstruction(CreateDirective(RECEIVE, false, kTtl),
                                     "op_id3");

  EXPECT_TRUE(IsPlaying(AUDIBLE));
  EXPECT_TRUE(IsPlaying(INAUDIBLE));
  EXPECT_TRUE(IsRecording(AUDIBLE));
  EXPECT_TRUE(IsRecording(INAUDIBLE));

  directive_handler_->RemoveInstructions("op_id1");
  EXPECT_FALSE(IsPlaying(AUDIBLE));
  EXPECT_TRUE(IsPlaying(INAUDIBLE));
  EXPECT_TRUE(IsRecording(AUDIBLE));
  EXPECT_TRUE(IsRecording(INAUDIBLE));

  directive_handler_->RemoveInstructions("op_id2");
  EXPECT_FALSE(IsPlaying(INAUDIBLE));
  EXPECT_FALSE(IsRecording(AUDIBLE));
  EXPECT_TRUE(IsRecording(INAUDIBLE));

  directive_handler_->RemoveInstructions("op_id3");
  EXPECT_FALSE(IsRecording(INAUDIBLE));
}

TEST_F(AudioDirectiveHandlerTest, Timed) {
  directive_handler_->AddInstruction(CreateDirective(TRANSMIT, true, 6),
                                     "op_id1");
  directive_handler_->AddInstruction(CreateDirective(TRANSMIT, false, 8),
                                     "op_id1");
  directive_handler_->AddInstruction(CreateDirective(RECEIVE, false, 4),
                                     "op_id3");

  EXPECT_TRUE(IsPlaying(AUDIBLE));
  EXPECT_TRUE(IsPlaying(INAUDIBLE));
  EXPECT_FALSE(IsRecording(AUDIBLE));
  EXPECT_TRUE(IsRecording(INAUDIBLE));

  // Every time we advance and a directive expires, the timer should fire also.
  clock_ptr_->Advance(base::TimeDelta::FromMilliseconds(5));
  timer_ptr_->Fire();

  // We are now at +5ms. This instruction expires at +10ms.
  directive_handler_->AddInstruction(CreateDirective(RECEIVE, true, 5),
                                     "op_id4");
  EXPECT_TRUE(IsPlaying(AUDIBLE));
  EXPECT_TRUE(IsPlaying(INAUDIBLE));
  EXPECT_TRUE(IsRecording(AUDIBLE));
  EXPECT_FALSE(IsRecording(INAUDIBLE));

  // Advance to +7ms.
  const base::TimeDelta twoMs = base::TimeDelta::FromMilliseconds(2);
  clock_ptr_->Advance(twoMs);
  timer_ptr_->Fire();

  EXPECT_FALSE(IsPlaying(AUDIBLE));
  EXPECT_TRUE(IsPlaying(INAUDIBLE));
  EXPECT_TRUE(IsRecording(AUDIBLE));

  // Advance to +9ms.
  clock_ptr_->Advance(twoMs);
  timer_ptr_->Fire();
  EXPECT_FALSE(IsPlaying(INAUDIBLE));
  EXPECT_TRUE(IsRecording(AUDIBLE));

  // Advance to +11ms.
  clock_ptr_->Advance(twoMs);
  timer_ptr_->Fire();
  EXPECT_FALSE(IsRecording(AUDIBLE));
}

// TODO(rkc): Write more tests that check more convoluted sequences of
// transmits/receives.

}  // namespace copresence
