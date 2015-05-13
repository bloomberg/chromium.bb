// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/push_messaging/push_messaging_app_identifier.h"

#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

void ExpectAppIdentifiersEqual(const PushMessagingAppIdentifier& a,
                               const PushMessagingAppIdentifier& b) {
  EXPECT_EQ(a.app_id(), b.app_id());
  EXPECT_EQ(a.origin(), b.origin());
  EXPECT_EQ(a.service_worker_registration_id(),
            b.service_worker_registration_id());
}

}  // namespace

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

  void SetUp() override {
    original_ = PushMessagingAppIdentifier::Generate(
        GURL("https://www.example.com/"), 1);
    same_origin_and_sw_ = PushMessagingAppIdentifier::Generate(
        GURL("https://www.example.com"), 1);
    different_origin_ = PushMessagingAppIdentifier::Generate(
        GURL("https://foobar.example.com/"), 1);
    different_sw_ = PushMessagingAppIdentifier::Generate(
        GURL("https://www.example.com/"), 42);
  }

  Profile* profile() { return &profile_; }

  PushMessagingAppIdentifier original_;
  PushMessagingAppIdentifier same_origin_and_sw_;
  PushMessagingAppIdentifier different_origin_;
  PushMessagingAppIdentifier different_sw_;

 private:
  content::TestBrowserThreadBundle thread_bundle_;
  TestingProfile profile_;
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

TEST_F(PushMessagingAppIdentifierTest, PersistAndGet) {
  ASSERT_TRUE(PushMessagingAppIdentifier::Get(profile(),
                                              original_.app_id()).is_null());
  ASSERT_TRUE(PushMessagingAppIdentifier::Get(profile(), original_.origin(),
      original_.service_worker_registration_id()).is_null());

  // Test basic PersistToPrefs round trips.
  original_.PersistToPrefs(profile());
  {
    PushMessagingAppIdentifier get_by_app_id =
        PushMessagingAppIdentifier::Get(profile(), original_.app_id());
    EXPECT_FALSE(get_by_app_id.is_null());
    ExpectAppIdentifiersEqual(original_, get_by_app_id);
  }
  {
    PushMessagingAppIdentifier get_by_origin_and_swr_id =
        PushMessagingAppIdentifier::Get(profile(),
            original_.origin(), original_.service_worker_registration_id());
    EXPECT_FALSE(get_by_origin_and_swr_id.is_null());
    ExpectAppIdentifiersEqual(original_, get_by_origin_and_swr_id);
  }
}

TEST_F(PushMessagingAppIdentifierTest, PersistOverwritesSameOriginAndSW) {
  original_.PersistToPrefs(profile());

  // Test that PersistToPrefs overwrites when same origin and Service Worker.
  ASSERT_NE(original_.app_id(), same_origin_and_sw_.app_id());
  ASSERT_EQ(original_.origin(), same_origin_and_sw_.origin());
  ASSERT_EQ(original_.service_worker_registration_id(),
            same_origin_and_sw_.service_worker_registration_id());
  same_origin_and_sw_.PersistToPrefs(profile());
  {
    PushMessagingAppIdentifier get_by_original_app_id =
        PushMessagingAppIdentifier::Get(profile(), original_.app_id());
    EXPECT_TRUE(get_by_original_app_id.is_null());
  }
  {
    PushMessagingAppIdentifier get_by_soas_app_id =
        PushMessagingAppIdentifier::Get(profile(),
                                        same_origin_and_sw_.app_id());
    EXPECT_FALSE(get_by_soas_app_id.is_null());
    ExpectAppIdentifiersEqual(same_origin_and_sw_, get_by_soas_app_id);
  }
  {
    PushMessagingAppIdentifier get_by_original_origin_and_swr_id =
        PushMessagingAppIdentifier::Get(profile(),
            original_.origin(), original_.service_worker_registration_id());
    EXPECT_FALSE(get_by_original_origin_and_swr_id.is_null());
    ExpectAppIdentifiersEqual(same_origin_and_sw_,
                              get_by_original_origin_and_swr_id);
  }
}

TEST_F(PushMessagingAppIdentifierTest, PersistDoesNotOverwriteDifferent) {
  original_.PersistToPrefs(profile());

  // Test that PersistToPrefs doesn't overwrite when different origin or SW.
  ASSERT_NE(original_.app_id(), different_origin_.app_id());
  ASSERT_NE(original_.app_id(), different_sw_.app_id());
  different_origin_.PersistToPrefs(profile());
  different_sw_.PersistToPrefs(profile());
  {
    PushMessagingAppIdentifier get_by_original_app_id =
        PushMessagingAppIdentifier::Get(profile(), original_.app_id());
    EXPECT_FALSE(get_by_original_app_id.is_null());
    ExpectAppIdentifiersEqual(original_, get_by_original_app_id);
  }
  {
    PushMessagingAppIdentifier get_by_original_origin_and_swr_id =
        PushMessagingAppIdentifier::Get(profile(),
            original_.origin(), original_.service_worker_registration_id());
    EXPECT_FALSE(get_by_original_origin_and_swr_id.is_null());
    ExpectAppIdentifiersEqual(original_, get_by_original_origin_and_swr_id);
  }
}

TEST_F(PushMessagingAppIdentifierTest, DeleteFromPrefs) {
  original_.PersistToPrefs(profile());
  different_origin_.PersistToPrefs(profile());
  different_sw_.PersistToPrefs(profile());

  // Test DeleteFromPrefs. Deleted app identifier should be deleted.
  original_.DeleteFromPrefs(profile());
  {
    PushMessagingAppIdentifier get_by_original_app_id =
        PushMessagingAppIdentifier::Get(profile(), original_.app_id());
    EXPECT_TRUE(get_by_original_app_id.is_null());
  }
  {
    PushMessagingAppIdentifier get_by_original_origin_and_swr_id =
        PushMessagingAppIdentifier::Get(profile(),
            original_.origin(), original_.service_worker_registration_id());
    EXPECT_TRUE(get_by_original_origin_and_swr_id.is_null());
  }
}

TEST_F(PushMessagingAppIdentifierTest, GetAll) {
  original_.PersistToPrefs(profile());
  different_origin_.PersistToPrefs(profile());
  different_sw_.PersistToPrefs(profile());

  original_.DeleteFromPrefs(profile());

  // Test GetAll. Non-deleted app identifiers should all be listed.
  std::vector<PushMessagingAppIdentifier> all_app_identifiers =
      PushMessagingAppIdentifier::GetAll(profile());
  EXPECT_EQ(2u, all_app_identifiers.size());
  // Order is unspecified.
  bool contained_different_origin = false;
  bool contained_different_sw = false;
  for (const PushMessagingAppIdentifier& app_identifier : all_app_identifiers) {
    if (app_identifier.app_id() == different_origin_.app_id()) {
      ExpectAppIdentifiersEqual(different_origin_, app_identifier);
      contained_different_origin = true;
    } else {
      ExpectAppIdentifiersEqual(different_sw_, app_identifier);
      contained_different_sw = true;
    }
  }
  EXPECT_TRUE(contained_different_origin);
  EXPECT_TRUE(contained_different_sw);
}
