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
  InvalidationLoggerObserverTest() { resetStates(); }

  void resetStates() {
    registrationReceived = false;
    unregistrationReceived = false;
    stateReceived = false;
    updateIdReceived = false;
    debugMessageReceived = false;
    invalidationReceived = false;
  }

  virtual void OnRegistration(const base::DictionaryValue& details) OVERRIDE {
    registrationReceived = true;
  }

  virtual void OnUnregistration(const base::DictionaryValue& details) OVERRIDE {
    unregistrationReceived = true;
  }

  virtual void OnStateChange(const syncer::InvalidatorState& newState)
      OVERRIDE {
    stateReceived = true;
  }

  virtual void OnUpdateIds(const base::DictionaryValue& details) OVERRIDE {
    updateIdReceived = true;
  }

  virtual void OnDebugMessage(const base::DictionaryValue& details) OVERRIDE {
    debugMessageReceived = true;
  }

  virtual void OnInvalidation(
      const syncer::ObjectIdInvalidationMap& newInvalidations) OVERRIDE {
    invalidationReceived = true;
  }

  bool registrationReceived;
  bool unregistrationReceived;
  bool stateReceived;
  bool updateIdReceived;
  bool debugMessageReceived;
  bool invalidationReceived;
};

// Test that the callbacks are actually being called when observers are
// registered and don't produce any other callback in the meantime.
TEST_F(InvalidationLoggerTest, TestCallbacks) {
  InvalidationLogger log;
  InvalidationLoggerObserverTest observerTest;

  log.RegisterForDebug(&observerTest);
  log.OnStateChange(syncer::INVALIDATIONS_ENABLED);
  EXPECT_TRUE(observerTest.stateReceived);
  EXPECT_FALSE(observerTest.updateIdReceived);
  EXPECT_FALSE(observerTest.registrationReceived);
  EXPECT_FALSE(observerTest.invalidationReceived);
  EXPECT_FALSE(observerTest.unregistrationReceived);
  EXPECT_FALSE(observerTest.debugMessageReceived);

  observerTest.resetStates();

  log.OnInvalidation(syncer::ObjectIdInvalidationMap());
  EXPECT_TRUE(observerTest.invalidationReceived);
  EXPECT_FALSE(observerTest.stateReceived);
  EXPECT_FALSE(observerTest.updateIdReceived);
  EXPECT_FALSE(observerTest.registrationReceived);
  EXPECT_FALSE(observerTest.unregistrationReceived);
  EXPECT_FALSE(observerTest.debugMessageReceived);

  log.UnregisterForDebug(&observerTest);
}

// Test that after registering an observer and then unregistering it
// no callbacks regarding that observer are called.
// (i.e. the observer is cleanly removed)
TEST_F(InvalidationLoggerTest, TestReleaseOfObserver) {
  InvalidationLogger log;
  InvalidationLoggerObserverTest observerTest;

  log.RegisterForDebug(&observerTest);
  log.UnregisterForDebug(&observerTest);

  log.OnInvalidation(syncer::ObjectIdInvalidationMap());
  log.OnStateChange(syncer::INVALIDATIONS_ENABLED);
  log.OnRegistration(base::DictionaryValue());
  log.OnUnregistration(base::DictionaryValue());
  log.OnDebugMessage(base::DictionaryValue());
  log.OnUpdateIds(base::DictionaryValue());
  EXPECT_FALSE(observerTest.registrationReceived);
  EXPECT_FALSE(observerTest.unregistrationReceived);
  EXPECT_FALSE(observerTest.updateIdReceived);
  EXPECT_FALSE(observerTest.invalidationReceived);
  EXPECT_FALSE(observerTest.stateReceived);
  EXPECT_FALSE(observerTest.debugMessageReceived);
}

// Test the EmitContet in InvalidationLogger is actually
// sending (only) state notifications.
TEST_F(InvalidationLoggerTest, TestEmitContent) {
  InvalidationLogger log;
  InvalidationLoggerObserverTest observerTest;

  log.RegisterForDebug(&observerTest);
  EXPECT_FALSE(observerTest.stateReceived);
  log.EmitContent();

  EXPECT_TRUE(observerTest.stateReceived);
  EXPECT_FALSE(observerTest.registrationReceived);
  EXPECT_FALSE(observerTest.unregistrationReceived);
  EXPECT_FALSE(observerTest.updateIdReceived);
  EXPECT_FALSE(observerTest.invalidationReceived);
  EXPECT_FALSE(observerTest.debugMessageReceived);
  log.UnregisterForDebug(&observerTest);
}
}  // namespace invalidation
