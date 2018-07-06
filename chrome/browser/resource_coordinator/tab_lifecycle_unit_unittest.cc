// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/resource_coordinator/tab_lifecycle_unit.h"

#include <memory>

#include "base/macros.h"
#include "base/observer_list.h"
#include "base/test/simple_test_tick_clock.h"
#include "build/build_config.h"
#include "chrome/browser/resource_coordinator/lifecycle_unit_observer.h"
#include "chrome/browser/resource_coordinator/local_site_characteristics_data_unittest_utils.h"
#include "chrome/browser/resource_coordinator/local_site_characteristics_webcontents_observer.h"
#include "chrome/browser/resource_coordinator/tab_helper.h"
#include "chrome/browser/resource_coordinator/tab_lifecycle_observer.h"
#include "chrome/browser/resource_coordinator/tab_lifecycle_unit_external.h"
#include "chrome/browser/resource_coordinator/tab_lifecycle_unit_source.h"
#include "chrome/browser/resource_coordinator/tab_load_tracker.h"
#include "chrome/browser/resource_coordinator/test_lifecycle_unit.h"
#include "chrome/browser/resource_coordinator/time.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/test_tab_strip_model_delegate.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "chrome/browser/media/webrtc/media_stream_capture_indicator.h"

namespace resource_coordinator {

namespace {

using LoadingState = TabLoadTracker::LoadingState;

constexpr base::TimeDelta kShortDelay = base::TimeDelta::FromSeconds(1);

class MockTabLifecycleObserver : public TabLifecycleObserver {
 public:
  MockTabLifecycleObserver() = default;

  MOCK_METHOD2(OnDiscardedStateChange,
               void(content::WebContents* contents, bool is_discarded));
  MOCK_METHOD2(OnAutoDiscardableStateChange,
               void(content::WebContents* contents, bool is_auto_discardable));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockTabLifecycleObserver);
};

}  // namespace

class MockLifecycleUnitObserver : public LifecycleUnitObserver {
 public:
  MockLifecycleUnitObserver() = default;

  MOCK_METHOD2(OnLifecycleUnitStateChanged,
               void(LifecycleUnit*, LifecycleUnitState));
  MOCK_METHOD1(OnLifecycleUnitDestroyed, void(LifecycleUnit*));
  MOCK_METHOD2(OnLifecycleUnitVisibilityChanged,
               void(LifecycleUnit*, content::Visibility));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockLifecycleUnitObserver);
};

class TabLifecycleUnitTest : public testing::ChromeTestHarnessWithLocalDB {
 protected:
  using TabLifecycleUnit = TabLifecycleUnitSource::TabLifecycleUnit;

  TabLifecycleUnitTest() : scoped_set_tick_clock_for_testing_(&test_clock_) {
    observers_.AddObserver(&observer_);
  }

  void SetUp() override {
    ChromeTestHarnessWithLocalDB::SetUp();

    std::unique_ptr<content::WebContents> test_web_contents =
        CreateTestWebContents();
    web_contents_ = test_web_contents.get();
    ResourceCoordinatorTabHelper::CreateForWebContents(web_contents_);
    // Commit an URL to allow discarding.
    content::WebContentsTester::For(web_contents_)
        ->NavigateAndCommit(GURL("https://www.example.com"));

    tab_strip_model_ =
        std::make_unique<TabStripModel>(&tab_strip_model_delegate_, profile());
    tab_strip_model_->AppendWebContents(std::move(test_web_contents), false);
    web_contents_->WasHidden();

    std::unique_ptr<content::WebContents> second_web_contents =
        CreateTestWebContents();
    content::WebContents* raw_second_web_contents = second_web_contents.get();
    tab_strip_model_->AppendWebContents(std::move(second_web_contents),
                                        /*foreground=*/true);
    raw_second_web_contents->WasHidden();

    testing::WaitForLocalDBEntryToBeInitialized(
        web_contents_,
        base::BindRepeating([]() { base::RunLoop().RunUntilIdle(); }));

    testing::ExpireLocalDBObservationWindows(web_contents_);
  }

  void TearDown() override {
    while (!tab_strip_model_->empty())
      tab_strip_model_->DetachWebContentsAt(0);
    tab_strip_model_.reset();
    ChromeTestHarnessWithLocalDB::TearDown();
  }

