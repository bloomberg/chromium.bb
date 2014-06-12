// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/invalidation/invalidation_service_android.h"

#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/invalidation/invalidation_controller_android.h"
#include "chrome/test/base/testing_profile.h"
#include "components/invalidation/fake_invalidation_handler.h"
#include "components/invalidation/invalidation_service_test_template.h"
#include "content/public/browser/notification_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace invalidation {

class MockInvalidationControllerAndroid : public InvalidationControllerAndroid {
 public:
  MockInvalidationControllerAndroid() {}
  virtual ~MockInvalidationControllerAndroid() {}

  virtual void SetRegisteredObjectIds(const syncer::ObjectIdSet& ids) OVERRIDE {
    registered_ids_ = ids;
  }

  syncer::ObjectIdSet registered_ids_;
};

class InvalidationServiceAndroidTestDelegate {
 public:
  InvalidationServiceAndroidTestDelegate() {}

  ~InvalidationServiceAndroidTestDelegate() {}

  void CreateInvalidationService() {
    profile_.reset(new TestingProfile());
    invalidation_service_android_.reset(
        new InvalidationServiceAndroid(
            profile_.get(),
            new MockInvalidationControllerAndroid()));
  }

  InvalidationService* GetInvalidationService() {
    return invalidation_service_android_.get();
  }

  void DestroyInvalidationService() {
    invalidation_service_android_.reset();
  }

  void TriggerOnInvalidatorStateChange(syncer::InvalidatorState state) {
    invalidation_service_android_->TriggerStateChangeForTest(state);
  }

  void TriggerOnIncomingInvalidation(
      const syncer::ObjectIdInvalidationMap& invalidation_map) {
    content::NotificationService::current()->Notify(
        chrome::NOTIFICATION_SYNC_REFRESH_REMOTE,
        content::Source<Profile>(profile_.get()),
        content::Details<const syncer::ObjectIdInvalidationMap>(
            &invalidation_map));
  }

  scoped_ptr<TestingProfile> profile_;
  scoped_ptr<InvalidationServiceAndroid> invalidation_service_android_;
};

INSTANTIATE_TYPED_TEST_CASE_P(
    AndroidInvalidationServiceTest, InvalidationServiceTest,
    InvalidationServiceAndroidTestDelegate);

class InvalidationServiceAndroidRegistrationTest : public testing::Test {
 protected:
  InvalidationServiceAndroidRegistrationTest()
      : invalidation_controller_(new MockInvalidationControllerAndroid()),
        invalidation_service_(&profile_, invalidation_controller_) {}

  virtual ~InvalidationServiceAndroidRegistrationTest() {}

  // Get the invalidation service being tested.
  InvalidationService& invalidation_service() {
    return invalidation_service_;
  }

  // Get the number of objects which are registered.
  size_t RegisteredObjectCount() {
    return registered_ids().size();
  }

  // Determines if the given object id is registered with the invalidation
  // controller.
  bool IsRegistered(const invalidation::ObjectId& id) {
    return registered_ids().find(id) != registered_ids().end();
  }

 private:
  // Get the set of objects registered with the invalidation controller.
  const syncer::ObjectIdSet& registered_ids() {
    return invalidation_controller_->registered_ids_;
  }

  TestingProfile profile_;
  MockInvalidationControllerAndroid* invalidation_controller_;
  InvalidationServiceAndroid invalidation_service_;
};

TEST_F(InvalidationServiceAndroidRegistrationTest, NoObjectRegistration) {
  syncer::FakeInvalidationHandler handler;
  invalidation_service().RegisterInvalidationHandler(&handler);
  EXPECT_EQ(0U, RegisteredObjectCount());
  invalidation_service().UnregisterInvalidationHandler(&handler);
}

TEST_F(InvalidationServiceAndroidRegistrationTest, UpdateObjectRegistration) {
  syncer::FakeInvalidationHandler handler;
  invalidation::ObjectId id1(1, "A");
  invalidation::ObjectId id2(2, "B");
  syncer::ObjectIdSet ids;
  invalidation_service().RegisterInvalidationHandler(&handler);

  // Register for both objects.
  ids.insert(id1);
  ids.insert(id2);
  invalidation_service().UpdateRegisteredInvalidationIds(&handler, ids);
  EXPECT_EQ(2U, RegisteredObjectCount());
  EXPECT_TRUE(IsRegistered(id1));
  EXPECT_TRUE(IsRegistered(id2));

  // Unregister for object 2.
  ids.erase(id2);
  invalidation_service().UpdateRegisteredInvalidationIds(&handler, ids);
  EXPECT_EQ(1U, RegisteredObjectCount());
  EXPECT_TRUE(IsRegistered(id1));

  // Unregister for object 1.
  ids.erase(id1);
  invalidation_service().UpdateRegisteredInvalidationIds(&handler, ids);
  EXPECT_EQ(0U, RegisteredObjectCount());

  invalidation_service().UnregisterInvalidationHandler(&handler);
}

#if defined(OS_ANDROID)

class InvalidationServiceAndroidTest : public testing::Test {
 public:
  InvalidationServiceAndroidTest()
      : invalidation_service_(&profile_, new InvalidationControllerAndroid()) {}
  virtual ~InvalidationServiceAndroidTest() {}

  InvalidationService& invalidation_service() {
    return invalidation_service_;
  }

 private:
  TestingProfile profile_;
  InvalidationServiceAndroid invalidation_service_;
};

TEST_F(InvalidationServiceAndroidTest, FetchClientId) {
  const std::string id1 = invalidation_service().GetInvalidatorClientId();
  ASSERT_FALSE(id1.empty());

  // If nothing else, the ID should be consistent.
  const std::string id2 = invalidation_service().GetInvalidatorClientId();
  ASSERT_EQ(id1, id2);
}

#endif

}  // namespace invalidation
