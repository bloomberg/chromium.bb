// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/feature_engagement/incognito_window/incognito_window_tracker.h"

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
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/feature_engagement/public/event_constants.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/feature_engagement/public/tracker.h"
#include "components/feature_engagement/test/test_tracker.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/variations/variations_params_manager.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace feature_engagement {

namespace {

const char kIncognitoWindowTrialName[] = "IncognitoWindowTrial";
const char kGroupName[] = "Enabled";
const char kTestProfileName[] = "test-profile";

class MockTracker : public Tracker {
 public:
  MockTracker() = default;
  MOCK_METHOD1(NotifyEvent, void(const std::string& event));
  MOCK_METHOD1(ShouldTriggerHelpUI, bool(const base::Feature& feature));
  MOCK_METHOD1(GetTriggerState,
               Tracker::TriggerState(const base::Feature& feature));
  MOCK_METHOD1(Dismissed, void(const base::Feature& feature));
  MOCK_METHOD0(IsInitialized, bool());
  MOCK_METHOD1(AddOnInitializedCallback, void(OnInitializedCallback callback));
};

class FakeIncognitoWindowTracker : public IncognitoWindowTracker {
 public:
  FakeIncognitoWindowTracker(Tracker* feature_tracker, Profile* profile)
      : IncognitoWindowTracker(
            feature_engagement::SessionDurationUpdaterFactory::GetInstance()
                ->GetForProfile(profile)),
        feature_tracker_(feature_tracker),
        pref_service_(
            base::MakeUnique<sync_preferences::TestingPrefServiceSyncable>()) {
    SessionDurationUpdater::RegisterProfilePrefs(pref_service_->registry());
  }

  PrefService* GetPrefs() { return pref_service_.get(); }

  // feature_engagement::NewTabTracker:
  Tracker* GetTracker() const override { return feature_tracker_; }

 private:
  Tracker* const feature_tracker_;
  const std::unique_ptr<sync_preferences::TestingPrefServiceSyncable>
      pref_service_;
};

class IncognitoWindowTrackerEventTest : public testing::Test {
 public:
  IncognitoWindowTrackerEventTest() = default;
  ~IncognitoWindowTrackerEventTest() override = default;

  void SetUp() override {
    // Start the DesktopSessionDurationTracker to track active session time.
    metrics::DesktopSessionDurationTracker::Initialize();
    testing_profile_manager_ = base::MakeUnique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal());
    ASSERT_TRUE(testing_profile_manager_->SetUp());
    mock_tracker_ = base::MakeUnique<testing::StrictMock<MockTracker>>();
    incognito_window_tracker_ = base::MakeUnique<FakeIncognitoWindowTracker>(
        mock_tracker_.get(),
        testing_profile_manager_->CreateTestingProfile(kTestProfileName));
  }

  void TearDown() override {
    incognito_window_tracker_->RemoveSessionDurationObserver();
    metrics::DesktopSessionDurationTracker::CleanupForTesting();
    // Need to invoke the reset method as TearDown is on the UI thread.
    testing_profile_manager_.reset();
  }

 protected:
  std::unique_ptr<TestingProfileManager> testing_profile_manager_;
  std::unique_ptr<MockTracker> mock_tracker_;
  std::unique_ptr<FakeIncognitoWindowTracker> incognito_window_tracker_;

 private:
  content::TestBrowserThreadBundle thread_bundle_;

  DISALLOW_COPY_AND_ASSIGN(IncognitoWindowTrackerEventTest);
};

}  // namespace

// Tests to verify FeatureEngagementTracker API boundary expectations:

// If OnIncognitoWindowOpened() is called, the FeatureEngagementTracker
// receives the kIncognitoWindowOpened.
TEST_F(IncognitoWindowTrackerEventTest, TestOnIncognitoWindowOpened) {
  EXPECT_CALL(*mock_tracker_, NotifyEvent(events::kIncognitoWindowOpened));
  incognito_window_tracker_->OnIncognitoWindowOpened();
}

// If OnSessionTimeMet() is called, the FeatureEngagementTracker
// receives the kSessionTime event.
TEST_F(IncognitoWindowTrackerEventTest, TestOnSessionTimeMet) {
  EXPECT_CALL(*mock_tracker_,
              NotifyEvent(events::kIncognitoWindowSessionTimeMet));
  incognito_window_tracker_->OnSessionTimeMet();
}

