// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/power/ml/user_activity_logger_delegate_ukm.h"

#include <memory>
#include <vector>

#include "chrome/browser/chromeos/power/ml/user_activity_event.pb.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_ukm_test_helper.h"
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

class UserActivityLoggerDelegateUkmTest : public TabActivityTestBase {
 public:
  UserActivityLoggerDelegateUkmTest() {
    // These values are arbitrary but must correspond with the values
    // in |user_activity_values_|.
    UserActivityEvent::Event* event = user_activity_event_.mutable_event();
    event->set_type(UserActivityEvent::Event::REACTIVATE);
    event->set_reason(UserActivityEvent::Event::USER_ACTIVITY);
    event->set_log_duration_sec(395);
    UserActivityEvent::Features* features =
        user_activity_event_.mutable_features();
    features->set_on_to_dim_sec(100);
    features->set_dim_to_screen_off_sec(200);
    features->set_last_activity_time_sec(7300);
    features->set_last_user_activity_time_sec(3800);
    features->set_last_activity_day(UserActivityEvent::Features::MON);
    features->set_time_since_last_mouse_sec(100);
    features->set_recent_time_active_sec(10);
    features->set_device_type(UserActivityEvent::Features::CHROMEBOOK);
    features->set_device_mode(UserActivityEvent::Features::CLAMSHELL);
    features->set_battery_percent(96.0);
    features->set_device_management(UserActivityEvent::Features::UNMANAGED);
  }

  void UpdateOpenTabsURLs() {
    user_activity_logger_delegate_ukm_.UpdateOpenTabsURLs();
  }

