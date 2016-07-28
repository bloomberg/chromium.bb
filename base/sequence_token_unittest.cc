// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/sequence_token.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace base {

TEST(SequenceTokenTest, IsValid) {
  EXPECT_FALSE(SequenceToken().IsValid());
  EXPECT_TRUE(SequenceToken::Create().IsValid());
}

TEST(SequenceTokenTest, OperatorEquals) {
  SequenceToken invalid_a;
  SequenceToken invalid_b;
  const SequenceToken valid_a = SequenceToken::Create();
  const SequenceToken valid_b = SequenceToken::Create();

  EXPECT_FALSE(invalid_a == invalid_a);
  EXPECT_FALSE(invalid_a == invalid_b);
  EXPECT_FALSE(invalid_a == valid_a);
  EXPECT_FALSE(invalid_a == valid_b);

  EXPECT_FALSE(valid_a == invalid_a);
  EXPECT_FALSE(valid_a == invalid_b);
  EXPECT_EQ(valid_a, valid_a);
  EXPECT_FALSE(valid_a == valid_b);
}

TEST(SequenceTokenTest, OperatorNotEquals) {
  SequenceToken invalid_a;
  SequenceToken invalid_b;
  const SequenceToken valid_a = SequenceToken::Create();
  const SequenceToken valid_b = SequenceToken::Create();

  EXPECT_NE(invalid_a, invalid_a);
  EXPECT_NE(invalid_a, invalid_b);
  EXPECT_NE(invalid_a, valid_a);
  EXPECT_NE(invalid_a, valid_b);

  EXPECT_NE(valid_a, invalid_a);
  EXPECT_NE(valid_a, invalid_b);
  EXPECT_FALSE(valid_a != valid_a);
  EXPECT_NE(valid_a, valid_b);
}

TEST(SequenceTokenTest, GetForCurrentThread) {
  const SequenceToken token = SequenceToken::Create();

  EXPECT_FALSE(SequenceToken::GetForCurrentThread().IsValid());

  {
    ScopedSetSequenceTokenForCurrentThread
        scoped_set_sequence_token_for_current_thread(token);
    EXPECT_TRUE(SequenceToken::GetForCurrentThread().IsValid());
    EXPECT_EQ(token, SequenceToken::GetForCurrentThread());
  }

  EXPECT_FALSE(SequenceToken::GetForCurrentThread().IsValid());
}

}  // namespace base
