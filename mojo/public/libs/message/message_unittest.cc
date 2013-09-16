// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/libs/message/message.h"

#include <string>

#include "mojo/public/libs/message/message_builder.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {

TEST(Message, Empty) {
  Message message;
  EXPECT_EQ(0U, message.data().size());
}

TEST(Message, Builder_Empty) {
  MessageBuilder builder;
  EXPECT_TRUE(builder.Initialize(1));

  Message message;
  EXPECT_TRUE(builder.Finish(&message));

  EXPECT_EQ(message.data().size(), 8U);
}

TEST(Message, Builder_Basic) {
  MessageBuilder builder;
  EXPECT_TRUE(builder.Initialize(1));

  EXPECT_TRUE(builder.Append(1, 2U));

  Message message;
  EXPECT_TRUE(builder.Finish(&message));

  EXPECT_EQ(message.data().size(), 8U + 8U);
}

TEST(Message, Builder_BasicPair) {
  MessageBuilder builder;
  EXPECT_TRUE(builder.Initialize(1));

  EXPECT_TRUE(builder.Append(1, 100));
  EXPECT_TRUE(builder.Append(2, 200));

  Message message;
  EXPECT_TRUE(builder.Finish(&message));

  EXPECT_EQ(message.data().size(), 8U + 8U + 8U);
}

TEST(Message, Builder_BasicString) {
  MessageBuilder builder;
  EXPECT_TRUE(builder.Initialize(1));

  EXPECT_TRUE(builder.Append(1, 100));
  EXPECT_TRUE(builder.Append(2, 200));
  EXPECT_TRUE(builder.Append(3, std::string("hello world")));

  Message message;
  EXPECT_TRUE(builder.Finish(&message));

  EXPECT_EQ(message.data().size(), 8U + 8U + 8U + 8U + 16U);
}

TEST(Message, Builder_BasicHandle) {
  MessageBuilder builder;
  EXPECT_TRUE(builder.Initialize(1));

  mojo::Handle handle = { 1 };

  EXPECT_TRUE(builder.Append(1, handle));

  Message message;
  EXPECT_TRUE(builder.Finish(&message));

  EXPECT_EQ(message.data().size(), 8U + 8U);
  EXPECT_EQ(message.handles().size(), 1U);
}

}  // namespace mojo