  void LogActivity() {
    user_activity_logger_delegate_ukm_.LogActivity(user_activity_event_);
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
  void CreateTestWebContents(TabStripModel* const tab_strip_model,
                             const GURL& url,
                             bool is_active) {
    DCHECK(tab_strip_model);
    DCHECK(!url.is_empty());
    content::WebContents* contents =
        AddWebContentsAndNavigate(tab_strip_model, url);
    if (is_active) {
      tab_strip_model->ActivateTabAt(tab_strip_model->count() - 1, false);
    }
    WebContentsTester::For(contents)->TestSetIsLoading(false);
  }

 protected:
  UkmEntryChecker ukm_entry_checker_;
  const GURL url1_ = GURL("https://example1.com/");
  const GURL url2_ = GURL("https://example2.com/");
  const GURL url3_ = GURL("https://example3.com/");
  const GURL url4_ = GURL("https://example4.com/");

  const UkmMetricMap user_activity_values_ = {
      {UserActivity::kEventTypeName, 1},    // REACTIVATE
      {UserActivity::kEventReasonName, 1},  // USER_ACTIVITY
      {UserActivity::kEventLogDurationName, 380},
      {UserActivity::kScreenDimDelayName, 100},
      {UserActivity::kScreenDimToOffDelayName, 200},
      {UserActivity::kLastActivityTimeName, 2},
      {UserActivity::kLastUserActivityTimeName, 1},
      {UserActivity::kLastActivityDayName, 1},  // MON
      {UserActivity::kTimeSinceLastMouseName, 100},
      {UserActivity::kRecentTimeActiveName, 10},
      {UserActivity::kDeviceTypeName, 1},  // CHROMEBOOK
      {UserActivity::kDeviceModeName, 2},  // CLAMSHELL
      {UserActivity::kBatteryPercentName, 95},
      {UserActivity::kDeviceManagementName, 2}  // UNMANAGED
  };

 private:
  UserActivityEvent user_activity_event_;
  UserActivityLoggerDelegateUkm user_activity_logger_delegate_ukm_;
  DISALLOW_COPY_AND_ASSIGN(UserActivityLoggerDelegateUkmTest);
};

TEST_F(UserActivityLoggerDelegateUkmTest, BucketEveryFivePercents) {
  const std::vector<int> original_values = {0, 14, 15, 100};
  const std::vector<int> buckets = {0, 10, 15, 100};
  for (size_t i = 0; i < original_values.size(); ++i) {
    EXPECT_EQ(buckets[i],
              UserActivityLoggerDelegateUkm::BucketEveryFivePercents(
                  original_values[i]));
  }
}

TEST_F(UserActivityLoggerDelegateUkmTest, ExponentiallyBucketTimestamp) {
  const std::vector<int> original_values = {0,   18,  59,  60,  62,  69,  72,
                                            299, 300, 306, 316, 599, 600, 602};
  const std::vector<int> buckets = {0,   18,  59,  60,  60,  60,  70,
                                    290, 300, 300, 300, 580, 600, 600};
  for (size_t i = 0; i < original_values.size(); ++i) {
    EXPECT_EQ(buckets[i],
              UserActivityLoggerDelegateUkm::ExponentiallyBucketTimestamp(
                  original_values[i]));
  }
}

TEST_F(UserActivityLoggerDelegateUkmTest, Basic) {
  std::unique_ptr<Browser> browser =
      CreateTestBrowser(true /* is_visible */, true /* is_focused */);
  BrowserList::GetInstance()->SetLastActive(browser.get());
  TabStripModel* tab_strip_model = browser->tab_strip_model();

  CreateTestWebContents(tab_strip_model, url1_, true /* is_active */);
  const ukm::SourceId source_id1 = ukm_entry_checker_.GetSourceIdForUrl(url1_);

  CreateTestWebContents(tab_strip_model, url2_, false /* is_active */);
  const ukm::SourceId source_id2 = ukm_entry_checker_.GetSourceIdForUrl(url2_);

  UpdateOpenTabsURLs();
  LogActivity();

  EXPECT_EQ(1,
            ukm_entry_checker_.NumNewEntriesRecorded(UserActivity::kEntryName));
  EXPECT_EQ(
      2, ukm_entry_checker_.NumNewEntriesRecorded(UserActivityId::kEntryName));

  ukm_entry_checker_.ExpectNewEntry(UserActivity::kEntryName, GURL(""),
                                    user_activity_values_);

  const ukm::mojom::UkmEntry* last_activity_entry =
      ukm_entry_checker_.LastUkmEntry(UserActivity::kEntryName);

  // Explicitly check kTimeSinceLastKeyName is not recorded.
  ASSERT_FALSE(ukm::TestUkmRecorder::FindMetric(
      last_activity_entry, UserActivity::kTimeSinceLastKeyName));

  const ukm::SourceId kSourceId = last_activity_entry->source_id;

  const UkmMetricMap kUserActivityIdValues1(
      {{UserActivityId::kActivityIdName, kSourceId},
       {UserActivityId::kIsActiveName, 1},
       {UserActivityId::kIsBrowserFocusedName, 1},
       {UserActivityId::kIsBrowserVisibleName, 1},
       {UserActivityId::kIsTopmostBrowserName, 1}});

  const UkmMetricMap kUserActivityIdValues2(
      {{UserActivityId::kActivityIdName, kSourceId},
       {UserActivityId::kIsActiveName, 0},
       {UserActivityId::kIsBrowserFocusedName, 1},
       {UserActivityId::kIsBrowserVisibleName, 1},
       {UserActivityId::kIsTopmostBrowserName, 1}});

  const std::map<ukm::SourceId, std::pair<GURL, UkmMetricMap>> expected_data({
      {source_id1, std::make_pair(url1_, kUserActivityIdValues1)},
      {source_id2, std::make_pair(url2_, kUserActivityIdValues2)},
  });

  ukm_entry_checker_.ExpectNewEntries(UserActivityId::kEntryName,
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
  LogActivity();

  EXPECT_EQ(1,
            ukm_entry_checker_.NumNewEntriesRecorded(UserActivity::kEntryName));
  EXPECT_EQ(
      4, ukm_entry_checker_.NumNewEntriesRecorded(UserActivityId::kEntryName));

  ukm_entry_checker_.ExpectNewEntry(UserActivity::kEntryName, GURL(""),
                                    user_activity_values_);

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

  ukm_entry_checker_.ExpectNewEntries(UserActivityId::kEntryName,
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
  LogActivity();

  EXPECT_EQ(1,
            ukm_entry_checker_.NumNewEntriesRecorded(UserActivity::kEntryName));
  EXPECT_EQ(
      0, ukm_entry_checker_.NumNewEntriesRecorded(UserActivityId::kEntryName));

  ukm_entry_checker_.ExpectNewEntry(UserActivity::kEntryName, GURL(""),
                                    user_activity_values_);

  tab_strip_model->CloseAllTabs();
}

TEST_F(UserActivityLoggerDelegateUkmTest, NoOpenTabs) {
  std::unique_ptr<Browser> browser =
      CreateTestBrowser(true /* is_visible */, true /* is_focused */);

  UpdateOpenTabsURLs();
  LogActivity();

  EXPECT_EQ(1,
            ukm_entry_checker_.NumNewEntriesRecorded(UserActivity::kEntryName));
  EXPECT_EQ(
      0, ukm_entry_checker_.NumNewEntriesRecorded(UserActivityId::kEntryName));

  ukm_entry_checker_.ExpectNewEntry(UserActivity::kEntryName, GURL(""),
                                    user_activity_values_);
}

}  // namespace ml
}  // namespace power
}  // namespace chromeos
