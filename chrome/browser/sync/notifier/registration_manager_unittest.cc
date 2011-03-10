// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/notifier/registration_manager.h"

#include <algorithm>
#include <cmath>
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

// Fake registration manager that lets you override jitter.
class FakeRegistrationManager : public RegistrationManager {
 public:
  explicit FakeRegistrationManager(
      invalidation::InvalidationClient* invalidation_client)
      : RegistrationManager(invalidation_client),
        jitter_(0.0) {}

  virtual ~FakeRegistrationManager() {}

  void SetJitter(double jitter) {
    jitter_ = jitter;
  }

 protected:
  virtual double GetJitter() {
    return jitter_;
  }

 private:
  double jitter_;

  DISALLOW_COPY_AND_ASSIGN(FakeRegistrationManager);
};

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
  syncable::ModelTypeSet registered_types_;

  DISALLOW_COPY_AND_ASSIGN(FakeInvalidationClient);
};

const syncable::ModelType kModelTypes[] = {
  syncable::BOOKMARKS,
  syncable::PREFERENCES,
  syncable::THEMES,
  syncable::AUTOFILL,
  syncable::EXTENSIONS,
};
const size_t kModelTypeCount = arraysize(kModelTypes);

void ExpectPendingRegistrations(
    const syncable::ModelTypeSet& expected_pending_types,
    double expected_delay_seconds,
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
    base::TimeDelta expected_delay =
        base::TimeDelta::FromSeconds(
            static_cast<int64>(expected_delay_seconds)) + offset;
    // TODO(akalin): Add base::PrintTo() for base::Time and
    // base::TimeDeltas.
    EXPECT_EQ(it->second.delay, expected_delay)
        << it->second.delay.InMicroseconds()
        << ", " << expected_delay.InMicroseconds();
    if (it->second.delay <= base::TimeDelta()) {
      EXPECT_EQ(it->second.actual_delay, base::TimeDelta());
    } else {
      EXPECT_EQ(it->second.delay, it->second.actual_delay);
    }
  }
  EXPECT_EQ(expected_pending_types, pending_types);
}

class RegistrationManagerTest : public testing::Test {
 protected:
  RegistrationManagerTest()
      : fake_registration_manager_(&fake_invalidation_client_) {}

  virtual ~RegistrationManagerTest() {}

  void LoseRegistrations(const syncable::ModelTypeSet& types) {
    for (syncable::ModelTypeSet::const_iterator it = types.begin();
         it != types.end(); ++it) {
      fake_invalidation_client_.LoseRegistration(*it);
      fake_registration_manager_.MarkRegistrationLost(*it);
    }
  }

  // Used by MarkRegistrationLostBackoff* tests.
  void RunBackoffTest(double jitter) {
    fake_registration_manager_.SetJitter(jitter);
    syncable::ModelTypeSet types(kModelTypes, kModelTypes + kModelTypeCount);
    fake_registration_manager_.SetRegisteredTypes(types);

    // Lose some types.
    syncable::ModelTypeSet lost_types(kModelTypes, kModelTypes + 2);
    LoseRegistrations(lost_types);
    ExpectPendingRegistrations(
        lost_types, 0.0,
        fake_registration_manager_.GetPendingRegistrations());

    // Trigger another failure to start delaying.
    fake_registration_manager_.FirePendingRegistrationsForTest();
    LoseRegistrations(lost_types);

    double scaled_jitter =
        jitter * RegistrationManager::kRegistrationDelayMaxJitter;

    double expected_delay =
        RegistrationManager::kInitialRegistrationDelaySeconds *
        (1.0 + scaled_jitter);
    expected_delay = std::floor(expected_delay);
    ExpectPendingRegistrations(
        lost_types, expected_delay,
        fake_registration_manager_.GetPendingRegistrations());

    // Trigger another failure.
    fake_registration_manager_.FirePendingRegistrationsForTest();
    LoseRegistrations(lost_types);
    expected_delay *=
        RegistrationManager::kRegistrationDelayExponent + scaled_jitter;
    expected_delay = std::floor(expected_delay);
    ExpectPendingRegistrations(
        lost_types, expected_delay,
        fake_registration_manager_.GetPendingRegistrations());

    // Trigger enough failures to hit the ceiling.
    while (expected_delay < RegistrationManager::kMaxRegistrationDelaySeconds) {
      fake_registration_manager_.FirePendingRegistrationsForTest();
      LoseRegistrations(lost_types);
      expected_delay *=
          RegistrationManager::kRegistrationDelayExponent + scaled_jitter;
      expected_delay = std::floor(expected_delay);
    }
    ExpectPendingRegistrations(
        lost_types,
        RegistrationManager::kMaxRegistrationDelaySeconds,
        fake_registration_manager_.GetPendingRegistrations());
  }

