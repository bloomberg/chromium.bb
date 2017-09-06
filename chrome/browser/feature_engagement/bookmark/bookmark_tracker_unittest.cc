// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/feature_engagement/bookmark/bookmark_tracker.h"

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

const char kBookmarkTrialName[] = "BookmarkTrial";
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

class FakeBookmarkTracker : public BookmarkTracker {
 public:
  FakeBookmarkTracker(Tracker* feature_tracker, Profile* profile)
      : BookmarkTracker(
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

class BookmarkTrackerEventTest : public testing::Test {
 public:
  BookmarkTrackerEventTest() = default;
  ~BookmarkTrackerEventTest() override = default;

  void SetUp() override {
    // Start the DesktopSessionDurationTracker to track active session time.
    metrics::DesktopSessionDurationTracker::Initialize();
    testing_profile_manager_ = base::MakeUnique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal());
    ASSERT_TRUE(testing_profile_manager_->SetUp());
    mock_tracker_ = base::MakeUnique<testing::StrictMock<MockTracker>>();
    bookmark_tracker_ = base::MakeUnique<FakeBookmarkTracker>(
        mock_tracker_.get(),
        testing_profile_manager_->CreateTestingProfile(kTestProfileName));
  }

  void TearDown() override {
    bookmark_tracker_->RemoveSessionDurationObserver();
    metrics::DesktopSessionDurationTracker::CleanupForTesting();
    testing_profile_manager_.reset();
  }

 protected:
  std::unique_ptr<TestingProfileManager> testing_profile_manager_;
  std::unique_ptr<MockTracker> mock_tracker_;
  std::unique_ptr<FakeBookmarkTracker> bookmark_tracker_;

 private:
  content::TestBrowserThreadBundle thread_bundle_;

  DISALLOW_COPY_AND_ASSIGN(BookmarkTrackerEventTest);
};

}  // namespace

// Tests to verify FeatureEngagementTracker API boundary expectations:

// If OnBookmarkAdded() is called, the FeatureEngagementTracker
// receives the kBookmarkAddedEvent.
TEST_F(BookmarkTrackerEventTest, TestOnBookmarkAdded) {
  EXPECT_CALL(*mock_tracker_, NotifyEvent(events::kBookmarkAdded));
  bookmark_tracker_->OnBookmarkAdded();
}

// If OnSessionTimeMet() is called, the FeatureEngagementTracker
// receives the kSessionTime event.
TEST_F(BookmarkTrackerEventTest, TestOnSessionTimeMet) {
  EXPECT_CALL(*mock_tracker_, NotifyEvent(events::kBookmarkSessionTimeMet));
  bookmark_tracker_->OnSessionTimeMet();
}

namespace {

class BookmarkTrackerTest : public testing::Test {
 public:
  BookmarkTrackerTest() = default;
  ~BookmarkTrackerTest() override = default;

  void SetUp() override {
    // Set up the kBookmarkTrialName field trial.
    base::FieldTrial* bookmark_trial =
        base::FieldTrialList::CreateFieldTrial(kBookmarkTrialName, kGroupName);
    trials_[kIPHBookmarkFeature.name] = bookmark_trial;

    std::unique_ptr<base::FeatureList> feature_list =
        base::MakeUnique<base::FeatureList>();
    feature_list->RegisterFieldTrialOverride(
        kIPHBookmarkFeature.name, base::FeatureList::OVERRIDE_ENABLE_FEATURE,
        bookmark_trial);

    scoped_feature_list_.InitWithFeatureList(std::move(feature_list));
    ASSERT_EQ(bookmark_trial,
              base::FeatureList::GetFieldTrial(kIPHBookmarkFeature));

    std::map<std::string, std::string> bookmark_params;
    bookmark_params["event_bookmark_added"] =
        "name:bookmark_added;comparator:==0;window:3650;storage:3650";
    bookmark_params["event_bookmark_session_time_met"] =
        "name:bookmark_session_time_met;comparator:>=1;window:3650;storage:"
        "3650";
    bookmark_params["event_trigger"] =
        "name:bookmark_trigger;comparator:any;window:3650;storage:3650";
    bookmark_params["event_used"] =
        "name:bookmark_clicked;comparator:any;window:3650;storage:3650";
    bookmark_params["session_rate"] = "<=3";
    bookmark_params["availability"] = "any";

    SetFeatureParams(kIPHBookmarkFeature, bookmark_params);

    // Start the DesktopSessionDurationTracker to track active session time.
    metrics::DesktopSessionDurationTracker::Initialize();

    testing_profile_manager_ = base::MakeUnique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal());
    ASSERT_TRUE(testing_profile_manager_->SetUp());

    feature_engagement_tracker_ = CreateTestTracker();

    bookmark_tracker_ = base::MakeUnique<FakeBookmarkTracker>(
        feature_engagement_tracker_.get(),
        testing_profile_manager_->CreateTestingProfile(kTestProfileName));

    // The feature engagement tracker does async initialization.
    base::RunLoop().RunUntilIdle();
    ASSERT_TRUE(feature_engagement_tracker_->IsInitialized());
  }

  void TearDown() override {
    bookmark_tracker_->RemoveSessionDurationObserver();
    testing_profile_manager_->DeleteTestingProfile(kTestProfileName);
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
  std::unique_ptr<FakeBookmarkTracker> bookmark_tracker_;
  std::unique_ptr<Tracker> feature_engagement_tracker_;
  variations::testing::VariationParamsManager params_manager_;

 private:
  std::unique_ptr<TestingProfileManager> testing_profile_manager_;
  base::test::ScopedFeatureList scoped_feature_list_;
  content::TestBrowserThreadBundle thread_bundle_;
  std::map<std::string, base::FieldTrial*> trials_;

  DISALLOW_COPY_AND_ASSIGN(BookmarkTrackerTest);
};

}  // namespace

// Tests to verify BookmarkFeatureEngagementTracker functional expectations:

// Test that a promo is not shown if the user has a Bookmark. If
// OnBookmarkAdded() is called, the ShouldShowPromo() should return false.
TEST_F(BookmarkTrackerTest, TestShouldShowPromo) {
  EXPECT_FALSE(bookmark_tracker_->ShouldShowPromo());

  bookmark_tracker_->OnSessionTimeMet();

  EXPECT_TRUE(bookmark_tracker_->ShouldShowPromo());

  bookmark_tracker_->OnBookmarkAdded();

  EXPECT_FALSE(bookmark_tracker_->ShouldShowPromo());
}

}  // namespace feature_engagement
