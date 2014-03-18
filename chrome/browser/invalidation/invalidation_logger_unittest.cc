// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/invalidation/invalidation_logger.h"
#include "chrome/browser/invalidation/invalidation_logger_observer.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace invalidation {

class InvalidationLoggerObserverTest : public InvalidationLoggerObserver {
 public:
  InvalidationLoggerObserverTest() { ResetStates(); }

  void ResetStates() {
    registration_change_received = false;
    state_received = false;
    update_id_received = false;
    debug_message_received = false;
    invalidation_received = false;
    detailed_status_received = false;
    update_id_replicated = std::map<std::string, syncer::ObjectIdSet>();
    registered_handlers = std::multiset<std::string>();
  }

  virtual void OnRegistrationChange(const std::multiset<std::string>& handlers)
      OVERRIDE {
    registered_handlers = handlers;
    registration_change_received = true;
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

  virtual void OnDetailedStatus(const base::DictionaryValue& details) OVERRIDE {
    detailed_status_received = true;
  }

  bool registration_change_received;
  bool state_received;
  bool update_id_received;
  bool debug_message_received;
  bool invalidation_received;
  bool detailed_status_received;
  std::map<std::string, syncer::ObjectIdSet> update_id_replicated;
  std::multiset<std::string> registered_handlers;
};

// Test that the callbacks are actually being called when observers are
// registered and don't produce any other callback in the meantime.
TEST(InvalidationLoggerTest, TestCallbacks) {
  InvalidationLogger log;
  InvalidationLoggerObserverTest observer_test;

  log.RegisterObserver(&observer_test);
  log.OnStateChange(syncer::INVALIDATIONS_ENABLED);
  EXPECT_TRUE(observer_test.state_received);
  EXPECT_FALSE(observer_test.update_id_received);
  EXPECT_FALSE(observer_test.registration_change_received);
  EXPECT_FALSE(observer_test.invalidation_received);
  EXPECT_FALSE(observer_test.debug_message_received);
  EXPECT_FALSE(observer_test.detailed_status_received);

  observer_test.ResetStates();

  log.OnInvalidation(syncer::ObjectIdInvalidationMap());
  EXPECT_TRUE(observer_test.invalidation_received);
  EXPECT_FALSE(observer_test.state_received);
  EXPECT_FALSE(observer_test.update_id_received);
  EXPECT_FALSE(observer_test.registration_change_received);
  EXPECT_FALSE(observer_test.debug_message_received);
  EXPECT_FALSE(observer_test.detailed_status_received);

  log.UnregisterObserver(&observer_test);
}

// Test that after registering an observer and then unregistering it
// no callbacks regarding that observer are called.
// (i.e. the observer is cleanly removed)
TEST(InvalidationLoggerTest, TestReleaseOfObserver) {
  InvalidationLogger log;
  InvalidationLoggerObserverTest observer_test;

  log.RegisterObserver(&observer_test);
  log.UnregisterObserver(&observer_test);

  log.OnInvalidation(syncer::ObjectIdInvalidationMap());
  log.OnStateChange(syncer::INVALIDATIONS_ENABLED);
  log.OnRegistration(std::string());
  log.OnUnregistration(std::string());
  log.OnDebugMessage(base::DictionaryValue());
  log.OnUpdateIds(std::map<std::string, syncer::ObjectIdSet>());
  EXPECT_FALSE(observer_test.registration_change_received);
  EXPECT_FALSE(observer_test.update_id_received);
  EXPECT_FALSE(observer_test.invalidation_received);
  EXPECT_FALSE(observer_test.state_received);
  EXPECT_FALSE(observer_test.debug_message_received);
  EXPECT_FALSE(observer_test.detailed_status_received);
}

// Test the EmitContet in InvalidationLogger is actually
// sending state and updateIds notifications.
TEST(InvalidationLoggerTest, TestEmitContent) {
  InvalidationLogger log;
  InvalidationLoggerObserverTest observer_test;

  log.RegisterObserver(&observer_test);
  EXPECT_FALSE(observer_test.state_received);
  EXPECT_FALSE(observer_test.update_id_received);
  log.EmitContent();
  // Expect state and registered handlers only because no Ids were registered.
  EXPECT_TRUE(observer_test.state_received);
  EXPECT_TRUE(observer_test.registration_change_received);
  EXPECT_FALSE(observer_test.update_id_received);
  EXPECT_FALSE(observer_test.invalidation_received);
  EXPECT_FALSE(observer_test.debug_message_received);
  EXPECT_FALSE(observer_test.detailed_status_received);

  observer_test.ResetStates();
  std::map<std::string, syncer::ObjectIdSet> test_map;
  test_map["Test"] = syncer::ObjectIdSet();
  log.OnUpdateIds(test_map);
  EXPECT_TRUE(observer_test.update_id_received);
  observer_test.ResetStates();

  log.EmitContent();
  // Expect now state, ids and registered handlers change.
  EXPECT_TRUE(observer_test.state_received);
  EXPECT_TRUE(observer_test.update_id_received);
  EXPECT_TRUE(observer_test.registration_change_received);
  EXPECT_FALSE(observer_test.invalidation_received);
  EXPECT_FALSE(observer_test.debug_message_received);
  EXPECT_FALSE(observer_test.detailed_status_received);
  log.UnregisterObserver(&observer_test);
}

// Test that the updateId notification actually sends
// what was sent to the Observer.
// The ObserverTest rebuilds the map that was sent in pieces by the logger.
TEST(InvalidationLoggerTest, TestUpdateIdsMap) {
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

// Test that registered handlers are being sent to the observers.
TEST(InvalidationLoggerTest, TestRegisteredHandlers) {
  InvalidationLogger log;
  InvalidationLoggerObserverTest observer_test;
  std::map<std::string, syncer::ObjectIdSet> test_map;
  log.RegisterObserver(&observer_test);

  log.OnRegistration(std::string("FakeHandler1"));
  std::multiset<std::string> test_multiset;
  test_multiset.insert("FakeHandler1");
  EXPECT_TRUE(observer_test.registration_change_received);
  EXPECT_EQ(observer_test.registered_handlers, test_multiset);

  observer_test.ResetStates();
  log.OnRegistration(std::string("FakeHandler2"));
  test_multiset.insert("FakeHandler2");
  EXPECT_TRUE(observer_test.registration_change_received);
  EXPECT_EQ(observer_test.registered_handlers, test_multiset);

  observer_test.ResetStates();
  log.OnUnregistration(std::string("FakeHandler2"));
  test_multiset.erase("FakeHandler2");
  EXPECT_TRUE(observer_test.registration_change_received);
  EXPECT_EQ(observer_test.registered_handlers, test_multiset);

  log.UnregisterObserver(&observer_test);
}
}  // namespace invalidation
