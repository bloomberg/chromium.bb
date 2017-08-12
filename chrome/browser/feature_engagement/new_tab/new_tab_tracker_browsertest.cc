// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/feature_engagement/new_tab/new_tab_tracker.h"

#include "base/run_loop.h"
#include "chrome/browser/feature_engagement/new_tab/new_tab_tracker_factory.h"
#include "chrome/browser/feature_engagement/tracker_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/views/feature_promos/new_tab_promo_bubble_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/tabs/new_tab_button.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "components/feature_engagement/public/event_constants.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/feature_engagement/public/tracker.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/omnibox/browser/omnibox_edit_model.h"
#include "components/omnibox/browser/omnibox_view.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/views/widget/widget.h"
#include "url/gurl.h"

namespace feature_engagement {

namespace {

MATCHER_P(IsFeature, feature, "") {
  return arg.name == feature.name;
}

class MockTracker : public Tracker {
 public:
  MockTracker() = default;
  MOCK_METHOD1(NotifyEvent, void(const std::string& event));
  TriggerState GetTriggerState(const base::Feature& feature) override {
    return Tracker::TriggerState::HAS_NOT_BEEN_DISPLAYED;
  }
  MOCK_METHOD1(ShouldTriggerHelpUI, bool(const base::Feature& feature));
  MOCK_METHOD1(Dismissed, void(const base::Feature& feature));
  MOCK_METHOD0(IsInitialized, bool());
  void AddOnInitializedCallback(OnInitializedCallback callback) override {}
};

std::unique_ptr<KeyedService> BuildTestTrackerFactory(
    content::BrowserContext* context) {
  return base::MakeUnique<testing::StrictMock<MockTracker>>();
}

class NewTabTrackerBrowserTest : public InProcessBrowserTest {
 public:
  NewTabTrackerBrowserTest() = default;
  ~NewTabTrackerBrowserTest() override = default;

  void SetUpOnMainThread() override {
    TrackerFactory::GetInstance()->SetTestingFactoryAndUse(
        browser()->profile(), &BuildTestTrackerFactory);

    // Ensure all initialization is finished.
    base::RunLoop().RunUntilIdle();

    feature_engagement_tracker_ = static_cast<MockTracker*>(
        TrackerFactory::GetForBrowserContext(browser()->profile()));

    EXPECT_CALL(*feature_engagement_tracker_, IsInitialized())
        .WillOnce(::testing::Return(true));

    ASSERT_TRUE(TrackerFactory::GetForBrowserContext(browser()->profile())
                    ->IsInitialized());
  }

  GURL GetGoogleURL() { return GURL("http://www.google.com/"); }

 protected:
  // Owned by the Profile.
  MockTracker* feature_engagement_tracker_;
};

}  // namespace

IN_PROC_BROWSER_TEST_F(NewTabTrackerBrowserTest, TestShowPromo) {
  // Bypassing the 2 hour active session time requirement.
  EXPECT_CALL(*feature_engagement_tracker_,
              NotifyEvent(events::kNewTabSessionTimeMet));
  NewTabTrackerFactory::GetInstance()
      ->GetForProfile(browser()->profile())
      ->OnSessionTimeMet();

  // Navigate in the omnibox.
  EXPECT_CALL(*feature_engagement_tracker_,
              NotifyEvent(events::kOmniboxInteraction));
  ui_test_utils::NavigateToURL(browser(), GetGoogleURL());
  OmniboxView* omnibox_view =
      browser()->window()->GetLocationBar()->GetOmniboxView();
  omnibox_view->OnBeforePossibleChange();
  omnibox_view->SetUserText(base::ASCIIToUTF16("http://www.chromium.org/"));
  omnibox_view->OnAfterPossibleChange(true);
  omnibox_view->model()->AcceptInput(WindowOpenDisposition::CURRENT_TAB, false);

  // Focus on the omnibox.
  EXPECT_CALL(*feature_engagement_tracker_,
              ShouldTriggerHelpUI(IsFeature(kIPHNewTabFeature)))
      .WillOnce(::testing::Return(true))
      .WillRepeatedly(::testing::Return(false));
  chrome::FocusLocationBar(browser());

  EXPECT_TRUE(BrowserView::GetBrowserViewForBrowser(browser())
                  ->tabstrip()
                  ->new_tab_button()
                  ->new_tab_promo()
                  ->GetWidget()
                  ->IsVisible());

  // Tracker::Dismissed() must be invoked when the promo is closed. This will
  // clear the flag for whether there is any in-product help being displayed.
  EXPECT_CALL(*feature_engagement_tracker_,
              Dismissed(IsFeature(kIPHNewTabFeature)));

  BrowserView::GetBrowserViewForBrowser(browser())
      ->tabstrip()
      ->new_tab_button()
      ->new_tab_promo()
      ->GetWidget()
      ->Close();
}

}  // namespace feature_engagement
