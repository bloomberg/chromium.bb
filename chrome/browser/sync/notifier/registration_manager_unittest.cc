// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/notifier/registration_manager.h"

#include <cstddef>
#include <deque>
#include <vector>

#include "base/basictypes.h"
#include "chrome/browser/sync/notifier/invalidation_util.h"
#include "chrome/browser/sync/syncable/model_type.h"
#include "google/cacheinvalidation/invalidation-client.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace sync_notifier {
namespace {

// Fake invalidation client that just stores the args of calls to
// Register().
class FakeInvalidationClient : public invalidation::InvalidationClient {
 public:
  FakeInvalidationClient() {}

  virtual ~FakeInvalidationClient() {}

  virtual void Register(const invalidation::ObjectId& oid) {
    registered_oids.push_back(oid);
  }

  virtual void Unregister(const invalidation::ObjectId& oid) {
    ADD_FAILURE();
  }

  virtual invalidation::NetworkEndpoint* network_endpoint() {
    ADD_FAILURE();
    return NULL;
  }

  std::deque<invalidation::ObjectId> registered_oids;

 private:
  DISALLOW_COPY_AND_ASSIGN(FakeInvalidationClient);
};

class RegistrationManagerTest : public testing::Test {
 protected:
  RegistrationManagerTest()
      : registration_manager_(&fake_invalidation_client_) {}

  virtual ~RegistrationManagerTest() {}

  FakeInvalidationClient fake_invalidation_client_;
  RegistrationManager registration_manager_;

 private:
  DISALLOW_COPY_AND_ASSIGN(RegistrationManagerTest);
};

syncable::ModelType ObjectIdToModelType(
    const invalidation::ObjectId& object_id) {
  syncable::ModelType model_type = syncable::UNSPECIFIED;
  EXPECT_TRUE(ObjectIdToRealModelType(object_id, &model_type));
  return model_type;
}

TEST_F(RegistrationManagerTest, RegisterType) {
  const syncable::ModelType kModelTypes[] = {
    syncable::BOOKMARKS,
    syncable::PREFERENCES,
    syncable::THEMES,
    syncable::AUTOFILL,
    syncable::EXTENSIONS,
  };
  const size_t kModelTypeCount = arraysize(kModelTypes);

  // Register types.
  for (size_t i = 0; i < kModelTypeCount; ++i) {
    // Register twice; it shouldn't matter.
    registration_manager_.RegisterType(kModelTypes[i]);
    registration_manager_.RegisterType(kModelTypes[i]);
  }

  ASSERT_EQ(kModelTypeCount,
            fake_invalidation_client_.registered_oids.size());

  // Everything should be registered.
  for (size_t i = 0; i < kModelTypeCount; ++i) {
    EXPECT_TRUE(registration_manager_.IsRegistered(kModelTypes[i]));
  }

  // Check object IDs.
  for (size_t i = 0; i < kModelTypeCount; ++i) {
    EXPECT_EQ(kModelTypes[i],
              ObjectIdToModelType(
                  fake_invalidation_client_.registered_oids[i]));
  }
}

TEST_F(RegistrationManagerTest, MarkRegistrationLost) {
  const syncable::ModelType kModelTypes[] = {
    syncable::BOOKMARKS,
    syncable::PREFERENCES,
    syncable::THEMES,
    syncable::AUTOFILL,
    syncable::EXTENSIONS,
  };
  const size_t kModelTypeCount = arraysize(kModelTypes);

  // Register types.
  for (size_t i = 0; i < kModelTypeCount; ++i) {
    registration_manager_.RegisterType(kModelTypes[i]);
  }

  ASSERT_EQ(kModelTypeCount,
            fake_invalidation_client_.registered_oids.size());

  // All should be registered.
  for (size_t i = 0; i < kModelTypeCount; ++i) {
    EXPECT_TRUE(registration_manager_.IsRegistered(kModelTypes[i]));
  }

  // Mark the registrations of all but the first one lost.
  for (size_t i = 1; i < kModelTypeCount; ++i) {
    registration_manager_.MarkRegistrationLost(kModelTypes[i]);
  }

  ASSERT_EQ(2 * kModelTypeCount - 1,
            fake_invalidation_client_.registered_oids.size());

  // All should still be registered.
  for (size_t i = 0; i < kModelTypeCount; ++i) {
    EXPECT_TRUE(registration_manager_.IsRegistered(kModelTypes[i]));
  }
}

TEST_F(RegistrationManagerTest, MarkAllRegistrationsLost) {
  const syncable::ModelType kModelTypes[] = {
    syncable::BOOKMARKS,
    syncable::PREFERENCES,
    syncable::THEMES,
    syncable::AUTOFILL,
    syncable::EXTENSIONS,
  };
  const size_t kModelTypeCount = arraysize(kModelTypes);

  // Register types.
  for (size_t i = 0; i < kModelTypeCount; ++i) {
    registration_manager_.RegisterType(kModelTypes[i]);
  }

  ASSERT_EQ(kModelTypeCount,
            fake_invalidation_client_.registered_oids.size());

  // All should be registered.
  for (size_t i = 0; i < kModelTypeCount; ++i) {
    EXPECT_TRUE(registration_manager_.IsRegistered(kModelTypes[i]));
  }

  // Mark the registrations of all but the first one lost.  Then mark
  // everything lost.
  for (size_t i = 1; i < kModelTypeCount; ++i) {
    registration_manager_.MarkRegistrationLost(kModelTypes[i]);
  }
  registration_manager_.MarkAllRegistrationsLost();

  ASSERT_EQ(3 * kModelTypeCount - 1,
            fake_invalidation_client_.registered_oids.size());

  // All should still be registered.
  for (size_t i = 0; i < kModelTypeCount; ++i) {
    EXPECT_TRUE(registration_manager_.IsRegistered(kModelTypes[i]));
  }
}

}  // namespace
}  // namespace notifier
