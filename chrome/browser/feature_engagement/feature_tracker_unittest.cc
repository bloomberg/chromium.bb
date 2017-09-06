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
#include "base/time/time.h"
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

const int kTestTimeDeltaInMinutes = 100;
const int kTestTimeSufficentInMinutes = 110;
const int kTestTimeInsufficientInMinutes = 90;
const char kGroupName[] = "Enabled";
const char kNewTabFieldTrialName[] = "NewTabFieldTrial";
const char kTestProfileName[] = "test-profile";

class TestFeatureTracker : public FeatureTracker {
 public:
  explicit TestFeatureTracker(Profile* profile)
      : FeatureTracker(
            profile,
            feature_engagement::SessionDurationUpdaterFactory::GetInstance()
                ->GetForProfile(profile),
            &kIPHNewTabFeature,
            base::TimeDelta::FromMinutes(kTestTimeDeltaInMinutes)),
        pref_service_(
            base::MakeUnique<sync_preferences::TestingPrefServiceSyncable>()) {
    SessionDurationUpdater::RegisterProfilePrefs(pref_service_->registry());
  }

  base::TimeDelta GetSessionTimeRequiredToShowWrapper() {
    return GetSessionTimeRequiredToShow();
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
    // Need to invoke the reset method as TearDown is on the UI thread.
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

class FeatureTrackerMinutesTest : public testing::Test {
 public:
  FeatureTrackerMinutesTest() = default;
  ~FeatureTrackerMinutesTest() override = default;

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
    // Set up the NewTabInProductHelp field trial.
    base::FieldTrial* new_tab_trial = base::FieldTrialList::CreateFieldTrial(
        kNewTabFieldTrialName, kGroupName);
    trials_[kIPHNewTabFeature.name] = new_tab_trial;

    std::unique_ptr<base::FeatureList> feature_list =
        base::MakeUnique<base::FeatureList>();
    feature_list->RegisterFieldTrialOverride(
        kIPHNewTabFeature.name, base::FeatureList::OVERRIDE_ENABLE_FEATURE,
        new_tab_trial);

    scoped_feature_list_.InitWithFeatureList(std::move(feature_list));
    ASSERT_EQ(new_tab_trial,
              base::FeatureList::GetFieldTrial(kIPHNewTabFeature));
  }

  void TearDown() override {
    mock_feature_tracker_.get()->RemoveSessionDurationObserver();
    metrics::DesktopSessionDurationTracker::CleanupForTesting();
    // Need to invoke the rest method as TearDown is on the UI thread.
    testing_profile_manager_.reset();
  }

  void SetFeatureParams(const base::Feature& feature,
                        std::map<std::string, std::string> params) {
    ASSERT_TRUE(
        base::FieldTrialParamAssociator::GetInstance()
            ->AssociateFieldTrialParams(trials_[feature.name]->trial_name(),
                                        kGroupName, params));

    std::map<std::string, std::string> actualParams;
    EXPECT_TRUE(base::GetFieldTrialParamsByFeature(feature, &actualParams));
    EXPECT_EQ(params, actualParams);
  }

 protected:
  std::unique_ptr<TestingProfileManager> testing_profile_manager_;
  std::unique_ptr<MockTestFeatureTracker> mock_feature_tracker_;
  base::test::ScopedFeatureList scoped_feature_list_;
  std::map<std::string, base::FieldTrial*> trials_;
  variations::testing::VariationParamsManager params_manager_;

 private:
  content::TestBrowserThreadBundle thread_bundle_;

  DISALLOW_COPY_AND_ASSIGN(FeatureTrackerMinutesTest);
};

// Test that session time defaults to the time in the constructor if there is no
// field param value.
TEST_F(FeatureTrackerMinutesTest, TestSessionTimeWithNoFieldTrialValue) {
  EXPECT_EQ(mock_feature_tracker_->GetSessionTimeRequiredToShowWrapper(),
            base::TimeDelta::FromMinutes(kTestTimeDeltaInMinutes));
}

// Test that session time defaults to the valid time from the field param value.
TEST_F(FeatureTrackerMinutesTest, TestSessionTimeWithValidFieldTrialValue) {
  std::map<std::string, std::string> new_tab_params;
  new_tab_params["x_minutes"] = "1";
  SetFeatureParams(kIPHNewTabFeature, new_tab_params);

  EXPECT_EQ(mock_feature_tracker_->GetSessionTimeRequiredToShowWrapper(),
            base::TimeDelta::FromMinutes(1));
}

// Test that session time defaults to the time in the constructor if the field
// param value is empty string.
TEST_F(FeatureTrackerMinutesTest, TestSessionTimeWithEmptyFieldTrialValue) {
  std::map<std::string, std::string> new_tab_params;
  new_tab_params["x_minutes"] = "";
  SetFeatureParams(kIPHNewTabFeature, new_tab_params);

  EXPECT_EQ(mock_feature_tracker_->GetSessionTimeRequiredToShowWrapper(),
            base::TimeDelta::FromMinutes(kTestTimeDeltaInMinutes));
}

// Test that session time defaults to the time in the constructor if the field
// param value is invalid.
TEST_F(FeatureTrackerMinutesTest, TestSessionTimeWithInvalidFieldTrialValue) {
  std::map<std::string, std::string> new_tab_params;
  new_tab_params["x_minutes"] = "12g4";
  SetFeatureParams(kIPHNewTabFeature, new_tab_params);

  EXPECT_EQ(mock_feature_tracker_->GetSessionTimeRequiredToShowWrapper(),
            base::TimeDelta::FromMinutes(kTestTimeDeltaInMinutes));
}

}  // namespace

}  // namespace feature_engagement
