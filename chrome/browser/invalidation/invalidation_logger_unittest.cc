// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/invalidation/invalidation_logger.h"
#include "chrome/browser/invalidation/invalidation_logger_observer.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace invalidation {

class InvalidationLoggerTest : public testing::Test {
 public:
  InvalidationLoggerTest() {}
};

class InvalidationLoggerObserverTest : public InvalidationLoggerObserver {
 public:
  InvalidationLoggerObserverTest() { ResetStates(); }

  void ResetStates() {
    registration_received = false;
    unregistration_received = false;
    state_received = false;
    update_id_received = false;
    debug_message_received = false;
    invalidation_received = false;
    update_id_replicated = std::map<std::string, syncer::ObjectIdSet>();
  }

  virtual void OnRegistration(const base::DictionaryValue& details) OVERRIDE {
    registration_received = true;
  }

  virtual void OnUnregistration(const base::DictionaryValue& details) OVERRIDE {
    unregistration_received = true;
  }

  virtual void OnStateChange(const syncer::InvalidatorState& newState)
      OVERRIDE {
    state_received = true;
  }

  virtual void OnUpdateIds(const std::string& handler,
                           const syncer::ObjectIdSet& details) OVERRIDE {
    update_id_received = true;
    update_id_replicated[handler] = details;
  }

  virtual void OnDebugMessage(const base::DictionaryValue& details) OVERRIDE {
    debug_message_received = true;
  }

  virtual void OnInvalidation(
      const syncer::ObjectIdInvalidationMap& newInvalidations) OVERRIDE {
    invalidation_received = true;
  }

  bool registration_received;
  bool unregistration_received;
  bool state_received;
  bool update_id_received;
  bool debug_message_received;
  bool invalidation_received;
  std::map<std::string, syncer::ObjectIdSet> update_id_replicated;
};

// Test that the callbacks are actually being called when observers are
// registered and don't produce any other callback in the meantime.
TEST_F(InvalidationLoggerTest, TestCallbacks) {
  InvalidationLogger log;
  InvalidationLoggerObserverTest observer_test;

  log.RegisterObserver(&observer_test);
  log.OnStateChange(syncer::INVALIDATIONS_ENABLED);
  EXPECT_TRUE(observer_test.state_received);
  EXPECT_FALSE(observer_test.update_id_received);
  EXPECT_FALSE(observer_test.registration_received);
  EXPECT_FALSE(observer_test.invalidation_received);
  EXPECT_FALSE(observer_test.unregistration_received);
  EXPECT_FALSE(observer_test.debug_message_received);

  observer_test.ResetStates();

  log.OnInvalidation(syncer::ObjectIdInvalidationMap());
  EXPECT_TRUE(observer_test.invalidation_received);
  EXPECT_FALSE(observer_test.state_received);
  EXPECT_FALSE(observer_test.update_id_received);
  EXPECT_FALSE(observer_test.registration_received);
  EXPECT_FALSE(observer_test.unregistration_received);
  EXPECT_FALSE(observer_test.debug_message_received);

  log.UnregisterObserver(&observer_test);
}

// Test that after registering an observer and then unregistering it
// no callbacks regarding that observer are called.
// (i.e. the observer is cleanly removed)
TEST_F(InvalidationLoggerTest, TestReleaseOfObserver) {
  InvalidationLogger log;
  InvalidationLoggerObserverTest observer_test;

  log.RegisterObserver(&observer_test);
  log.UnregisterObserver(&observer_test);

  log.OnInvalidation(syncer::ObjectIdInvalidationMap());
  log.OnStateChange(syncer::INVALIDATIONS_ENABLED);
  log.OnRegistration(base::DictionaryValue());
  log.OnUnregistration(base::DictionaryValue());
  log.OnDebugMessage(base::DictionaryValue());
  log.OnUpdateIds(std::map<std::string, syncer::ObjectIdSet>());
  EXPECT_FALSE(observer_test.registration_received);
  EXPECT_FALSE(observer_test.unregistration_received);
  EXPECT_FALSE(observer_test.update_id_received);
  EXPECT_FALSE(observer_test.invalidation_received);
  EXPECT_FALSE(observer_test.state_received);
  EXPECT_FALSE(observer_test.debug_message_received);
}

// Test the EmitContet in InvalidationLogger is actually
// sending state and updateIds notifications.
TEST_F(InvalidationLoggerTest, TestEmitContent) {
  InvalidationLogger log;
  InvalidationLoggerObserverTest observer_test;

  log.RegisterObserver(&observer_test);
  EXPECT_FALSE(observer_test.state_received);
  EXPECT_FALSE(observer_test.update_id_received);
  log.EmitContent();
  // Only expect state because no Ids were registered.
  EXPECT_TRUE(observer_test.state_received);
  EXPECT_FALSE(observer_test.registration_received);
  EXPECT_FALSE(observer_test.unregistration_received);
  EXPECT_FALSE(observer_test.update_id_received);
  EXPECT_FALSE(observer_test.invalidation_received);
  EXPECT_FALSE(observer_test.debug_message_received);

  observer_test.ResetStates();
  std::map<std::string, syncer::ObjectIdSet> test_map;
  test_map["Test"] = syncer::ObjectIdSet();
  log.OnUpdateIds(test_map);
  EXPECT_TRUE(observer_test.update_id_received);
  observer_test.ResetStates();

  log.EmitContent();
  // Expect now state and ids change.
  EXPECT_TRUE(observer_test.state_received);
  EXPECT_TRUE(observer_test.update_id_received);
  EXPECT_FALSE(observer_test.registration_received);
  EXPECT_FALSE(observer_test.unregistration_received);
  EXPECT_FALSE(observer_test.invalidation_received);
  EXPECT_FALSE(observer_test.debug_message_received);
  log.UnregisterObserver(&observer_test);
}

// Test that the updateId notification actually sends
// what was sent to the Observer.
// The ObserverTest rebuilds the map that was sent in pieces by the logger.
TEST_F(InvalidationLoggerTest, TestUpdateIdsMap) {
  InvalidationLogger log;
  InvalidationLoggerObserverTest observer_test;
  std::map<std::string, syncer::ObjectIdSet> test_map;
  log.RegisterObserver(&observer_test);

  syncer::ObjectIdSet sync_set_A;
  sync_set_A.insert(ObjectId(1000, "DataType1"));
  sync_set_A.insert(ObjectId(1000, "DataType2"));
  syncer::ObjectIdSet sync_set_B;
  sync_set_B.insert(ObjectId(1020, "DataTypeA"));
  test_map["TestA"] = sync_set_A;
  test_map["TestB"] = sync_set_B;

  log.OnUpdateIds(test_map);
  EXPECT_EQ(test_map, observer_test.update_id_replicated);

  syncer::ObjectIdSet sync_set_B2;
  sync_set_B2.insert(ObjectId(1020, "DataTypeF"));
  sync_set_B2.insert(ObjectId(1020, "DataTypeG"));
  test_map["TestB"] = sync_set_B2;

  log.OnUpdateIds(test_map);
  EXPECT_EQ(test_map, observer_test.update_id_replicated);

  // The emit content should return the same map too.
  observer_test.ResetStates();
  log.EmitContent();
  EXPECT_EQ(test_map, observer_test.update_id_replicated);

  log.UnregisterObserver(&observer_test);
}
}  // namespace invalidation
