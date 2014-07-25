// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/services/gcm/push_messaging_application_id.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(PushMessagingApplicationIdTest, ConstructorValidity) {
  EXPECT_TRUE(gcm::PushMessagingApplicationId(GURL("https://www.example.com/"),
                                              1).IsValid());
  EXPECT_TRUE(gcm::PushMessagingApplicationId(GURL("https://www.example.com"),
                                              1).IsValid());
  EXPECT_FALSE(gcm::PushMessagingApplicationId(GURL(""), 1).IsValid());
  EXPECT_FALSE(gcm::PushMessagingApplicationId(GURL("foo"), 1).IsValid());
  EXPECT_FALSE(gcm::PushMessagingApplicationId(
                   GURL("https://www.example.com/foo"), 1).IsValid());
  EXPECT_FALSE(gcm::PushMessagingApplicationId(
                   GURL("https://www.example.com/#foo"), 1).IsValid());
  EXPECT_FALSE(gcm::PushMessagingApplicationId(GURL("https://www.example.com/"),
                                               -1).IsValid());
  EXPECT_FALSE(gcm::PushMessagingApplicationId().IsValid());
}

TEST(PushMessagingApplicationIdTest, ToString) {
  EXPECT_EQ(gcm::PushMessagingApplicationId(GURL("https://www.example.com/"), 1)
                .ToString(),
            "push#https://www.example.com/#1");
  EXPECT_EQ(gcm::PushMessagingApplicationId(GURL("https://www.example.com"), 1)
                .ToString(),
            "push#https://www.example.com/#1");
}

TEST(PushMessagingApplicationIdTest, ParseValidity) {
  EXPECT_TRUE(gcm::PushMessagingApplicationId::Parse(
                  "push#https://www.example.com/#1").IsValid());
  EXPECT_FALSE(gcm::PushMessagingApplicationId::Parse("").IsValid());
  EXPECT_FALSE(gcm::PushMessagingApplicationId::Parse(
                   "sync#https://www.example.com/#1").IsValid());
  EXPECT_FALSE(gcm::PushMessagingApplicationId::Parse("push#foo#1").IsValid());
  EXPECT_FALSE(gcm::PushMessagingApplicationId::Parse(
                   "push#https://www.example.com/foo#1").IsValid());
  EXPECT_FALSE(gcm::PushMessagingApplicationId::Parse(
                   "push#https://www.example.com/#one").IsValid());
  EXPECT_FALSE(gcm::PushMessagingApplicationId::Parse(
                   "push#https://www.example.com/#foo#1").IsValid());
  EXPECT_FALSE(gcm::PushMessagingApplicationId::Parse(
                   "push#https://www.example.com/#1#1").IsValid());
  EXPECT_FALSE(gcm::PushMessagingApplicationId::Parse(
                   "push#https://www.example.com/#-1").IsValid());
}
