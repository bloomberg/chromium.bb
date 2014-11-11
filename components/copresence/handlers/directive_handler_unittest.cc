// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/time/time.h"
#include "components/copresence/handlers/audio/audio_directive_handler.h"
#include "components/copresence/handlers/directive_handler_impl.h"
#include "components/copresence/proto/data.pb.h"
#include "components/copresence/test/stub_whispernet_client.h"
#include "testing/gmock/include/gmock/gmock.h"

using testing::ElementsAre;
using testing::IsEmpty;

namespace {

const int64 kMaxUnlabeledTtl = 60000;  // 1 minute
const int64 kExcessiveUnlabeledTtl = 120000;  // 2 minutes
const int64 kDefaultTtl = 600000;  // 10 minutes

}  // namespace

namespace copresence {

Directive CreateDirective(const std::string& publish_id,
                          const std::string& subscribe_id,
                          const std::string& token,
                          int64 ttl_ms) {
  Directive directive;
  directive.set_instruction_type(TOKEN);
  directive.set_published_message_id(publish_id);
  directive.set_subscription_id(subscribe_id);
  directive.set_ttl_millis(ttl_ms);

  TokenInstruction* instruction = new TokenInstruction;
  instruction->set_token_id(token);
  instruction->set_medium(AUDIO_ULTRASOUND_PASSBAND);
  directive.set_allocated_token_instruction(instruction);

  return directive;
}

Directive CreateDirective(const std::string& publish_id,
                          const std::string& subscribe_id,
                          const std::string& token) {
  return CreateDirective(publish_id, subscribe_id, token, kDefaultTtl);
}

class FakeAudioDirectiveHandler final : public AudioDirectiveHandler {
 public:
  FakeAudioDirectiveHandler() {}

  void Initialize(WhispernetClient* /* whispernet_client */,
                  const TokensCallback& /* tokens_cb */) override {}

  void AddInstruction(const TokenInstruction& instruction,
                      const std::string& /* op_id */,
                      base::TimeDelta ttl) override {
    added_tokens_.push_back(instruction.token_id());
    added_ttls_.push_back(ttl.InMilliseconds());
  }

  void RemoveInstructions(const std::string& op_id) override {
    removed_operations_.push_back(op_id);
  }

  const std::string PlayingToken(AudioType /* type */) const override {
    NOTREACHED();
    return "";
  }

  bool IsPlayingTokenHeard(AudioType /* type */) const override {
    NOTREACHED();
    return false;
  }

  const std::vector<std::string>& added_tokens() const {
    return added_tokens_;
  }

  const std::vector<int64>& added_ttls() const {
    return added_ttls_;
  }

  const std::vector<std::string>& removed_operations() const {
    return removed_operations_;
  }

 private:
  std::vector<std::string> added_tokens_;
  std::vector<int64> added_ttls_;
  std::vector<std::string> removed_operations_;
};

class DirectiveHandlerTest : public testing::Test {
 public:
  DirectiveHandlerTest()
      : whispernet_client_(new StubWhispernetClient),
        audio_handler_(new FakeAudioDirectiveHandler),
        directive_handler_(
            make_scoped_ptr<AudioDirectiveHandler>(audio_handler_)) {}

 protected:
  void StartDirectiveHandler() {
    directive_handler_.Start(whispernet_client_.get(), TokensCallback());
  }

  scoped_ptr<WhispernetClient> whispernet_client_;
  FakeAudioDirectiveHandler* audio_handler_;
  DirectiveHandlerImpl directive_handler_;
};

TEST_F(DirectiveHandlerTest, DirectiveTtl) {
  StartDirectiveHandler();
  directive_handler_.AddDirective(
      CreateDirective("", "", "token 1", kMaxUnlabeledTtl));
  directive_handler_.AddDirective(
      CreateDirective("", "", "token 2", kExcessiveUnlabeledTtl));
  EXPECT_THAT(audio_handler_->added_ttls(),
      ElementsAre(kMaxUnlabeledTtl, kMaxUnlabeledTtl));
}

TEST_F(DirectiveHandlerTest, Queuing) {
  directive_handler_.AddDirective(CreateDirective("id 1", "", "token 1"));
  directive_handler_.AddDirective(CreateDirective("", "id 1", "token 2"));
  directive_handler_.AddDirective(CreateDirective("id 2", "", "token 3"));
  directive_handler_.RemoveDirectives("id 1");

  EXPECT_THAT(audio_handler_->added_tokens(), IsEmpty());
  EXPECT_THAT(audio_handler_->removed_operations(), IsEmpty());

  StartDirectiveHandler();
  directive_handler_.RemoveDirectives("id 3");

  EXPECT_THAT(audio_handler_->added_tokens(), ElementsAre("token 3"));
  EXPECT_THAT(audio_handler_->added_ttls(), ElementsAre(kDefaultTtl));
  EXPECT_THAT(audio_handler_->removed_operations(), ElementsAre("id 3"));
}

}  // namespace copresence
