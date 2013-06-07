// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_resource/eula_accepted_notifier.h"

#include "base/prefs/pref_registry_simple.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "testing/gtest/include/gtest/gtest.h"

class EulaAcceptedNotifierTest : public testing::Test,
                                 public EulaAcceptedNotifier::Observer {
 public:
  EulaAcceptedNotifierTest() : eula_accepted_called_(false) {
  }

  // testing::Test overrides.
  virtual void SetUp() OVERRIDE {
    local_state_.registry()->RegisterBooleanPref(prefs::kEulaAccepted, false);
    notifier_.reset(new EulaAcceptedNotifier(&local_state_));
    notifier_->Init(this);
  }

  // EulaAcceptedNotifier::Observer overrides.
  virtual void OnEulaAccepted() OVERRIDE {
    EXPECT_FALSE(eula_accepted_called_);
    eula_accepted_called_ = true;
  }

  void SetEulaAcceptedPref() {
    local_state_.SetBoolean(prefs::kEulaAccepted, true);
  }

  EulaAcceptedNotifier* notifier() {
    return notifier_.get();
  }

  bool eula_accepted_called() {
    return eula_accepted_called_;
  }

 private:
  TestingPrefServiceSimple local_state_;
  scoped_ptr<EulaAcceptedNotifier> notifier_;
  bool eula_accepted_called_;

  DISALLOW_COPY_AND_ASSIGN(EulaAcceptedNotifierTest);
};

TEST_F(EulaAcceptedNotifierTest, EulaAlreadyAccepted) {
  SetEulaAcceptedPref();
  EXPECT_TRUE(notifier()->IsEulaAccepted());
  EXPECT_FALSE(eula_accepted_called());
  // Call it a second time, to ensure the answer doesn't change.
  EXPECT_TRUE(notifier()->IsEulaAccepted());
  EXPECT_FALSE(eula_accepted_called());
}

TEST_F(EulaAcceptedNotifierTest, EulaNotAccepted) {
  EXPECT_FALSE(notifier()->IsEulaAccepted());
  EXPECT_FALSE(eula_accepted_called());
  // Call it a second time, to ensure the answer doesn't change.
  EXPECT_FALSE(notifier()->IsEulaAccepted());
  EXPECT_FALSE(eula_accepted_called());
}

TEST_F(EulaAcceptedNotifierTest, EulaNotInitiallyAccepted) {
  EXPECT_FALSE(notifier()->IsEulaAccepted());
  SetEulaAcceptedPref();
  EXPECT_TRUE(notifier()->IsEulaAccepted());
  EXPECT_TRUE(eula_accepted_called());
  // Call it a second time, to ensure the answer doesn't change.
  EXPECT_TRUE(notifier()->IsEulaAccepted());
}
