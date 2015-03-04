// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/push_messaging/push_messaging_application_id.h"
#include "testing/gtest/include/gtest/gtest.h"

class PushMessagingApplicationIdTest : public testing::Test {
 protected:
  PushMessagingApplicationId GenerateId(
      const GURL& origin,
      int64 service_worker_registration_id) {
    // To bypass DCHECK in PushMessagingApplicationId::Generate, we just use it
    // to generate app_id_guid, and then use private constructor.
    std::string app_id_guid = PushMessagingApplicationId::Generate(
        GURL("https://www.example.com/"), 1).app_id_guid();
    return PushMessagingApplicationId(app_id_guid, origin,
                                      service_worker_registration_id);
  }
};

TEST_F(PushMessagingApplicationIdTest, ConstructorValidity) {
  EXPECT_TRUE(GenerateId(GURL("https://www.example.com/"), 1).IsValid());
  EXPECT_TRUE(GenerateId(GURL("https://www.example.com"), 1).IsValid());
  EXPECT_FALSE(GenerateId(GURL(""), 1).IsValid());
  EXPECT_FALSE(GenerateId(GURL("foo"), 1).IsValid());
  EXPECT_FALSE(GenerateId(GURL("https://www.example.com/foo"), 1).IsValid());
  EXPECT_FALSE(GenerateId(GURL("https://www.example.com/#foo"), 1).IsValid());
  EXPECT_FALSE(GenerateId(GURL("https://www.example.com/"), -1).IsValid());
}

TEST_F(PushMessagingApplicationIdTest, UniqueGuids) {
  EXPECT_NE(PushMessagingApplicationId::Generate(
                GURL("https://www.example.com/"), 1).app_id_guid(),
            PushMessagingApplicationId::Generate(
                GURL("https://www.example.com/"), 1).app_id_guid());
}