namespace {

class IncognitoWindowTrackerTest : public testing::Test {
 public:
  IncognitoWindowTrackerTest() = default;
  ~IncognitoWindowTrackerTest() override = default;

  void SetUp() override {
    // Set up the kIncognitoWindowTrialName field trial.
    base::FieldTrial* incognito_window_trial =
        base::FieldTrialList::CreateFieldTrial(kIncognitoWindowTrialName,
                                               kGroupName);
    trials_[kIPHIncognitoWindowFeature.name] = incognito_window_trial;

    std::unique_ptr<base::FeatureList> feature_list =
        base::MakeUnique<base::FeatureList>();
    feature_list->RegisterFieldTrialOverride(
        kIPHIncognitoWindowFeature.name,
        base::FeatureList::OVERRIDE_ENABLE_FEATURE, incognito_window_trial);

    scoped_feature_list_.InitWithFeatureList(std::move(feature_list));
    ASSERT_EQ(incognito_window_trial,
              base::FeatureList::GetFieldTrial(kIPHIncognitoWindowFeature));

    std::map<std::string, std::string> incognito_window_params;
    incognito_window_params["event_incognito_window_opened"] =
        "name:incognito_window_opened;comparator:==0;window:3650;storage:3650";
    incognito_window_params["event_incognito_window_session_time_met"] =
        "name:incognito_window_session_time_met;comparator:>=1;window:3650;"
        "storage:3650";
    incognito_window_params["event_trigger"] =
        "name:incognito_window_trigger;comparator:==0;window:3650;storage:3650";
    incognito_window_params["event_used"] =
        "name:incognito_window_clicked;comparator:any;window:3650;storage:3650";
    incognito_window_params["session_rate"] = "<=3";
    incognito_window_params["availability"] = "any";

    SetFeatureParams(kIPHIncognitoWindowFeature, incognito_window_params);

    // Start the DesktopSessionDurationTracker to track active session time.
    metrics::DesktopSessionDurationTracker::Initialize();

    testing_profile_manager_ = base::MakeUnique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal());
    ASSERT_TRUE(testing_profile_manager_->SetUp());

    feature_engagement_tracker_ = CreateTestTracker();

    incognito_window_tracker_ = base::MakeUnique<FakeIncognitoWindowTracker>(
        feature_engagement_tracker_.get(),
        testing_profile_manager_->CreateTestingProfile(kTestProfileName));

    // The feature engagement tracker does async initialization.
    base::RunLoop().RunUntilIdle();
    ASSERT_TRUE(feature_engagement_tracker_->IsInitialized());
  }

  void TearDown() override {
    incognito_window_tracker_->RemoveSessionDurationObserver();
    testing_profile_manager_->DeleteTestingProfile(kTestProfileName);
    // Need to invoke the reset method as TearDown is on the UI thread.
    testing_profile_manager_.reset();
    metrics::DesktopSessionDurationTracker::CleanupForTesting();

    // This is required to ensure each test can define its own params.
    base::FieldTrialParamAssociator::GetInstance()->ClearAllParamsForTesting();
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
  std::unique_ptr<FakeIncognitoWindowTracker> incognito_window_tracker_;
  std::unique_ptr<Tracker> feature_engagement_tracker_;
  variations::testing::VariationParamsManager params_manager_;

 private:
  std::unique_ptr<TestingProfileManager> testing_profile_manager_;
  base::test::ScopedFeatureList scoped_feature_list_;
  content::TestBrowserThreadBundle thread_bundle_;
  std::map<std::string, base::FieldTrial*> trials_;

  DISALLOW_COPY_AND_ASSIGN(IncognitoWindowTrackerTest);
};

}  // namespace

// Tests to verify IncognitoWindowFeatureEngagementTracker functional
// expectations:

// Test that a promo is not shown if the user has not opened Incognito Window.
// If OnIncognitoWindowOpened() is called, the ShouldShowPromo() should return
// false.
TEST_F(IncognitoWindowTrackerTest, TestShouldShowPromo) {
  EXPECT_FALSE(incognito_window_tracker_->ShouldShowPromo());

  incognito_window_tracker_->OnSessionTimeMet();

  EXPECT_TRUE(incognito_window_tracker_->ShouldShowPromo());

  incognito_window_tracker_->OnIncognitoWindowOpened();

  EXPECT_FALSE(incognito_window_tracker_->ShouldShowPromo());
}

}  // namespace feature_engagement
