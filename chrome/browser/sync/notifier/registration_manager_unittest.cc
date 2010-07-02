// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/notifier/registration_manager.h"

#include <cstddef>
#include <deque>
#include <vector>

#include "base/basictypes.h"
#include "base/logging.h"
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
  struct Args {
    Args() : callback(NULL) {}

    Args(const invalidation::ObjectId& oid,
         invalidation::RegistrationCallback* callback)
        : oid(oid), callback(callback) {}

    invalidation::ObjectId oid;
    invalidation::RegistrationCallback* callback;
  };

  FakeInvalidationClient() {}

  virtual ~FakeInvalidationClient() {
    for (std::deque<Args>::iterator it = register_calls.begin();
         it != register_calls.end(); ++it) {
      delete it->callback;
    }
  }

  void Register(const invalidation::ObjectId& oid,
                invalidation::RegistrationCallback* callback) {
    register_calls.push_back(Args(oid, callback));
  }

  void Unregister(const invalidation::ObjectId& oid,
                  invalidation::RegistrationCallback* callback) {
    ADD_FAILURE();
    delete callback;
  }

  invalidation::NetworkEndpoint* network_endpoint() {
    ADD_FAILURE();
    return NULL;
  }

  void GetClientUniquifier(invalidation::string* uniquifier) const {
    ADD_FAILURE();
  }

  std::deque<Args> register_calls;

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

invalidation::ObjectId ModelTypeToObjectId(
    syncable::ModelType model_type) {
  invalidation::ObjectId object_id;
  EXPECT_TRUE(RealModelTypeToObjectId(model_type, &object_id));
  return object_id;
}

syncable::ModelType ObjectIdToModelType(
    const invalidation::ObjectId& object_id) {
  syncable::ModelType model_type = syncable::UNSPECIFIED;
  EXPECT_TRUE(ObjectIdToRealModelType(object_id, &model_type));
  return model_type;
}

invalidation::RegistrationUpdateResult MakeRegistrationUpdateResult(
    syncable::ModelType model_type) {
  invalidation::RegistrationUpdateResult result;
  result.mutable_operation()->
      set_type(invalidation::RegistrationUpdate::REGISTER);
  *result.mutable_operation()->mutable_object_id() =
      ModelTypeToObjectId(model_type);
  result.mutable_status()->set_code(invalidation::Status::SUCCESS);
  return result;
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
            fake_invalidation_client_.register_calls.size());

  // Nothing should be registered yet.
  for (size_t i = 0; i < kModelTypeCount; ++i) {
    EXPECT_FALSE(registration_manager_.IsRegistered(kModelTypes[i]));
  }

  // Check object IDs.
  for (size_t i = 0; i < kModelTypeCount; ++i) {
    EXPECT_EQ(kModelTypes[i],
              ObjectIdToModelType(
                  fake_invalidation_client_.register_calls[i].oid));
  }

  // Prepare results.
  std::vector<invalidation::RegistrationUpdateResult> results(
      kModelTypeCount);
  for (size_t i = 0; i < kModelTypeCount; ++i) {
    results[i] = MakeRegistrationUpdateResult(kModelTypes[i]);
  }

  // Generate a variety of error conditions in all but the first
  // result.
  results[1].mutable_operation()->set_type(
      invalidation::RegistrationUpdate::UNREGISTER);
  results[2].mutable_operation()->mutable_object_id()->
      mutable_name()->set_string_value("garbage");
  results[3].mutable_status()->
      set_code(invalidation::Status::PERMANENT_FAILURE);
  *results[4].mutable_operation()->mutable_object_id() =
      ModelTypeToObjectId(syncable::TYPED_URLS);


  // Send the registration results back.
  for (size_t i = 0; i < kModelTypeCount; ++i) {
    fake_invalidation_client_.register_calls[i].callback->Run(results[i]);
  }

  // Only the first one should be registered.
  EXPECT_TRUE(registration_manager_.IsRegistered(kModelTypes[0]));
  for (size_t i = 1; i < kModelTypeCount; ++i) {
    EXPECT_FALSE(registration_manager_.IsRegistered(kModelTypes[i]));
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
            fake_invalidation_client_.register_calls.size());

  // Send the registration results back.
  for (size_t i = 0; i < kModelTypeCount; ++i) {
    fake_invalidation_client_.register_calls[i].callback->Run(
        MakeRegistrationUpdateResult(kModelTypes[i]));
  }

  // All should be registered.
  for (size_t i = 0; i < kModelTypeCount; ++i) {
    EXPECT_TRUE(registration_manager_.IsRegistered(kModelTypes[i]));
  }

  // Mark the registrations of all but the first one lost.
  for (size_t i = 1; i < kModelTypeCount; ++i) {
    registration_manager_.MarkRegistrationLost(kModelTypes[i]);
  }

  ASSERT_EQ(2 * kModelTypeCount - 1,
            fake_invalidation_client_.register_calls.size());

  // Only the first one should be registered.
  EXPECT_TRUE(registration_manager_.IsRegistered(kModelTypes[0]));
  for (size_t i = 1; i < kModelTypeCount; ++i) {
    EXPECT_FALSE(registration_manager_.IsRegistered(kModelTypes[i]));
  }

  // Send more registration results back.
  for (size_t i = 1; i < kModelTypeCount; ++i) {
    fake_invalidation_client_.register_calls[kModelTypeCount + i - 1].
        callback->Run(
            MakeRegistrationUpdateResult(kModelTypes[i]));
  }

  // All should be registered.
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
            fake_invalidation_client_.register_calls.size());

  // Send the registration results back.
  for (size_t i = 0; i < kModelTypeCount; ++i) {
    fake_invalidation_client_.register_calls[i].callback->Run(
        MakeRegistrationUpdateResult(kModelTypes[i]));
  }

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
            fake_invalidation_client_.register_calls.size());

  // None should be registered.
  for (size_t i = 0; i < kModelTypeCount; ++i) {
    EXPECT_FALSE(registration_manager_.IsRegistered(kModelTypes[i]));
  }

  // Send more registration results back.
  for (size_t i = 0; i < kModelTypeCount; ++i) {
    fake_invalidation_client_.register_calls[2 * kModelTypeCount + i - 1].
        callback->Run(
            MakeRegistrationUpdateResult(kModelTypes[i]));
  }

  // All should be registered.
  for (size_t i = 0; i < kModelTypeCount; ++i) {
    EXPECT_TRUE(registration_manager_.IsRegistered(kModelTypes[i]));
  }
}

}  // namespace
}  // namespace notifier
