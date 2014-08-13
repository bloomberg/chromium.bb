// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/copresence/handlers/audio/audio_directive_list.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/message_loop/message_loop.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace copresence {

class AudioDirectiveListTest : public testing::Test {
 public:
  AudioDirectiveListTest() : directive_list_(new AudioDirectiveList()) {}

  virtual ~AudioDirectiveListTest() {}

 protected:
  base::MessageLoop message_loop_;
  scoped_ptr<AudioDirectiveList> directive_list_;
};

TEST_F(AudioDirectiveListTest, Basic) {
  const base::TimeDelta kTtl = base::TimeDelta::FromSeconds(9999);

  EXPECT_EQ(NULL, directive_list_->GetActiveDirective().get());

  directive_list_->AddDirective("op_id1", kTtl);
  directive_list_->AddDirective("op_id2", kTtl * 3);
  directive_list_->AddDirective("op_id3", kTtl * 2);
  EXPECT_EQ("op_id2", directive_list_->GetActiveDirective()->op_id);

  directive_list_->RemoveDirective("op_id2");
  EXPECT_EQ("op_id3", directive_list_->GetActiveDirective()->op_id);
}

TEST_F(AudioDirectiveListTest, AddDirectiveMultiple) {
  const base::TimeDelta kTtl = base::TimeDelta::FromSeconds(9999);

  directive_list_->AddDirective("op_id1", kTtl);
  directive_list_->AddDirective("op_id2", kTtl * 2);
  directive_list_->AddDirective("op_id3", kTtl * 3 * 2);
  directive_list_->AddDirective("op_id3", kTtl * 3 * 3);
  directive_list_->AddDirective("op_id4", kTtl * 4);

  EXPECT_EQ("op_id3", directive_list_->GetActiveDirective()->op_id);
  directive_list_->RemoveDirective("op_id3");
  EXPECT_EQ("op_id4", directive_list_->GetActiveDirective()->op_id);
  directive_list_->RemoveDirective("op_id4");
  EXPECT_EQ("op_id2", directive_list_->GetActiveDirective()->op_id);
  directive_list_->RemoveDirective("op_id2");
  EXPECT_EQ("op_id1", directive_list_->GetActiveDirective()->op_id);
  directive_list_->RemoveDirective("op_id1");
  EXPECT_EQ(NULL, directive_list_->GetActiveDirective().get());
}

TEST_F(AudioDirectiveListTest, RemoveDirectiveMultiple) {
  const base::TimeDelta kTtl = base::TimeDelta::FromSeconds(9999);

  directive_list_->AddDirective("op_id1", kTtl);
  directive_list_->AddDirective("op_id2", kTtl * 2);
  directive_list_->AddDirective("op_id3", kTtl * 3);
  directive_list_->AddDirective("op_id4", kTtl * 4);

  EXPECT_EQ("op_id4", directive_list_->GetActiveDirective()->op_id);
  directive_list_->RemoveDirective("op_id4");
  EXPECT_EQ("op_id3", directive_list_->GetActiveDirective()->op_id);
  directive_list_->RemoveDirective("op_id3");
  directive_list_->RemoveDirective("op_id3");
  directive_list_->RemoveDirective("op_id3");
  EXPECT_EQ("op_id2", directive_list_->GetActiveDirective()->op_id);
  directive_list_->RemoveDirective("op_id2");
  EXPECT_EQ("op_id1", directive_list_->GetActiveDirective()->op_id);
  directive_list_->RemoveDirective("op_id1");
  EXPECT_EQ(NULL, directive_list_->GetActiveDirective().get());
}

}  // namespace copresence
