// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/copresence/handlers/audio/audio_directive_list.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/message_loop/message_loop.h"
#include "components/copresence/test/audio_test_support.h"
#include "media/base/audio_bus.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace copresence {

class AudioDirectiveListTest : public testing::Test {
 public:
  AudioDirectiveListTest()
      : directive_list_(new AudioDirectiveList(
            base::Bind(&AudioDirectiveListTest::EncodeToken,
                       base::Unretained(this)),
            base::Bind(&base::DoNothing),
            false)) {}

  virtual ~AudioDirectiveListTest() {}

 protected:
  void EncodeToken(const std::string& token,
                   bool /* audible */,
                   const AudioDirectiveList::SamplesCallback& callback) {
    callback.Run(token, CreateRandomAudioRefCounted(0x1337, 1, 0x7331));
  }

  base::MessageLoop message_loop_;
  scoped_ptr<AudioDirectiveList> directive_list_;
};

// TODO(rkc): Find and fix the memory leak here.
#define MAYBE_Basic DISABLED_Basic

TEST_F(AudioDirectiveListTest, MAYBE_Basic) {
  const base::TimeDelta kZeroTtl = base::TimeDelta::FromMilliseconds(0);
  const base::TimeDelta kLargeTtl = base::TimeDelta::FromSeconds(0x7331);

  directive_list_->AddTransmitDirective("token1", "op_id1", kZeroTtl);
  directive_list_->AddTransmitDirective("token2", "op_id2", kLargeTtl);
  directive_list_->AddTransmitDirective("token3", "op_id1", kZeroTtl);

  EXPECT_EQ("token2", directive_list_->GetNextTransmit()->token);

  directive_list_->AddReceiveDirective("op_id1", kZeroTtl);
  directive_list_->AddReceiveDirective("op_id3", kZeroTtl);
  directive_list_->AddReceiveDirective("op_id3", kLargeTtl);
  directive_list_->AddReceiveDirective("op_id7", kZeroTtl);

  EXPECT_EQ("op_id3", directive_list_->GetNextReceive()->op_id);
}

// TODO(rkc): Find out why this is breaking on bots and fix it.
#define MAYBE_OutOfOrderAndMultiple DISABLED_OutOfOrderAndMultiple

TEST_F(AudioDirectiveListTest, MAYBE_OutOfOrderAndMultiple) {
  const base::TimeDelta kZeroTtl = base::TimeDelta::FromMilliseconds(0);
  const base::TimeDelta kLargeTtl = base::TimeDelta::FromSeconds(0x7331);

  EXPECT_EQ(NULL, directive_list_->GetNextTransmit().get());
  EXPECT_EQ(NULL, directive_list_->GetNextReceive().get());

  directive_list_->AddTransmitDirective("token1", "op_id1", kZeroTtl);
  directive_list_->AddTransmitDirective("token2", "op_id2", kLargeTtl);
  directive_list_->AddTransmitDirective("token3", "op_id1", kLargeTtl);

  // Should keep getting the directive till it expires or we add a newer one.
  EXPECT_EQ("token3", directive_list_->GetNextTransmit()->token);
  EXPECT_EQ("token3", directive_list_->GetNextTransmit()->token);
  EXPECT_EQ("token3", directive_list_->GetNextTransmit()->token);
  EXPECT_EQ(NULL, directive_list_->GetNextReceive().get());

  directive_list_->AddReceiveDirective("op_id1", kLargeTtl);
  directive_list_->AddReceiveDirective("op_id3", kZeroTtl);
  directive_list_->AddReceiveDirective("op_id3", kLargeTtl);
  directive_list_->AddReceiveDirective("op_id7", kLargeTtl);

  // Should keep getting the directive till it expires or we add a newer one.
  EXPECT_EQ("op_id7", directive_list_->GetNextReceive()->op_id);
  EXPECT_EQ("op_id7", directive_list_->GetNextReceive()->op_id);
  EXPECT_EQ("op_id7", directive_list_->GetNextReceive()->op_id);
}

}  // namespace copresence
