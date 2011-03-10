// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/notifier/registration_manager.h"

#include <algorithm>
#include <cstddef>
#include <deque>
#include <vector>

#include "base/basictypes.h"
#include "base/message_loop.h"
#include "chrome/browser/sync/notifier/invalidation_util.h"
#include "chrome/browser/sync/syncable/model_type.h"
#include "google/cacheinvalidation/invalidation-client.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace sync_notifier {
namespace {

syncable::ModelType ObjectIdToModelType(
    const invalidation::ObjectId& object_id) {
  syncable::ModelType model_type = syncable::UNSPECIFIED;
  EXPECT_TRUE(ObjectIdToRealModelType(object_id, &model_type));
  return model_type;
}

// Fake invalidation client that just stores the currently-registered
// model types.
class FakeInvalidationClient : public invalidation::InvalidationClient {
 public:
  FakeInvalidationClient() {}

  virtual ~FakeInvalidationClient() {}

  void LoseRegistration(syncable::ModelType model_type) {
    EXPECT_GT(registered_types_.count(model_type), 0u);
    registered_types_.erase(model_type);
  }

  void LoseAllRegistrations() {
    registered_types_.clear();
  }

  // invalidation::InvalidationClient implementation.

  virtual void Start(const std::string& state) {}

  virtual void Register(const invalidation::ObjectId& oid) {
    syncable::ModelType model_type = ObjectIdToModelType(oid);
    EXPECT_EQ(0u, registered_types_.count(model_type));
    registered_types_.insert(model_type);
  }

  virtual void Unregister(const invalidation::ObjectId& oid) {
    syncable::ModelType model_type = ObjectIdToModelType(oid);
    EXPECT_GT(registered_types_.count(model_type), 0u);
    registered_types_.erase(model_type);
  }

  virtual invalidation::NetworkEndpoint* network_endpoint() {
    ADD_FAILURE();
    return NULL;
  }

  const syncable::ModelTypeSet GetRegisteredTypes() const {
    return registered_types_;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(FakeInvalidationClient);

  syncable::ModelTypeSet registered_types_;
};

class RegistrationManagerTest : public testing::Test {
 protected:
  RegistrationManagerTest()
      : registration_manager_(&fake_invalidation_client_) {}

  virtual ~RegistrationManagerTest() {}

  void LoseRegistrations(const syncable::ModelTypeSet& types) {
    for (syncable::ModelTypeSet::const_iterator it = types.begin();
         it != types.end(); ++it) {
      fake_invalidation_client_.LoseRegistration(*it);
      registration_manager_.MarkRegistrationLost(*it);
    }
  }

  FakeInvalidationClient fake_invalidation_client_;
  RegistrationManager registration_manager_;

 private:
  // Needed by timers in RegistrationManager.
  MessageLoop message_loop_;