  FakeInvalidationClient fake_invalidation_client_;
  FakeRegistrationManager fake_registration_manager_;

 private:
  // Needed by timers in RegistrationManager.
  MessageLoop message_loop_;

  DISALLOW_COPY_AND_ASSIGN(RegistrationManagerTest);
};

TEST_F(RegistrationManagerTest, SetRegisteredTypes) {
  syncable::ModelTypeSet no_types;
  syncable::ModelTypeSet types(kModelTypes, kModelTypes + kModelTypeCount);

  EXPECT_EQ(no_types, fake_registration_manager_.GetRegisteredTypes());
  EXPECT_EQ(no_types, fake_invalidation_client_.GetRegisteredTypes());

  fake_registration_manager_.SetRegisteredTypes(types);
  EXPECT_EQ(types, fake_registration_manager_.GetRegisteredTypes());
  EXPECT_EQ(types, fake_invalidation_client_.GetRegisteredTypes());

  types.insert(syncable::APPS);
  types.erase(syncable::BOOKMARKS);
  fake_registration_manager_.SetRegisteredTypes(types);
  EXPECT_EQ(types, fake_registration_manager_.GetRegisteredTypes());
  EXPECT_EQ(types, fake_invalidation_client_.GetRegisteredTypes());
}

int GetRoundedBackoff(double retry_interval, double jitter) {
  const double kInitialRetryInterval = 3.0;
  const double kMinRetryInterval = 2.0;
  const double kMaxRetryInterval = 20.0;
  const double kBackoffExponent = 2.0;
  const double kMaxJitter = 0.5;

  return static_cast<int>(
      RegistrationManager::CalculateBackoff(retry_interval,
                                            kInitialRetryInterval,
                                            kMinRetryInterval,
                                            kMaxRetryInterval,
                                            kBackoffExponent,
                                            jitter,
                                            kMaxJitter));
}

TEST_F(RegistrationManagerTest, CalculateBackoff) {
  // Test initial.
  EXPECT_EQ(2, GetRoundedBackoff(0.0, -1.0));
  EXPECT_EQ(3, GetRoundedBackoff(0.0,  0.0));
  EXPECT_EQ(4, GetRoundedBackoff(0.0, +1.0));

  // Test non-initial.
  EXPECT_EQ(4, GetRoundedBackoff(3.0, -1.0));
  EXPECT_EQ(6, GetRoundedBackoff(3.0,  0.0));
  EXPECT_EQ(7, GetRoundedBackoff(3.0, +1.0));

  EXPECT_EQ(7, GetRoundedBackoff(5.0, -1.0));
  EXPECT_EQ(10, GetRoundedBackoff(5.0,  0.0));
  EXPECT_EQ(12, GetRoundedBackoff(5.0, +1.0));

  // Test ceiling.
  EXPECT_EQ(19, GetRoundedBackoff(13.0, -1.0));
  EXPECT_EQ(20, GetRoundedBackoff(13.0,  0.0));
  EXPECT_EQ(20, GetRoundedBackoff(13.0, +1.0));
}

