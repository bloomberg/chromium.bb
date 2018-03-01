// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/power/ml/user_activity_logger_delegate_ukm.h"

#include <memory>
#include <vector>

#include "chrome/browser/chromeos/power/ml/user_activity_event.pb.h"
#include "chrome/browser/engagement/site_engagement_service.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tabs/tab_activity_simulator.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_ukm_test_helper.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/test_browser_window_aura.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/web_contents_tester.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace power {
namespace ml {

using ukm::builders::UserActivity;
using ukm::builders::UserActivityId;
using content::WebContentsTester;

class UserActivityLoggerDelegateUkmTest
    : public ChromeRenderViewHostTestHarness {
 public:
  UserActivityLoggerDelegateUkmTest() {
    // These values are arbitrary but must correspond with the values
    // in |user_activity_values_|.
    UserActivityEvent::Event* event = user_activity_event_.mutable_event();
    event->set_log_duration_sec(395);
    event->set_reason(UserActivityEvent::Event::USER_ACTIVITY);
    event->set_type(UserActivityEvent::Event::REACTIVATE);

    // In the order of metrics names in ukm.
    UserActivityEvent::Features* features =
        user_activity_event_.mutable_features();
    features->set_battery_percent(96.0);
    features->set_device_management(UserActivityEvent::Features::UNMANAGED);
    features->set_device_mode(UserActivityEvent::Features::CLAMSHELL);
    features->set_device_type(UserActivityEvent::Features::CHROMEBOOK);
    features->set_last_activity_day(UserActivityEvent::Features::MON);
    features->set_last_activity_time_sec(7300);
    features->set_last_user_activity_time_sec(3800);
    features->set_key_events_in_last_hour(20000);
    features->set_recent_time_active_sec(10);
    features->set_video_playing_time_sec(800);
    features->set_on_to_dim_sec(100);
    features->set_dim_to_screen_off_sec(200);
    features->set_time_since_last_mouse_sec(100);
    features->set_time_since_last_touch_sec(311);
    features->set_time_since_video_ended_sec(400);
    features->set_mouse_events_in_last_hour(89);
    features->set_touch_events_in_last_hour(1890);
  }

  void UpdateOpenTabsURLs() {
    user_activity_logger_delegate_ukm_.UpdateOpenTabsURLs();
  }

  void LogActivity(const UserActivityEvent& event) {
    user_activity_logger_delegate_ukm_.LogActivity(event);
  }

  // Creates a test browser window and sets its visibility, activity and
  // incognito status.
  std::unique_ptr<Browser> CreateTestBrowser(bool is_visible,
                                             bool is_focused,
                                             bool is_incognito = false) {
    Profile* const original_profile = profile();
    Profile* const used_profile =
        is_incognito ? original_profile->GetOffTheRecordProfile()
                     : original_profile;
    Browser::CreateParams params(used_profile, true);

    std::unique_ptr<aura::Window> dummy_window(new aura::Window(nullptr));
    dummy_window->Init(ui::LAYER_SOLID_COLOR);
    root_window()->AddChild(dummy_window.get());
    dummy_window->SetBounds(gfx::Rect(root_window()->bounds().size()));
    if (is_visible) {
      dummy_window->Show();
    } else {
      dummy_window->Hide();
    }

    std::unique_ptr<Browser> browser =
        chrome::CreateBrowserWithAuraTestWindowForParams(
            std::move(dummy_window), &params);
    if (is_focused) {
      browser->window()->Activate();
    } else {
      browser->window()->Deactivate();
    }
    return browser;
  }

  // Adds a tab with specified url to the tab strip model. Also optionally sets
  // the tab to be the active one in the tab strip model.
  // If |mime_type| is an empty string, the content has a default text type.
  // TODO(jiameng): there doesn't seem to be a way to set form entry (via
  // page importance signal). Check if there's some other way to set it.
  void CreateTestWebContents(TabStripModel* const tab_strip_model,
                             const GURL& url,
                             bool is_active,
                             const std::string& mime_type = "") {
    DCHECK(tab_strip_model);
    DCHECK(!url.is_empty());
    content::WebContents* contents =
        tab_activity_simulator_.AddWebContentsAndNavigate(tab_strip_model, url);
    if (is_active) {
      tab_strip_model->ActivateTabAt(tab_strip_model->count() - 1, false);
    }
    if (!mime_type.empty())
      WebContentsTester::For(contents)->SetMainFrameMimeType(mime_type);

    WebContentsTester::For(contents)->TestSetIsLoading(false);
  }

 protected:
  UserActivityEvent user_activity_event_;
  UkmEntryChecker ukm_entry_checker_;
  TabActivitySimulator tab_activity_simulator_;

  const GURL url1_ = GURL("https://example1.com/");
  const GURL url2_ = GURL("https://example2.com/");
  const GURL url3_ = GURL("https://example3.com/");
  const GURL url4_ = GURL("https://example4.com/");

  const UkmMetricMap user_activity_values_ = {
      {UserActivity::kEventLogDurationName, 380},
      {UserActivity::kEventReasonName, UserActivityEvent::Event::USER_ACTIVITY},
      {UserActivity::kEventTypeName, UserActivityEvent::Event::REACTIVATE},
      {UserActivity::kBatteryPercentName, 95},
      {UserActivity::kDeviceManagementName,
       UserActivityEvent::Features::UNMANAGED},
      {UserActivity::kDeviceModeName, UserActivityEvent::Features::CLAMSHELL},
      {UserActivity::kDeviceTypeName, UserActivityEvent::Features::CHROMEBOOK},
      {UserActivity::kLastActivityDayName, UserActivityEvent::Features::MON},
      {UserActivity::kKeyEventsInLastHourName, 10000},
      {UserActivity::kLastActivityTimeName, 2},
      {UserActivity::kLastUserActivityTimeName, 1},
      {UserActivity::kMouseEventsInLastHourName, 89},
      {UserActivity::kOnBatteryName, base::nullopt},
      {UserActivity::kRecentTimeActiveName, 10},
      {UserActivity::kRecentVideoPlayingTimeName, 600},
      {UserActivity::kScreenDimDelayName, 100},
      {UserActivity::kScreenDimToOffDelayName, 200},
      {UserActivity::kSequenceIdName, 1},
      {UserActivity::kTimeSinceLastKeyName, base::nullopt},
      {UserActivity::kTimeSinceLastMouseName, 100},
      {UserActivity::kTimeSinceLastTouchName, 311},
      {UserActivity::kTimeSinceLastVideoEndedName, 360},
      {UserActivity::kTouchEventsInLastHourName, 1000}};

 private:
  UserActivityLoggerDelegateUkm user_activity_logger_delegate_ukm_;
  DISALLOW_COPY_AND_ASSIGN(UserActivityLoggerDelegateUkmTest);
};

TEST_F(UserActivityLoggerDelegateUkmTest, BucketEveryFivePercents) {
  const std::vector<int> original_values = {0, 14, 15, 100};
  const std::vector<int> results = {0, 10, 15, 100};
  constexpr UserActivityLoggerDelegateUkm::Bucket buckets[] = {{100, 5}};

  for (size_t i = 0; i < original_values.size(); ++i) {
    EXPECT_EQ(results[i], UserActivityLoggerDelegateUkm::Bucketize(
                              original_values[i], buckets, arraysize(buckets)));
  }
}

TEST_F(UserActivityLoggerDelegateUkmTest, Bucketize) {
  const std::vector<int> original_values = {0,   18,  59,  60,  62,  69,  72,
                                            299, 300, 306, 316, 599, 600, 602};
  constexpr UserActivityLoggerDelegateUkm::Bucket buckets[] = {
      {60, 1}, {300, 10}, {600, 20}};
  const std::vector<int> results = {0,   18,  59,  60,  60,  60,  70,
                                    290, 300, 300, 300, 580, 600, 600};
  for (size_t i = 0; i < original_values.size(); ++i) {
    EXPECT_EQ(results[i], UserActivityLoggerDelegateUkm::Bucketize(
                              original_values[i], buckets, arraysize(buckets)));
  }
}

TEST_F(UserActivityLoggerDelegateUkmTest, Basic) {
  std::unique_ptr<Browser> browser =
      CreateTestBrowser(true /* is_visible */, true /* is_focused */);
  BrowserList::GetInstance()->SetLastActive(browser.get());
  TabStripModel* tab_strip_model = browser->tab_strip_model();

  CreateTestWebContents(tab_strip_model, url1_, true /* is_active */,
                        "application/pdf");

  // Set engagement score to 95, which will be rounded down to 90 when it's
  // logged to UKM.
  SiteEngagementService::Get(profile())->ResetBaseScoreForURL(url1_, 95);

  const ukm::SourceId source_id1 = ukm_entry_checker_.GetSourceIdForUrl(url1_);

  CreateTestWebContents(tab_strip_model, url2_, false /* is_active */);
  const ukm::SourceId source_id2 = ukm_entry_checker_.GetSourceIdForUrl(url2_);

  UpdateOpenTabsURLs();
  LogActivity(user_activity_event_);

  EXPECT_EQ(1,
            ukm_entry_checker_.NumNewEntriesRecorded(UserActivity::kEntryName));
  EXPECT_EQ(
      2, ukm_entry_checker_.NumNewEntriesRecorded(UserActivityId::kEntryName));

  ukm_entry_checker_.ExpectNewEntries(UserActivity::kEntryName,
                                      {user_activity_values_});

  const ukm::mojom::UkmEntry* last_activity_entry =
      ukm_entry_checker_.LastUkmEntry(UserActivity::kEntryName);

  const ukm::SourceId kSourceId = last_activity_entry->source_id;

  const UkmMetricMap kUserActivityIdValues1(
      {{UserActivityId::kActivityIdName, kSourceId},
       {UserActivityId::kIsActiveName, 1},
       {UserActivityId::kIsBrowserFocusedName, 1},
       {UserActivityId::kIsBrowserVisibleName, 1},
       {UserActivityId::kIsTopmostBrowserName, 1},
       {UserActivityId::kSiteEngagementScoreName, 90},
       {UserActivityId::kContentTypeName,
        metrics::TabMetricsEvent::CONTENT_TYPE_APPLICATION},
       {UserActivityId::kHasFormEntryName, 0}});

  const UkmMetricMap kUserActivityIdValues2(
      {{UserActivityId::kActivityIdName, kSourceId},
       {UserActivityId::kIsActiveName, 0},
       {UserActivityId::kIsBrowserFocusedName, 1},
       {UserActivityId::kIsBrowserVisibleName, 1},
       {UserActivityId::kIsTopmostBrowserName, 1},
       {UserActivityId::kSiteEngagementScoreName, 0},
       {UserActivityId::kContentTypeName,
        metrics::TabMetricsEvent::CONTENT_TYPE_TEXT_HTML},
       {UserActivityId::kHasFormEntryName, 0}});

  const std::map<ukm::SourceId, std::pair<GURL, UkmMetricMap>> expected_data({
      {source_id1, std::make_pair(url1_, kUserActivityIdValues1)},
      {source_id2, std::make_pair(url2_, kUserActivityIdValues2)},
  });

  ukm_entry_checker_.ExpectNewEntriesBySource(UserActivityId::kEntryName,
                                              expected_data);

  tab_strip_model->CloseAllTabs();
}

TEST_F(UserActivityLoggerDelegateUkmTest, MultiBrowsersAndTabs) {
  // Simulates three browsers:
  //  - browser1 is the last active but minimized and so not visible.
  //  - browser2 and browser3 are both visible but browser2 is the topmost.
  std::unique_ptr<Browser> browser1 =
      CreateTestBrowser(false /* is_visible */, false /* is_focused */);
  std::unique_ptr<Browser> browser2 =
      CreateTestBrowser(true /* is_visible */, true /* is_focused */);
  std::unique_ptr<Browser> browser3 =
      CreateTestBrowser(true /* is_visible */, false /* is_focused */);

  BrowserList::GetInstance()->SetLastActive(browser3.get());
  BrowserList::GetInstance()->SetLastActive(browser2.get());
  BrowserList::GetInstance()->SetLastActive(browser1.get());

  TabStripModel* tab_strip_model1 = browser1->tab_strip_model();
  CreateTestWebContents(tab_strip_model1, url1_, false /* is_active */);
  CreateTestWebContents(tab_strip_model1, url2_, true /* is_active */);
  const ukm::SourceId source_id1 = ukm_entry_checker_.GetSourceIdForUrl(url1_);
  const ukm::SourceId source_id2 = ukm_entry_checker_.GetSourceIdForUrl(url2_);

  TabStripModel* tab_strip_model2 = browser2->tab_strip_model();
  CreateTestWebContents(tab_strip_model2, url3_, true /* is_active */);
  const ukm::SourceId source_id3 = ukm_entry_checker_.GetSourceIdForUrl(url3_);

  TabStripModel* tab_strip_model3 = browser3->tab_strip_model();
  CreateTestWebContents(tab_strip_model3, url4_, true /* is_active */);
  const ukm::SourceId source_id4 = ukm_entry_checker_.GetSourceIdForUrl(url4_);

  UpdateOpenTabsURLs();
  LogActivity(user_activity_event_);

  EXPECT_EQ(1,
            ukm_entry_checker_.NumNewEntriesRecorded(UserActivity::kEntryName));
  EXPECT_EQ(
      4, ukm_entry_checker_.NumNewEntriesRecorded(UserActivityId::kEntryName));

  ukm_entry_checker_.ExpectNewEntries(UserActivity::kEntryName,
                                      {user_activity_values_});

  const ukm::mojom::UkmEntry* last_activity_entry =
      ukm_entry_checker_.LastUkmEntry(UserActivity::kEntryName);

  const ukm::SourceId kSourceId = last_activity_entry->source_id;

  const UkmMetricMap kUserActivityIdValues1(
      {{UserActivityId::kActivityIdName, kSourceId},
       {UserActivityId::kIsActiveName, 0},
       {UserActivityId::kIsBrowserFocusedName, 0},
       {UserActivityId::kIsBrowserVisibleName, 0},
       {UserActivityId::kIsTopmostBrowserName, 0}});

  const UkmMetricMap kUserActivityIdValues2(
      {{UserActivityId::kActivityIdName, kSourceId},
       {UserActivityId::kIsActiveName, 1},
       {UserActivityId::kIsBrowserFocusedName, 0},
       {UserActivityId::kIsBrowserVisibleName, 0},
       {UserActivityId::kIsTopmostBrowserName, 0}});

  const UkmMetricMap kUserActivityIdValues3(
      {{UserActivityId::kActivityIdName, kSourceId},
       {UserActivityId::kIsActiveName, 1},
       {UserActivityId::kIsBrowserFocusedName, 1},
       {UserActivityId::kIsBrowserVisibleName, 1},
       {UserActivityId::kIsTopmostBrowserName, 1}});

  const UkmMetricMap kUserActivityIdValues4(
      {{UserActivityId::kActivityIdName, kSourceId},
       {UserActivityId::kIsActiveName, 1},
       {UserActivityId::kIsBrowserFocusedName, 0},
       {UserActivityId::kIsBrowserVisibleName, 1},
       {UserActivityId::kIsTopmostBrowserName, 0}});

  const std::map<ukm::SourceId, std::pair<GURL, UkmMetricMap>> expected_data({
      {source_id1, std::make_pair(url1_, kUserActivityIdValues1)},
      {source_id2, std::make_pair(url2_, kUserActivityIdValues2)},
      {source_id3, std::make_pair(url3_, kUserActivityIdValues3)},
      {source_id4, std::make_pair(url4_, kUserActivityIdValues4)},
  });

  ukm_entry_checker_.ExpectNewEntriesBySource(UserActivityId::kEntryName,
                                              expected_data);

  tab_strip_model1->CloseAllTabs();
  tab_strip_model2->CloseAllTabs();
  tab_strip_model3->CloseAllTabs();
}

TEST_F(UserActivityLoggerDelegateUkmTest, Incognito) {
  std::unique_ptr<Browser> browser = CreateTestBrowser(
      true /* is_visible */, true /* is_focused */, true /* is_incognito */);
  BrowserList::GetInstance()->SetLastActive(browser.get());

  TabStripModel* tab_strip_model = browser->tab_strip_model();
  CreateTestWebContents(tab_strip_model, url1_, true /* is_active */);
  CreateTestWebContents(tab_strip_model, url2_, false /* is_active */);

  UpdateOpenTabsURLs();
  LogActivity(user_activity_event_);

  EXPECT_EQ(1,
            ukm_entry_checker_.NumNewEntriesRecorded(UserActivity::kEntryName));
  EXPECT_EQ(
      0, ukm_entry_checker_.NumNewEntriesRecorded(UserActivityId::kEntryName));

  ukm_entry_checker_.ExpectNewEntries(UserActivity::kEntryName,
                                      {user_activity_values_});

  tab_strip_model->CloseAllTabs();
}

TEST_F(UserActivityLoggerDelegateUkmTest, NoOpenTabs) {
  std::unique_ptr<Browser> browser =
      CreateTestBrowser(true /* is_visible */, true /* is_focused */);

  UpdateOpenTabsURLs();
  LogActivity(user_activity_event_);

  EXPECT_EQ(1,
            ukm_entry_checker_.NumNewEntriesRecorded(UserActivity::kEntryName));
  EXPECT_EQ(
      0, ukm_entry_checker_.NumNewEntriesRecorded(UserActivityId::kEntryName));

  ukm_entry_checker_.ExpectNewEntries(UserActivity::kEntryName,
                                      {user_activity_values_});
}

TEST_F(UserActivityLoggerDelegateUkmTest, TwoUserActivityEvents) {
  std::unique_ptr<Browser> browser =
      CreateTestBrowser(true /* is_visible */, true /* is_focused */);

  // A second event will be logged.
  UserActivityEvent user_activity_event2;
  UserActivityEvent::Event* event = user_activity_event2.mutable_event();
  event->set_log_duration_sec(35);
  event->set_reason(UserActivityEvent::Event::IDLE_SLEEP);
  event->set_type(UserActivityEvent::Event::TIMEOUT);

  UserActivityEvent::Features* features =
      user_activity_event2.mutable_features();
  features->set_battery_percent(86.0);
  features->set_device_management(UserActivityEvent::Features::MANAGED);
  features->set_device_mode(UserActivityEvent::Features::CLAMSHELL);
  features->set_device_type(UserActivityEvent::Features::CHROMEBOOK);
  features->set_last_activity_day(UserActivityEvent::Features::TUE);
  features->set_last_activity_time_sec(7300);
  features->set_last_user_activity_time_sec(3800);
  features->set_recent_time_active_sec(20);
  features->set_on_to_dim_sec(10);
  features->set_dim_to_screen_off_sec(20);
  features->set_time_since_last_mouse_sec(200);

  const UkmMetricMap user_activity_values2 = {
      {UserActivity::kEventLogDurationName, 35},
      {UserActivity::kEventTypeName, UserActivityEvent::Event::TIMEOUT},
      {UserActivity::kEventReasonName, UserActivityEvent::Event::IDLE_SLEEP},
      {UserActivity::kBatteryPercentName, 85},
      {UserActivity::kDeviceManagementName,
       UserActivityEvent::Features::MANAGED},
      {UserActivity::kDeviceModeName, UserActivityEvent::Features::CLAMSHELL},
      {UserActivity::kDeviceTypeName, UserActivityEvent::Features::CHROMEBOOK},
      {UserActivity::kLastActivityDayName, UserActivityEvent::Features::TUE},
      {UserActivity::kLastActivityTimeName, 2},
      {UserActivity::kLastUserActivityTimeName, 1},
      {UserActivity::kOnBatteryName, base::nullopt},
      {UserActivity::kRecentTimeActiveName, 20},
      {UserActivity::kScreenDimDelayName, 10},
      {UserActivity::kScreenDimToOffDelayName, 20},
      {UserActivity::kSequenceIdName, 2},
      {UserActivity::kTimeSinceLastKeyName, base::nullopt},
      {UserActivity::kTimeSinceLastMouseName, 200}};

  UpdateOpenTabsURLs();
  LogActivity(user_activity_event_);
  LogActivity(user_activity_event2);

  EXPECT_EQ(2,
            ukm_entry_checker_.NumNewEntriesRecorded(UserActivity::kEntryName));
  EXPECT_EQ(
      0, ukm_entry_checker_.NumNewEntriesRecorded(UserActivityId::kEntryName));

  ukm_entry_checker_.ExpectNewEntries(
      UserActivity::kEntryName, {user_activity_values_, user_activity_values2});
}

}  // namespace ml
}  // namespace power
}  // namespace chromeos
