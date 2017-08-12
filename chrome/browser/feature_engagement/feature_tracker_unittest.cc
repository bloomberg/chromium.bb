// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/feature_engagement/feature_tracker.h"

#include "base/feature_list.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/field_trial_param_associator.h"
#include "base/metrics/field_trial_params.h"
#include "base/run_loop.h"
#include "base/sequenced_task_runner.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/feature_engagement/session_duration_updater.h"
#include "chrome/browser/feature_engagement/session_duration_updater_factory.h"
#include "chrome/browser/metrics/desktop_session_duration/desktop_session_duration_tracker.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/feature_engagement/public/event_constants.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/variations/variations_params_manager.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace feature_engagement {

namespace {

const int kTestTimeInMinutes = 100;
const int kTestTimeSufficentInMinutes = 110;
const int kTestTimeInsufficientInMinutes = 90;
const char kTestProfileName[] = "test-profile";

class TestFeatureTracker : public FeatureTracker {
 public:
  explicit TestFeatureTracker(Profile* profile)
      : FeatureTracker(
            profile,
            feature_engagement::SessionDurationUpdaterFactory::GetInstance()
                ->GetForProfile(profile)),
        pref_service_(
            base::MakeUnique<sync_preferences::TestingPrefServiceSyncable>()) {
    SessionDurationUpdater::RegisterProfilePrefs(pref_service_->registry());
  }

  int GetSessionTimeRequiredToShowInMinutes() override {
    return kTestTimeInMinutes;
  }

  void OnSessionTimeMet() override {}

 private:
  const std::unique_ptr<sync_preferences::TestingPrefServiceSyncable>
      pref_service_;
};

class MockTestFeatureTracker : public TestFeatureTracker {
 public:
  explicit MockTestFeatureTracker(Profile* profile)
      : TestFeatureTracker(profile) {}
  MOCK_METHOD0(OnSessionTimeMet, void());
};

class FeatureTrackerTest : public testing::Test {
 public:
  FeatureTrackerTest() = default;
  ~FeatureTrackerTest() override = default;

  // testing::Test:
  void SetUp() override {
    // Start the DesktopSessionDurationTracker to track active session time.
    metrics::DesktopSessionDurationTracker::Initialize();
    testing_profile_manager_ = base::MakeUnique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal());
    ASSERT_TRUE(testing_profile_manager_->SetUp());
    mock_feature_tracker_ =
        base::MakeUnique<testing::StrictMock<MockTestFeatureTracker>>(
            testing_profile_manager_->CreateTestingProfile(kTestProfileName));
  }

  void TearDown() override {
    metrics::DesktopSessionDurationTracker::CleanupForTesting();
    testing_profile_manager_.reset();
  }

 protected:
  std::unique_ptr<TestingProfileManager> testing_profile_manager_;
  std::unique_ptr<MockTestFeatureTracker> mock_feature_tracker_;

 private:
  content::TestBrowserThreadBundle thread_bundle_;

  DISALLOW_COPY_AND_ASSIGN(FeatureTrackerTest);
};

// If OnSessionEnded parameter is greater than HasEnoughSessionTimeElapsed
// then OnSessionTimeMet should be called.
//
// Note: in this case, RemoveSessionDurationObserver is called inside of
// OnSessionTimeMet, so it doesn't need to be called after the fact.
TEST_F(FeatureTrackerTest, TestExpectOnSessionTimeMet) {
  EXPECT_CALL(*mock_feature_tracker_, OnSessionTimeMet());
  mock_feature_tracker_.get()->OnSessionEnded(
      base::TimeDelta::FromMinutes(kTestTimeSufficentInMinutes));
}

// If OnSessionEnded parameter is less than than HasEnoughSessionTimeElapsed
// then OnSessionTimeMet should not be called.
TEST_F(FeatureTrackerTest, TestDontExpectOnSessionTimeMet) {
  mock_feature_tracker_.get()->OnSessionEnded(
      base::TimeDelta::FromMinutes(kTestTimeInsufficientInMinutes));
  mock_feature_tracker_.get()->RemoveSessionDurationObserver();
}

// The FeatureTracker should be observing sources until the
// RemoveSessionDurationObserver is called.
TEST_F(FeatureTrackerTest, TestAddAndRemoveObservers) {
  // AddSessionDurationObserver is called on initialization.
  ASSERT_TRUE(mock_feature_tracker_->IsObserving());

  mock_feature_tracker_.get()->RemoveSessionDurationObserver();

  EXPECT_FALSE(mock_feature_tracker_->IsObserving());
}

}  // namespace

}  // namespace feature_engagement