  void TestCannotDiscardBasedOnHeuristicUsage(
      DecisionFailureReason failure_reason,
      void (SiteCharacteristicsDataWriter::*notify_feature_usage_method)());

  // Create a new test WebContents and append it to the tab strip to allow
  // testing discarding and freezing operations on it. The returned WebContents
  // is in the hidden state.
  content::WebContents* AddNewHiddenWebContentsToTabStrip() {
    std::unique_ptr<content::WebContents> test_web_contents =
        CreateTestWebContents();
    content::WebContents* web_contents = test_web_contents.get();
    ResourceCoordinatorTabHelper::CreateForWebContents(web_contents);
    tab_strip_model_->AppendWebContents(std::move(test_web_contents), false);
    web_contents->WasHidden();
    return web_contents;
  }

  ::testing::StrictMock<MockTabLifecycleObserver> observer_;
  base::ObserverList<TabLifecycleObserver> observers_;
  content::WebContents* web_contents_;  // Owned by tab_strip_model_.
  std::unique_ptr<TabStripModel> tab_strip_model_;
  base::SimpleTestTickClock test_clock_;

 private:
  TestTabStripModelDelegate tab_strip_model_delegate_;
  ScopedSetTickClockForTesting scoped_set_tick_clock_for_testing_;

  DISALLOW_COPY_AND_ASSIGN(TabLifecycleUnitTest);
};

void TabLifecycleUnitTest::TestCannotDiscardBasedOnHeuristicUsage(
    DecisionFailureReason failure_reason,
    void (SiteCharacteristicsDataWriter::*notify_feature_usage_method)()) {
  testing::GetLocalSiteCharacteristicsDataImplForWC(web_contents_)
      ->ClearObservationsAndInvalidateReadOperationForTesting();
  TabLifecycleUnit tab_lifecycle_unit(&observers_, web_contents_,
                                      tab_strip_model_.get());
  auto* observer = ResourceCoordinatorTabHelper::FromWebContents(web_contents_)
                       ->local_site_characteristics_wc_observer_for_testing();
  test_clock_.Advance(base::TimeDelta::FromSeconds(1));
  testing::MarkWebContentsAsLoadedInBackground(web_contents_);
  if (notify_feature_usage_method) {
    // If |notify_feature_usage_method| is not null then all the observation
    // windows should be expired to make sure that |CanDiscard| doesn't return
    // false simply because of a lack of observations.
    testing::ExpireLocalDBObservationWindows(web_contents_);
    (observer->GetWriterForTesting()->*notify_feature_usage_method)();
  }
  {
    DecisionDetails decision_details;
    EXPECT_FALSE(tab_lifecycle_unit.CanDiscard(DiscardReason::kProactive,
                                               &decision_details));
    EXPECT_FALSE(decision_details.IsPositive());
    EXPECT_EQ(failure_reason, decision_details.FailureReason());
  }
  {
    DecisionDetails decision_details;
    EXPECT_FALSE(tab_lifecycle_unit.CanDiscard(DiscardReason::kExternal,
                                               &decision_details));
    EXPECT_FALSE(decision_details.IsPositive());
    EXPECT_EQ(failure_reason, decision_details.FailureReason());
  }
  // Heuristics shouldn't be considered for urgent tab discarding.
  {
    DecisionDetails decision_details;
    EXPECT_TRUE(tab_lifecycle_unit.CanDiscard(DiscardReason::kUrgent,
                                              &decision_details));
    EXPECT_TRUE(decision_details.IsPositive());
  }

  testing::GetLocalSiteCharacteristicsDataImplForWC(web_contents_)
      ->NotifySiteUnloaded(TabVisibility::kBackground);
}

TEST_F(TabLifecycleUnitTest, AsTabLifecycleUnitExternal) {
  TabLifecycleUnit tab_lifecycle_unit(&observers_, web_contents_,
                                      tab_strip_model_.get());
  EXPECT_EQ(&tab_lifecycle_unit,
            tab_lifecycle_unit.AsTabLifecycleUnitExternal());
}

