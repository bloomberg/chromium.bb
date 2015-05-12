// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/push_messaging/push_messaging_app_identifier.h"
#include "testing/gtest/include/gtest/gtest.h"

class PushMessagingAppIdentifierTest : public testing::Test {
 protected:
  PushMessagingAppIdentifier GenerateId(
      const GURL& origin,
      int64_t service_worker_registration_id) {
    // To bypass DCHECK in PushMessagingAppIdentifier::Generate, we just use it
    // to generate app_id, and then use private constructor.
    std::string app_id = PushMessagingAppIdentifier::Generate(
        GURL("https://www.example.com/"), 1).app_id();
    return PushMessagingAppIdentifier(app_id, origin,
                                      service_worker_registration_id);
  }
};

TEST_F(PushMessagingAppIdentifierTest, ConstructorValidity) {
  // The following two are valid:
  EXPECT_FALSE(GenerateId(GURL("https://www.example.com/"), 1).is_null());
  EXPECT_FALSE(GenerateId(GURL("https://www.example.com"), 1).is_null());
  // The following four are invalid and will DCHECK in Generate:
  EXPECT_FALSE(GenerateId(GURL(""), 1).is_null());
  EXPECT_FALSE(GenerateId(GURL("foo"), 1).is_null());
  EXPECT_FALSE(GenerateId(GURL("https://www.example.com/foo"), 1).is_null());
  EXPECT_FALSE(GenerateId(GURL("https://www.example.com/#foo"), 1).is_null());
  // The following one is invalid and will DCHECK in Generate and be null:
  EXPECT_TRUE(GenerateId(GURL("https://www.example.com/"), -1).is_null());
}

TEST_F(PushMessagingAppIdentifierTest, UniqueGuids) {
  EXPECT_NE(PushMessagingAppIdentifier::Generate(
                GURL("https://www.example.com/"), 1).app_id(),
            PushMessagingAppIdentifier::Generate(
                GURL("https://www.example.com/"), 1).app_id());
}