  DISALLOW_COPY_AND_ASSIGN(RegistrationManagerTest);
};

const syncable::ModelType kModelTypes[] = {
  syncable::BOOKMARKS,
  syncable::PREFERENCES,
  syncable::THEMES,
  syncable::AUTOFILL,
  syncable::EXTENSIONS,
};
const size_t kModelTypeCount = arraysize(kModelTypes);

TEST_F(RegistrationManagerTest, SetRegisteredTypes) {
  syncable::ModelTypeSet no_types;
  syncable::ModelTypeSet types(kModelTypes, kModelTypes + kModelTypeCount);

  EXPECT_EQ(no_types, registration_manager_.GetRegisteredTypes());
  EXPECT_EQ(no_types, fake_invalidation_client_.GetRegisteredTypes());

  registration_manager_.SetRegisteredTypes(types);
  EXPECT_EQ(types, registration_manager_.GetRegisteredTypes());
  EXPECT_EQ(types, fake_invalidation_client_.GetRegisteredTypes());

  types.insert(syncable::APPS);
  types.erase(syncable::BOOKMARKS);
  registration_manager_.SetRegisteredTypes(types);
  EXPECT_EQ(types, registration_manager_.GetRegisteredTypes());
  EXPECT_EQ(types, fake_invalidation_client_.GetRegisteredTypes());
}

void ExpectPendingRegistrations(
    const syncable::ModelTypeSet& expected_pending_types,
    double expected_min_delay_seconds, double expected_max_delay_seconds,
    const RegistrationManager::PendingRegistrationMap& pending_registrations) {
  syncable::ModelTypeSet pending_types;
  for (RegistrationManager::PendingRegistrationMap::const_iterator it =
           pending_registrations.begin(); it != pending_registrations.end();
       ++it) {
    SCOPED_TRACE(syncable::ModelTypeToString(it->first));
    pending_types.insert(it->first);
    base::TimeDelta offset =
        it->second.last_registration_request -
        it->second.registration_attempt;
    base::TimeDelta expected_min_delay =
        base::TimeDelta::FromSeconds(
            static_cast<int64>(expected_min_delay_seconds)) + offset;
    base::TimeDelta expected_max_delay =
        base::TimeDelta::FromSeconds(
            static_cast<int64>(expected_max_delay_seconds)) + offset;
    // TODO(akalin): Add base::PrintTo() for base::Time and
    // base::TimeDeltas.
    EXPECT_GE(it->second.delay, expected_min_delay)
        << it->second.delay.InMicroseconds()
        << ", " << expected_min_delay.InMicroseconds();
    EXPECT_LE(it->second.delay, expected_max_delay)
        << it->second.delay.InMicroseconds()
        << ", " << expected_max_delay.InMicroseconds();
    if (it->second.delay <= base::TimeDelta()) {
      EXPECT_EQ(it->second.actual_delay, base::TimeDelta());
    } else {
      EXPECT_EQ(it->second.delay, it->second.actual_delay);
    }
  }
  EXPECT_EQ(expected_pending_types, pending_types);
}

TEST_F(RegistrationManagerTest, MarkRegistrationLost) {
  syncable::ModelTypeSet types(kModelTypes, kModelTypes + kModelTypeCount);

  registration_manager_.SetRegisteredTypes(types);
  EXPECT_TRUE(registration_manager_.GetPendingRegistrations().empty());

  // Lose some types.
  syncable::ModelTypeSet lost_types(
      kModelTypes, kModelTypes + 3);
  syncable::ModelTypeSet non_lost_types(
      kModelTypes + 3, kModelTypes + kModelTypeCount);
  LoseRegistrations(lost_types);
  ExpectPendingRegistrations(lost_types, 0.0, 0.0,
                             registration_manager_.GetPendingRegistrations());
  EXPECT_EQ(non_lost_types, registration_manager_.GetRegisteredTypes());
  EXPECT_EQ(non_lost_types, fake_invalidation_client_.GetRegisteredTypes());

  // Pretend we waited long enough to re-register.
  registration_manager_.FirePendingRegistrationsForTest();
  EXPECT_EQ(types, registration_manager_.GetRegisteredTypes());
  EXPECT_EQ(types, fake_invalidation_client_.GetRegisteredTypes());
}

TEST_F(RegistrationManagerTest, MarkRegistrationLostBackoff) {
  syncable::ModelTypeSet types(kModelTypes, kModelTypes + kModelTypeCount);

  registration_manager_.SetRegisteredTypes(types);

  // Lose some types.
  syncable::ModelTypeSet lost_types(kModelTypes, kModelTypes + 2);
  LoseRegistrations(lost_types);
  ExpectPendingRegistrations(lost_types, 0.0, 0.0,
                             registration_manager_.GetPendingRegistrations());

  // Trigger another failure to start delaying.
  registration_manager_.FirePendingRegistrationsForTest();
  LoseRegistrations(lost_types);
  double min_delay =
      RegistrationManager::kInitialRegistrationDelaySeconds *
      (1.0 - RegistrationManager::kRegistrationDelayMaxJitter);
  double max_delay =
      RegistrationManager::kInitialRegistrationDelaySeconds *
      (1.0 + RegistrationManager::kRegistrationDelayMaxJitter);
  ExpectPendingRegistrations(lost_types, min_delay, max_delay,
                             registration_manager_.GetPendingRegistrations());

  // Trigger another failure.
  registration_manager_.FirePendingRegistrationsForTest();
  LoseRegistrations(lost_types);
  min_delay *=
      RegistrationManager::kRegistrationDelayExponent -
      RegistrationManager::kRegistrationDelayMaxJitter;
  max_delay *=
      RegistrationManager::kRegistrationDelayExponent +
      RegistrationManager::kRegistrationDelayMaxJitter;
  ExpectPendingRegistrations(lost_types, min_delay, max_delay,
                             registration_manager_.GetPendingRegistrations());

  // Trigger enough failures to hit the ceiling.
  while (min_delay < RegistrationManager::kMaxRegistrationDelaySeconds) {
    registration_manager_.FirePendingRegistrationsForTest();
    LoseRegistrations(lost_types);
    min_delay *=
        RegistrationManager::kRegistrationDelayExponent -
        RegistrationManager::kRegistrationDelayMaxJitter;
  }
  ExpectPendingRegistrations(
      lost_types,
      RegistrationManager::kMaxRegistrationDelaySeconds,
      RegistrationManager::kMaxRegistrationDelaySeconds,
      registration_manager_.GetPendingRegistrations());
}

TEST_F(RegistrationManagerTest, MarkRegistrationLostBackoffReset) {
  syncable::ModelTypeSet types(kModelTypes, kModelTypes + kModelTypeCount);

  registration_manager_.SetRegisteredTypes(types);

  // Lose some types.
  syncable::ModelTypeSet lost_types(kModelTypes, kModelTypes + 2);
  LoseRegistrations(lost_types);
  ExpectPendingRegistrations(lost_types, 0.0, 0.0,
                             registration_manager_.GetPendingRegistrations());

  // Trigger another failure to start delaying.
  registration_manager_.FirePendingRegistrationsForTest();
  LoseRegistrations(lost_types);
  double min_delay =
      RegistrationManager::kInitialRegistrationDelaySeconds *
      (1.0 - RegistrationManager::kRegistrationDelayMaxJitter);
  double max_delay =
      RegistrationManager::kInitialRegistrationDelaySeconds *
      (1.0 + RegistrationManager::kRegistrationDelayMaxJitter);
  ExpectPendingRegistrations(
      lost_types, min_delay, max_delay,
      registration_manager_.GetPendingRegistrations());

  // Set types again.
  registration_manager_.SetRegisteredTypes(types);
  ExpectPendingRegistrations(syncable::ModelTypeSet(), 0.0, 0.0,
                             registration_manager_.GetPendingRegistrations());
}

TEST_F(RegistrationManagerTest, MarkAllRegistrationsLost) {
  syncable::ModelTypeSet types(kModelTypes, kModelTypes + kModelTypeCount);

  registration_manager_.SetRegisteredTypes(types);

  fake_invalidation_client_.LoseAllRegistrations();
  registration_manager_.MarkAllRegistrationsLost();

  syncable::ModelTypeSet expected_types;
  EXPECT_EQ(expected_types, registration_manager_.GetRegisteredTypes());
  EXPECT_EQ(expected_types, fake_invalidation_client_.GetRegisteredTypes());

  ExpectPendingRegistrations(types, 0.0, 0.0,
                             registration_manager_.GetPendingRegistrations());

  // Trigger another failure to start delaying.
  registration_manager_.FirePendingRegistrationsForTest();
  fake_invalidation_client_.LoseAllRegistrations();
  registration_manager_.MarkAllRegistrationsLost();
  double min_delay =
      RegistrationManager::kInitialRegistrationDelaySeconds *
      (1.0 - RegistrationManager::kRegistrationDelayMaxJitter);
  double max_delay =
      RegistrationManager::kInitialRegistrationDelaySeconds *
      (1.0 + RegistrationManager::kRegistrationDelayMaxJitter);
  ExpectPendingRegistrations(
      types, min_delay, max_delay,
      registration_manager_.GetPendingRegistrations());

  // Pretend we waited long enough to re-register.
  registration_manager_.FirePendingRegistrationsForTest();
  EXPECT_EQ(types, registration_manager_.GetRegisteredTypes());
  EXPECT_EQ(types, fake_invalidation_client_.GetRegisteredTypes());
}

}  // namespace
}  // namespace notifier