TEST_F(TabLifecycleUnitTest, CanDiscardByDefault) {
  TabLifecycleUnit tab_lifecycle_unit(&observers_, web_contents_,
                                      tab_strip_model_.get());
  ExpectCanDiscardTrueAllReasons(&tab_lifecycle_unit);
}

TEST_F(TabLifecycleUnitTest, SetFocused) {
  TabLifecycleUnit tab_lifecycle_unit(&observers_, web_contents_,
                                      tab_strip_model_.get());
  EXPECT_EQ(NowTicks(), tab_lifecycle_unit.GetLastFocusedTime());
  ExpectCanDiscardTrueAllReasons(&tab_lifecycle_unit);

  tab_lifecycle_unit.SetFocused(true);
  tab_strip_model_->ActivateTabAt(0, false);
  web_contents_->WasShown();
  EXPECT_EQ(base::TimeTicks::Max(), tab_lifecycle_unit.GetLastFocusedTime());
  ExpectCanDiscardFalseAllReasons(&tab_lifecycle_unit,
                                  DecisionFailureReason::LIVE_STATE_VISIBLE);

  tab_lifecycle_unit.SetFocused(false);
  tab_strip_model_->ActivateTabAt(1, false);
  web_contents_->WasHidden();
  EXPECT_EQ(test_clock_.NowTicks(), tab_lifecycle_unit.GetLastFocusedTime());
  ExpectCanDiscardTrueAllReasons(&tab_lifecycle_unit);
}

TEST_F(TabLifecycleUnitTest, AutoDiscardable) {
  TabLifecycleUnit tab_lifecycle_unit(&observers_, web_contents_,
                                      tab_strip_model_.get());

  EXPECT_TRUE(tab_lifecycle_unit.IsAutoDiscardable());
  ExpectCanDiscardTrueAllReasons(&tab_lifecycle_unit);

  EXPECT_CALL(observer_, OnAutoDiscardableStateChange(web_contents_, false));
  tab_lifecycle_unit.SetAutoDiscardable(false);
  ::testing::Mock::VerifyAndClear(&observer_);
  EXPECT_FALSE(tab_lifecycle_unit.IsAutoDiscardable());
  ExpectCanDiscardFalseAllReasons(
      &tab_lifecycle_unit,
      DecisionFailureReason::LIVE_STATE_EXTENSION_DISALLOWED);

  EXPECT_CALL(observer_, OnAutoDiscardableStateChange(web_contents_, true));
  tab_lifecycle_unit.SetAutoDiscardable(true);
  ::testing::Mock::VerifyAndClear(&observer_);
  EXPECT_TRUE(tab_lifecycle_unit.IsAutoDiscardable());
  ExpectCanDiscardTrueAllReasons(&tab_lifecycle_unit);
}

TEST_F(TabLifecycleUnitTest, CannotDiscardCrashed) {
  TabLifecycleUnit tab_lifecycle_unit(&observers_, web_contents_,
                                      tab_strip_model_.get());

  web_contents_->SetIsCrashed(base::TERMINATION_STATUS_PROCESS_CRASHED, 0);
  ExpectCanDiscardFalseTrivialAllReasons(&tab_lifecycle_unit);
}

#if !defined(OS_CHROMEOS)
TEST_F(TabLifecycleUnitTest, CannotDiscardActive) {
  TabLifecycleUnit tab_lifecycle_unit(&observers_, web_contents_,
                                      tab_strip_model_.get());

  tab_strip_model_->ActivateTabAt(0, false);

  ExpectCanDiscardFalseAllReasons(&tab_lifecycle_unit,
                                  DecisionFailureReason::LIVE_STATE_VISIBLE);
}
#endif  // !defined(OS_CHROMEOS)

TEST_F(TabLifecycleUnitTest, CannotDiscardInvalidURL) {
  content::WebContents* web_contents = AddNewHiddenWebContentsToTabStrip();
  TabLifecycleUnit tab_lifecycle_unit(&observers_, web_contents,
                                      tab_strip_model_.get());
  // TODO(sebmarchand): Fix this test, this doesn't really test that it's not
  // possible to discard an invalid URL, TestWebContents::GetLastCommittedURL()
  // doesn't return the URL set with "SetLastCommittedURL" if this one is
  // invalid.
  content::WebContentsTester::For(web_contents)
      ->SetLastCommittedURL(GURL("Invalid :)"));
  ExpectCanDiscardFalseTrivialAllReasons(&tab_lifecycle_unit);
}