TEST_F(RegistrationManagerTest, MarkRegistrationLost) {
  syncable::ModelTypeSet types(kModelTypes, kModelTypes + kModelTypeCount);

  fake_registration_manager_.SetRegisteredTypes(types);
  EXPECT_TRUE(fake_registration_manager_.GetPendingRegistrations().empty());

  // Lose some types.
  syncable::ModelTypeSet lost_types(
      kModelTypes, kModelTypes + 3);
  syncable::ModelTypeSet non_lost_types(
      kModelTypes + 3, kModelTypes + kModelTypeCount);
  LoseRegistrations(lost_types);
  ExpectPendingRegistrations(
      lost_types, 0.0,
      fake_registration_manager_.GetPendingRegistrations());
  EXPECT_EQ(non_lost_types, fake_registration_manager_.GetRegisteredTypes());
  EXPECT_EQ(non_lost_types, fake_invalidation_client_.GetRegisteredTypes());

  // Pretend we waited long enough to re-register.
  fake_registration_manager_.FirePendingRegistrationsForTest();
  EXPECT_EQ(types, fake_registration_manager_.GetRegisteredTypes());
  EXPECT_EQ(types, fake_invalidation_client_.GetRegisteredTypes());
}

TEST_F(RegistrationManagerTest, MarkRegistrationLostBackoffLow) {
  RunBackoffTest(-1.0);
}

TEST_F(RegistrationManagerTest, MarkRegistrationLostBackoffMid) {
  RunBackoffTest(0.0);
}

TEST_F(RegistrationManagerTest, MarkRegistrationLostBackoffHigh) {
  RunBackoffTest(+1.0);
}

TEST_F(RegistrationManagerTest, MarkRegistrationLostBackoffReset) {
  syncable::ModelTypeSet types(kModelTypes, kModelTypes + kModelTypeCount);

  fake_registration_manager_.SetRegisteredTypes(types);

  // Lose some types.
  syncable::ModelTypeSet lost_types(kModelTypes, kModelTypes + 2);
  LoseRegistrations(lost_types);
  ExpectPendingRegistrations(
      lost_types, 0.0,
      fake_registration_manager_.GetPendingRegistrations());

  // Trigger another failure to start delaying.
  fake_registration_manager_.FirePendingRegistrationsForTest();
  LoseRegistrations(lost_types);
  double expected_delay =
      RegistrationManager::kInitialRegistrationDelaySeconds;
  ExpectPendingRegistrations(
      lost_types, expected_delay,
      fake_registration_manager_.GetPendingRegistrations());

  // Set types again.
  fake_registration_manager_.SetRegisteredTypes(types);
  ExpectPendingRegistrations(
      syncable::ModelTypeSet(), 0.0,
      fake_registration_manager_.GetPendingRegistrations());
}

TEST_F(RegistrationManagerTest, MarkAllRegistrationsLost) {
  syncable::ModelTypeSet types(kModelTypes, kModelTypes + kModelTypeCount);

  fake_registration_manager_.SetRegisteredTypes(types);

  fake_invalidation_client_.LoseAllRegistrations();
  fake_registration_manager_.MarkAllRegistrationsLost();

  syncable::ModelTypeSet expected_types;
  EXPECT_EQ(expected_types, fake_registration_manager_.GetRegisteredTypes());
  EXPECT_EQ(expected_types, fake_invalidation_client_.GetRegisteredTypes());

  ExpectPendingRegistrations(
      types, 0.0,
      fake_registration_manager_.GetPendingRegistrations());

  // Trigger another failure to start delaying.
  fake_registration_manager_.FirePendingRegistrationsForTest();
  fake_invalidation_client_.LoseAllRegistrations();
  fake_registration_manager_.MarkAllRegistrationsLost();
  double expected_delay =
      RegistrationManager::kInitialRegistrationDelaySeconds;
  ExpectPendingRegistrations(
      types, expected_delay,
      fake_registration_manager_.GetPendingRegistrations());

  // Pretend we waited long enough to re-register.
  fake_registration_manager_.FirePendingRegistrationsForTest();
  EXPECT_EQ(types, fake_registration_manager_.GetRegisteredTypes());
  EXPECT_EQ(types, fake_invalidation_client_.GetRegisteredTypes());
}

}  // namespace
}  // namespace notifier
