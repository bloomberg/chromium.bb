// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/feature_engagement/session_duration_updater.h"

#include "base/feature_list.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/field_trial_param_associator.h"
#include "base/metrics/field_trial_params.h"
#include "base/run_loop.h"
#include "base/sequenced_task_runner.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/feature_engagement/feature_tracker.h"
#include "chrome/browser/feature_engagement/session_duration_updater.h"
#include "chrome/browser/feature_engagement/session_duration_updater_factory.h"
#include "chrome/browser/metrics/desktop_session_duration/desktop_session_duration_tracker.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/feature_engagement/public/event_constants.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/feature_engagement/test/test_tracker.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace feature_engagement {

namespace {

class TestObserver : public SessionDurationUpdater::Observer {
 public:
  TestObserver()
      : pref_service_(
            base::MakeUnique<sync_preferences::TestingPrefServiceSyncable>()),
        session_duration_updater_(
            new SessionDurationUpdater(pref_service_.get())),
        session_duration_observer_(this) {
    SessionDurationUpdater::RegisterProfilePrefs(pref_service_->registry());
  }

  void AddSessionDurationObserver() {
    session_duration_observer_.Add(session_duration_updater_.get());
  }

  void RemoveSessionDurationObserver() {
    session_duration_observer_.Remove(session_duration_updater_.get());
  }

  // SessionDurationUpdater::Observer:
  void OnSessionEnded(base::TimeDelta total_session_time) override {}

  PrefService* GetPrefs() { return pref_service_.get(); }

  SessionDurationUpdater* GetSessionDurationUpdater() {
    return session_duration_updater_.get();
  }

 private:
  const std::unique_ptr<sync_preferences::TestingPrefServiceSyncable>
      pref_service_;
  std::unique_ptr<SessionDurationUpdater> session_duration_updater_;
  ScopedObserver<SessionDurationUpdater, SessionDurationUpdater::Observer>
      session_duration_observer_;

  DISALLOW_COPY_AND_ASSIGN(TestObserver);
};

class SessionDurationUpdaterTest : public testing::Test {
 public:
  SessionDurationUpdaterTest() = default;
  ~SessionDurationUpdaterTest() override = default;

  // testing::Test:
  void SetUp() override {
    // Start the DesktopSessionDurationTracker to track active session time.
    metrics::DesktopSessionDurationTracker::Initialize();

    test_observer_ = base::MakeUnique<TestObserver>();
    test_observer_->AddSessionDurationObserver();
  }

  void TearDown() override {
    test_observer_->RemoveSessionDurationObserver();
    metrics::DesktopSessionDurationTracker::CleanupForTesting();
  }

 protected:
  std::unique_ptr<TestObserver> test_observer_;

 private:
  content::TestBrowserThreadBundle thread_bundle_;

  DISALLOW_COPY_AND_ASSIGN(SessionDurationUpdaterTest);
};

}  // namespace

// kObservedSessionTime should be 0 on initalization and 50 after simulation.
TEST_F(SessionDurationUpdaterTest, TestTimeAdded) {
  // Tests the pref is registered to 0 before any session time passes.
  EXPECT_EQ(
      0, test_observer_->GetPrefs()->GetInteger(prefs::kObservedSessionTime));

  // Tests 50 minutes passing with an observer added.
  test_observer_->GetSessionDurationUpdater()->OnSessionEnded(
      base::TimeDelta::FromMinutes(50));

  EXPECT_EQ(
      50, test_observer_->GetPrefs()->GetInteger(prefs::kObservedSessionTime));
}

// kObservedSessionTime should not be updated when SessionDurationUpdater has
// no observers, but should start updating again if another observer is added.
TEST_F(SessionDurationUpdaterTest, TestAddingAndRemovingObservers) {
  // Tests 50 minutes passing with an observer added.
  test_observer_->GetSessionDurationUpdater()->OnSessionEnded(
      base::TimeDelta::FromMinutes(50));

  EXPECT_EQ(
      50, test_observer_->GetPrefs()->GetInteger(prefs::kObservedSessionTime));

  // Tests 50 minutes passing without any observers. No time should be added to
  // the pref in this case.
  test_observer_->RemoveSessionDurationObserver();

  test_observer_->GetSessionDurationUpdater()->OnSessionEnded(
      base::TimeDelta::FromMinutes(50));

  EXPECT_EQ(
      50, test_observer_->GetPrefs()->GetInteger(prefs::kObservedSessionTime));

  // Tests 50 minutes passing with an observer re-added. Time should be added
  // again now.
  test_observer_->AddSessionDurationObserver();

  test_observer_->GetSessionDurationUpdater()->OnSessionEnded(
      base::TimeDelta::FromMinutes(50));

  EXPECT_EQ(
      100, test_observer_->GetPrefs()->GetInteger(prefs::kObservedSessionTime));
}

}  // namespace feature_engagement