TEST_F(TabLifecycleUnitTest, CannotDiscardEmptyURL) {
  content::WebContents* web_contents = AddNewHiddenWebContentsToTabStrip();
  TabLifecycleUnit tab_lifecycle_unit(&observers_, web_contents,
                                      tab_strip_model_.get());

  ExpectCanDiscardFalseTrivialAllReasons(&tab_lifecycle_unit);
}

TEST_F(TabLifecycleUnitTest, CannotDiscardVideoCapture) {
  TabLifecycleUnit tab_lifecycle_unit(&observers_, web_contents_,
                                      tab_strip_model_.get());

  content::MediaStreamDevices video_devices(1);
  video_devices[0] =
      content::MediaStreamDevice(content::MEDIA_DEVICE_VIDEO_CAPTURE,
                                 "fake_media_device", "fake_media_device");
  MediaCaptureDevicesDispatcher* dispatcher =
      MediaCaptureDevicesDispatcher::GetInstance();
  dispatcher->SetTestVideoCaptureDevices(video_devices);
  std::unique_ptr<content::MediaStreamUI> video_stream_ui =
      dispatcher->GetMediaStreamCaptureIndicator()->RegisterMediaStream(
          web_contents_, video_devices);
  video_stream_ui->OnStarted(base::RepeatingClosure());

  ExpectCanDiscardFalseAllReasons(&tab_lifecycle_unit,
                                  DecisionFailureReason::LIVE_STATE_CAPTURING);

  video_stream_ui.reset();

  ExpectCanDiscardTrueAllReasons(&tab_lifecycle_unit);
}

TEST_F(TabLifecycleUnitTest, CannotDiscardRecentlyAudible) {
  TabLifecycleUnit tab_lifecycle_unit(&observers_, web_contents_,
                                      tab_strip_model_.get());

  // Cannot discard when the "recently audible" bit is set.
  tab_lifecycle_unit.SetRecentlyAudible(true);
  ExpectCanDiscardFalseAllReasons(
      &tab_lifecycle_unit, DecisionFailureReason::LIVE_STATE_PLAYING_AUDIO);

  // The "recently audible" bit is still set. The tab cannot be discarded.
  test_clock_.Advance(kTabAudioProtectionTime);
  ExpectCanDiscardFalseAllReasons(
      &tab_lifecycle_unit, DecisionFailureReason::LIVE_STATE_PLAYING_AUDIO);

  // The "recently audible" bit was unset less than
  // kTabAudioProtectionTime ago. The tab cannot be discarded.
  tab_lifecycle_unit.SetRecentlyAudible(false);
  test_clock_.Advance(kShortDelay);
  ExpectCanDiscardFalseAllReasons(
      &tab_lifecycle_unit, DecisionFailureReason::LIVE_STATE_PLAYING_AUDIO);

  // The "recently audible" bit was unset kTabAudioProtectionTime ago. The tab
  // can be discarded.
  test_clock_.Advance(kTabAudioProtectionTime - kShortDelay);
  ExpectCanDiscardTrueAllReasons(&tab_lifecycle_unit);

  // Calling SetRecentlyAudible(false) again does not change the fact that the
  // tab can be discarded.
  tab_lifecycle_unit.SetRecentlyAudible(false);
  ExpectCanDiscardTrueAllReasons(&tab_lifecycle_unit);
}

TEST_F(TabLifecycleUnitTest, CanDiscardNeverAudibleTab) {
  TabLifecycleUnit tab_lifecycle_unit(&observers_, web_contents_,
                                      tab_strip_model_.get());

  tab_lifecycle_unit.SetRecentlyAudible(false);
  // Since the tab was never audible, it should be possible to discard it,
  // even if there was a recent call to SetRecentlyAudible(false).
  ExpectCanDiscardTrueAllReasons(&tab_lifecycle_unit);
}

TEST_F(TabLifecycleUnitTest, CannotDiscardPDF) {
  TabLifecycleUnit tab_lifecycle_unit(&observers_, web_contents_,
                                      tab_strip_model_.get());

  content::WebContentsTester::For(web_contents_)
      ->SetMainFrameMimeType("application/pdf");
  ExpectCanDiscardFalseAllReasons(&tab_lifecycle_unit,
                                  DecisionFailureReason::LIVE_STATE_IS_PDF);
}

TEST_F(TabLifecycleUnitTest, CannotProactivelyDiscardTabWithAudioHeuristic) {
  TestCannotDiscardBasedOnHeuristicUsage(
      DecisionFailureReason::HEURISTIC_AUDIO,
      &SiteCharacteristicsDataWriter::NotifyUsesAudioInBackground);
}

TEST_F(TabLifecycleUnitTest, CannotProactivelyDiscardTabWithFaviconHeuristic) {
  TestCannotDiscardBasedOnHeuristicUsage(
      DecisionFailureReason::HEURISTIC_FAVICON,
      &SiteCharacteristicsDataWriter::NotifyUpdatesFaviconInBackground);
}

TEST_F(TabLifecycleUnitTest,
       CannotProactivelyDiscardTabWithNotificationsHeuristic) {
  TestCannotDiscardBasedOnHeuristicUsage(
      DecisionFailureReason::HEURISTIC_NOTIFICATIONS,
      &SiteCharacteristicsDataWriter::NotifyUsesNotificationsInBackground);
}

TEST_F(TabLifecycleUnitTest, CannotProactivelyDiscardTabWithTitleHeuristic) {
  TestCannotDiscardBasedOnHeuristicUsage(
      DecisionFailureReason::HEURISTIC_TITLE,
      &SiteCharacteristicsDataWriter::NotifyUpdatesTitleInBackground);
}

TEST_F(TabLifecycleUnitTest,
       CannotProactivelyDiscardTabIfInsufficientObservation) {
  TestCannotDiscardBasedOnHeuristicUsage(
      DecisionFailureReason::HEURISTIC_INSUFFICIENT_OBSERVATION, nullptr);
}

TEST_F(TabLifecycleUnitTest, CannotFreezeAFrozenTab) {
  TabLifecycleUnit tab_lifecycle_unit(&observers_, web_contents_,
                                      tab_strip_model_.get());
  TabLoadTracker::Get()->TransitionStateForTesting(web_contents_,
                                                   LoadingState::LOADED);
  {
    DecisionDetails decision_details;
    EXPECT_TRUE(tab_lifecycle_unit.CanFreeze(&decision_details));
  }
  tab_lifecycle_unit.Freeze();
  {
    DecisionDetails decision_details;
    EXPECT_FALSE(tab_lifecycle_unit.CanFreeze(&decision_details));
  }
}

TEST_F(TabLifecycleUnitTest, NotifiedOfWebContentsVisibilityChanges) {
  TabLifecycleUnit tab_lifecycle_unit(&observers_, web_contents_,
                                      tab_strip_model_.get());

  ::testing::StrictMock<MockLifecycleUnitObserver> observer;
  tab_lifecycle_unit.AddObserver(&observer);

  EXPECT_CALL(observer, OnLifecycleUnitVisibilityChanged(
                            &tab_lifecycle_unit, content::Visibility::VISIBLE));
  web_contents_->WasShown();
  ::testing::Mock::VerifyAndClear(&observer);

  EXPECT_CALL(observer, OnLifecycleUnitVisibilityChanged(
                            &tab_lifecycle_unit, content::Visibility::HIDDEN));
  web_contents_->WasHidden();
  ::testing::Mock::VerifyAndClear(&observer);

  EXPECT_CALL(observer, OnLifecycleUnitVisibilityChanged(
                            &tab_lifecycle_unit, content::Visibility::VISIBLE));
  web_contents_->WasShown();
  ::testing::Mock::VerifyAndClear(&observer);

  EXPECT_CALL(observer,
              OnLifecycleUnitVisibilityChanged(&tab_lifecycle_unit,
                                               content::Visibility::OCCLUDED));
  web_contents_->WasOccluded();
  ::testing::Mock::VerifyAndClear(&observer);

  tab_lifecycle_unit.RemoveObserver(&observer);
}

}  // namespace resource_coordinator
