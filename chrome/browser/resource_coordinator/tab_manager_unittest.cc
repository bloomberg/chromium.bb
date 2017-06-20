// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/resource_coordinator/tab_manager.h"

#include <algorithm>
#include <map>
#include <vector>

#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/field_trial.h"
#include "base/strings/string16.h"
#include "base/test/mock_entropy_provider.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/resource_coordinator/tab_manager_web_contents_data.h"
#include "chrome/browser/resource_coordinator/tab_stats.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/test_tab_strip_model_delegate.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/variations/variations_associated_data.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using content::WebContents;
using content::WebContentsTester;

namespace resource_coordinator {
namespace {

class TabStripDummyDelegate : public TestTabStripModelDelegate {
 public:
  TabStripDummyDelegate() {}

  bool RunUnloadListenerBeforeClosing(WebContents* contents) override {
    return false;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TabStripDummyDelegate);
};

class MockTabStripModelObserver : public TabStripModelObserver {
 public:
  MockTabStripModelObserver()
      : nb_events_(0), old_contents_(nullptr), new_contents_(nullptr) {}

  int NbEvents() const { return nb_events_; }
  WebContents* OldContents() const { return old_contents_; }
  WebContents* NewContents() const { return new_contents_; }

  void Reset() {
    nb_events_ = 0;
    old_contents_ = nullptr;
    new_contents_ = nullptr;
  }

  // TabStripModelObserver implementation:
  void TabReplacedAt(TabStripModel* tab_strip_model,
                     WebContents* old_contents,
                     WebContents* new_contents,
                     int index) override {
    nb_events_++;
    old_contents_ = old_contents;
    new_contents_ = new_contents;
  }

 private:
  int nb_events_;
  WebContents* old_contents_;
  WebContents* new_contents_;

  DISALLOW_COPY_AND_ASSIGN(MockTabStripModelObserver);
};

enum TestIndicies {
  kSelected,
  kAutoDiscardable,
  kPinned,
  kApp,
  kPlayingAudio,
  kFormEntry,
  kRecent,
  kOld,
  kReallyOld,
  kOldButPinned,
  kInternalPage,
};

}  // namespace

class TabManagerTest : public ChromeRenderViewHostTestHarness {
 public:
  WebContents* CreateWebContents() {
    return WebContents::Create(WebContents::CreateParams(profile()));
  }
};

// TODO(georgesak): Add tests for protection to tabs with form input and
// playing audio;

// Tests the sorting comparator to make sure it's producing the desired order.
TEST_F(TabManagerTest, Comparator) {
  TabStatsList test_list;
  const base::TimeTicks now = base::TimeTicks::Now();

  // Add kSelected last to verify that the array is being sorted.

  {
    TabStats stats;
    stats.last_active = now;
    stats.is_pinned = true;
    stats.child_process_host_id = kPinned;
    test_list.push_back(stats);
  }

  {
    TabStats stats;
    stats.last_active = now;
    stats.is_app = true;
    stats.child_process_host_id = kApp;
    test_list.push_back(stats);
  }

  {
    TabStats stats;
    stats.last_active = now;
    stats.is_media = true;
    stats.child_process_host_id = kPlayingAudio;
    test_list.push_back(stats);
  }

  {
    TabStats stats;
    stats.last_active = now;
    stats.has_form_entry = true;
    stats.child_process_host_id = kFormEntry;
    test_list.push_back(stats);
  }

  {
    TabStats stats;
    stats.last_active = now - base::TimeDelta::FromSeconds(10);
    stats.child_process_host_id = kRecent;
    test_list.push_back(stats);
  }

  {
    TabStats stats;
    stats.last_active = now - base::TimeDelta::FromMinutes(15);
    stats.child_process_host_id = kOld;
    test_list.push_back(stats);
  }

  {
    TabStats stats;
    stats.last_active = now - base::TimeDelta::FromDays(365);
    stats.child_process_host_id = kReallyOld;
    test_list.push_back(stats);
  }

  {
    TabStats stats;
    stats.is_pinned = true;
    stats.last_active = now - base::TimeDelta::FromDays(365);
    stats.child_process_host_id = kOldButPinned;
    test_list.push_back(stats);
  }

  {
    TabStats stats;
    stats.last_active = now;
    stats.is_internal_page = true;
    stats.child_process_host_id = kInternalPage;
    test_list.push_back(stats);
  }

  {
    TabStats stats;
    stats.last_active = now;
    stats.is_auto_discardable = false;
    stats.child_process_host_id = kAutoDiscardable;
    test_list.push_back(stats);
  }

  // This entry sorts to the front, so by adding it last, it verifies that the
  // array is being sorted.
  {
    TabStats stats;
    stats.last_active = now;
    stats.is_selected = true;
    stats.child_process_host_id = kSelected;
    test_list.push_back(stats);
  }

  std::sort(test_list.begin(), test_list.end(), TabManager::CompareTabStats);

  int index = 0;
  EXPECT_EQ(kSelected, test_list[index++].child_process_host_id);
  EXPECT_EQ(kAutoDiscardable, test_list[index++].child_process_host_id);
  EXPECT_EQ(kFormEntry, test_list[index++].child_process_host_id);
  EXPECT_EQ(kPlayingAudio, test_list[index++].child_process_host_id);
  EXPECT_EQ(kPinned, test_list[index++].child_process_host_id);
  EXPECT_EQ(kOldButPinned, test_list[index++].child_process_host_id);
  EXPECT_EQ(kApp, test_list[index++].child_process_host_id);
  EXPECT_EQ(kRecent, test_list[index++].child_process_host_id);
  EXPECT_EQ(kOld, test_list[index++].child_process_host_id);
  EXPECT_EQ(kReallyOld, test_list[index++].child_process_host_id);
  EXPECT_EQ(kInternalPage, test_list[index++].child_process_host_id);
}

TEST_F(TabManagerTest, IsInternalPage) {
  EXPECT_TRUE(TabManager::IsInternalPage(GURL(chrome::kChromeUIDownloadsURL)));
  EXPECT_TRUE(TabManager::IsInternalPage(GURL(chrome::kChromeUIHistoryURL)));
  EXPECT_TRUE(TabManager::IsInternalPage(GURL(chrome::kChromeUINewTabURL)));
  EXPECT_TRUE(TabManager::IsInternalPage(GURL(chrome::kChromeUISettingsURL)));

// Debugging URLs are not included.
#if defined(OS_CHROMEOS)
  EXPECT_FALSE(TabManager::IsInternalPage(GURL(chrome::kChromeUIDiscardsURL)));
#endif
  EXPECT_FALSE(
      TabManager::IsInternalPage(GURL(chrome::kChromeUINetInternalsURL)));

  // Prefix matches are included.
  EXPECT_TRUE(
      TabManager::IsInternalPage(GURL("chrome://settings/fakeSetting")));
}

// Ensures discarding tabs leaves TabStripModel in a good state.
TEST_F(TabManagerTest, DiscardWebContentsAt) {
  TabManager tab_manager;

  TabStripDummyDelegate delegate;
  TabStripModel tabstrip(&delegate, profile());
  tabstrip.AddObserver(&tab_manager);

  // Fill it with some tabs.
  WebContents* contents1 = CreateWebContents();
  tabstrip.AppendWebContents(contents1, true);
  WebContents* contents2 = CreateWebContents();
  tabstrip.AppendWebContents(contents2, true);

  // Start watching for events after the appends to avoid observing state
  // transitions that aren't relevant to this test.
  MockTabStripModelObserver tabstrip_observer;
  tabstrip.AddObserver(&tabstrip_observer);

  // Discard one of the tabs.
  WebContents* null_contents1 = tab_manager.DiscardWebContentsAt(0, &tabstrip);
  ASSERT_EQ(2, tabstrip.count());
  EXPECT_TRUE(tab_manager.IsTabDiscarded(tabstrip.GetWebContentsAt(0)));
  EXPECT_FALSE(tab_manager.IsTabDiscarded(tabstrip.GetWebContentsAt(1)));
  ASSERT_EQ(null_contents1, tabstrip.GetWebContentsAt(0));
  ASSERT_EQ(contents2, tabstrip.GetWebContentsAt(1));
  ASSERT_EQ(1, tabstrip_observer.NbEvents());
  EXPECT_EQ(contents1, tabstrip_observer.OldContents());
  EXPECT_EQ(null_contents1, tabstrip_observer.NewContents());
  tabstrip_observer.Reset();

  // Discard the same tab again, after resetting its discard state.
  tab_manager.GetWebContentsData(tabstrip.GetWebContentsAt(0))
      ->SetDiscardState(false);
  WebContents* null_contents2 = tab_manager.DiscardWebContentsAt(0, &tabstrip);
  ASSERT_EQ(2, tabstrip.count());
  EXPECT_TRUE(tab_manager.IsTabDiscarded(tabstrip.GetWebContentsAt(0)));
  EXPECT_FALSE(tab_manager.IsTabDiscarded(tabstrip.GetWebContentsAt(1)));
  ASSERT_EQ(null_contents2, tabstrip.GetWebContentsAt(0));
  ASSERT_EQ(contents2, tabstrip.GetWebContentsAt(1));
  ASSERT_EQ(1, tabstrip_observer.NbEvents());
  EXPECT_EQ(null_contents1, tabstrip_observer.OldContents());
  EXPECT_EQ(null_contents2, tabstrip_observer.NewContents());

  // Activating the tab should clear its discard state.
  tabstrip.ActivateTabAt(0, true /* user_gesture */);
  ASSERT_EQ(2, tabstrip.count());
  EXPECT_FALSE(tab_manager.IsTabDiscarded(tabstrip.GetWebContentsAt(0)));
  EXPECT_FALSE(tab_manager.IsTabDiscarded(tabstrip.GetWebContentsAt(1)));

  // Don't discard active tab.
  tab_manager.DiscardWebContentsAt(0, &tabstrip);
  ASSERT_EQ(2, tabstrip.count());
  EXPECT_FALSE(tab_manager.IsTabDiscarded(tabstrip.GetWebContentsAt(0)));
  EXPECT_FALSE(tab_manager.IsTabDiscarded(tabstrip.GetWebContentsAt(1)));

  tabstrip.CloseAllTabs();
  EXPECT_TRUE(tabstrip.empty());
}

// Makes sure that reloading a discarded tab without activating it unmarks the
// tab as discarded so it won't reload on activation.
TEST_F(TabManagerTest, ReloadDiscardedTabContextMenu) {
  // Note that we do not add |tab_manager| as an observer to |tabstrip| here as
  // the event we are trying to test for is not related to the tab strip, but
  // the web content instead and therefore should be handled by WebContentsData
  // (which observes the web content).
  TabManager tab_manager;
  TabStripDummyDelegate delegate;
  TabStripModel tabstrip(&delegate, profile());

  // Create 2 tabs because the active tab cannot be discarded.
  tabstrip.AppendWebContents(CreateWebContents(), true);
  content::WebContents* test_contents =
      WebContentsTester::CreateTestWebContents(browser_context(), nullptr);
  tabstrip.AppendWebContents(test_contents, false);  // Opened in background.

  // Navigate to a web page. This is necessary to set a current entry in memory
  // so the reload can happen.
  WebContentsTester::For(test_contents)
      ->NavigateAndCommit(GURL("chrome://newtab"));
  EXPECT_FALSE(tab_manager.IsTabDiscarded(tabstrip.GetWebContentsAt(1)));

  tab_manager.DiscardWebContentsAt(1, &tabstrip);
  EXPECT_TRUE(tab_manager.IsTabDiscarded(tabstrip.GetWebContentsAt(1)));

  tabstrip.GetWebContentsAt(1)->GetController().Reload(
      content::ReloadType::NORMAL, false);
  EXPECT_FALSE(tab_manager.IsTabDiscarded(tabstrip.GetWebContentsAt(1)));
  tabstrip.CloseAllTabs();
  EXPECT_TRUE(tabstrip.empty());
}

// Makes sure that the last active time property is saved even though the tab is
// discarded.
TEST_F(TabManagerTest, DiscardedTabKeepsLastActiveTime) {
  TabManager tab_manager;
  TabStripDummyDelegate delegate;
  TabStripModel tabstrip(&delegate, profile());
  tabstrip.AddObserver(&tab_manager);

  tabstrip.AppendWebContents(CreateWebContents(), true);
  WebContents* test_contents = CreateWebContents();
  tabstrip.AppendWebContents(test_contents, false);

  // Simulate an old inactive tab about to get discarded.
  base::TimeTicks new_last_active_time =
      base::TimeTicks::Now() - base::TimeDelta::FromMinutes(35);
  test_contents->SetLastActiveTime(new_last_active_time);
  EXPECT_EQ(new_last_active_time, test_contents->GetLastActiveTime());

  WebContents* null_contents = tab_manager.DiscardWebContentsAt(1, &tabstrip);
  EXPECT_EQ(new_last_active_time, null_contents->GetLastActiveTime());

  tabstrip.CloseAllTabs();
  EXPECT_TRUE(tabstrip.empty());
}

// Test to see if a tab can only be discarded once. On Windows and Mac, this
// defaults to true unless overridden through a variation parameter. On other
// platforms, it's always false.
#if defined(OS_WIN) || defined(OS_MACOSX)
TEST_F(TabManagerTest, CanOnlyDiscardOnce) {
  TabManager tab_manager;
  const std::string kTrialName = features::kAutomaticTabDiscarding.name;

  // Not setting the variation parameter.
  {
    bool discard_once_value = tab_manager.CanOnlyDiscardOnce();
    EXPECT_TRUE(discard_once_value);
  }

  // Setting the variation parameter to true.
  {
    std::unique_ptr<base::FieldTrialList> field_trial_list_;
    field_trial_list_.reset(new base::FieldTrialList(
        base::MakeUnique<base::MockEntropyProvider>()));
    variations::testing::ClearAllVariationParams();

    std::map<std::string, std::string> params;
    params["AllowMultipleDiscards"] = "true";
    ASSERT_TRUE(variations::AssociateVariationParams(kTrialName, "A", params));
    base::FieldTrialList::CreateFieldTrial(kTrialName, "A");

    bool discard_once_value = tab_manager.CanOnlyDiscardOnce();
    EXPECT_FALSE(discard_once_value);
  }

  // Setting the variation parameter to something else.
  {
    std::unique_ptr<base::FieldTrialList> field_trial_list_;
    field_trial_list_.reset(new base::FieldTrialList(
        base::MakeUnique<base::MockEntropyProvider>()));
    variations::testing::ClearAllVariationParams();

    std::map<std::string, std::string> params;
    params["AllowMultipleDiscards"] = "somethingElse";
    ASSERT_TRUE(variations::AssociateVariationParams(kTrialName, "B", params));
    base::FieldTrialList::CreateFieldTrial(kTrialName, "B");

    bool discard_once_value = tab_manager.CanOnlyDiscardOnce();
    EXPECT_TRUE(discard_once_value);
  }
}
#else
TEST_F(TabManagerTest, CanOnlyDiscardOnce) {
  TabManager tab_manager;

  bool discard_once_value = tab_manager.CanOnlyDiscardOnce();
  EXPECT_FALSE(discard_once_value);
}
#endif  // defined(OS_WIN) || defined(OS_MACOSX)

TEST_F(TabManagerTest, DefaultTimeToPurgeInCorrectRange) {
  TabManager tab_manager;
  base::TimeDelta time_to_purge =
      tab_manager.GetTimeToPurge(TabManager::kDefaultMinTimeToPurge);
  EXPECT_GE(time_to_purge, base::TimeDelta::FromMinutes(30));
  EXPECT_LT(time_to_purge, base::TimeDelta::FromMinutes(60));
}

TEST_F(TabManagerTest, ShouldPurgeAtDefaultTime) {
  TabManager tab_manager;
  TabStripDummyDelegate delegate;
  TabStripModel tabstrip(&delegate, profile());
  tabstrip.AddObserver(&tab_manager);

  WebContents* test_contents = CreateWebContents();
  tabstrip.AppendWebContents(test_contents, false);

  base::SimpleTestTickClock test_clock;
  tab_manager.set_test_tick_clock(&test_clock);

  tab_manager.GetWebContentsData(test_contents)->set_is_purged(false);
  tab_manager.GetWebContentsData(test_contents)
      ->SetLastInactiveTime(test_clock.NowTicks());
  tab_manager.GetWebContentsData(test_contents)
      ->set_time_to_purge(base::TimeDelta::FromMinutes(1));

  // Wait 1 minute and verify that the tab is still not to be purged.
  test_clock.Advance(base::TimeDelta::FromMinutes(1));
  EXPECT_FALSE(tab_manager.ShouldPurgeNow(test_contents));

  // Wait another 1 second and verify that it should be purged now .
  test_clock.Advance(base::TimeDelta::FromSeconds(1));
  EXPECT_TRUE(tab_manager.ShouldPurgeNow(test_contents));

  tab_manager.GetWebContentsData(test_contents)->set_is_purged(true);
  tab_manager.GetWebContentsData(test_contents)
      ->SetLastInactiveTime(test_clock.NowTicks());

  // Wait 1 day and verify that the tab is still be purged.
  test_clock.Advance(base::TimeDelta::FromHours(24));
  EXPECT_FALSE(tab_manager.ShouldPurgeNow(test_contents));
}

TEST_F(TabManagerTest, ActivateTabResetPurgeState) {
  TabManager tab_manager;
  TabStripDummyDelegate delegate;
  TabStripModel tabstrip(&delegate, profile());
  tabstrip.AddObserver(&tab_manager);

  TabManager::BrowserInfo browser_info;
  browser_info.tab_strip_model = &tabstrip;
  browser_info.window_is_active = true;
  browser_info.window_is_minimized = false;
  browser_info.window_bounds = gfx::Rect(0, 0, 0, 0);
  browser_info.browser_is_app = false;
  tab_manager.test_browser_info_list_.push_back(browser_info);

  base::SimpleTestTickClock test_clock;
  tab_manager.set_test_tick_clock(&test_clock);

  WebContents* tab1 = CreateWebContents();
  WebContents* tab2 = CreateWebContents();
  tabstrip.AppendWebContents(tab1, true);
  tabstrip.AppendWebContents(tab2, false);

  tab_manager.GetWebContentsData(tab2)->SetLastInactiveTime(
      test_clock.NowTicks());
  static_cast<content::MockRenderProcessHost*>(tab2->GetRenderProcessHost())
      ->set_is_process_backgrounded(true);
  EXPECT_TRUE(tab2->GetRenderProcessHost()->IsProcessBackgrounded());

  // Initially PurgeAndSuspend state should be NOT_PURGED.
  EXPECT_FALSE(tab_manager.GetWebContentsData(tab2)->is_purged());
  tab_manager.GetWebContentsData(tab2)->set_time_to_purge(
      base::TimeDelta::FromMinutes(1));
  test_clock.Advance(base::TimeDelta::FromMinutes(2));
  tab_manager.PurgeBackgroundedTabsIfNeeded();
  // Since tab2 is kept inactive and background for more than time-to-purge,
  // tab2 should be purged.
  EXPECT_TRUE(tab_manager.GetWebContentsData(tab2)->is_purged());

  // Activate tab2. Tab2's PurgeAndSuspend state should be NOT_PURGED.
  tabstrip.ActivateTabAt(1, true /* user_gesture */);
  EXPECT_FALSE(tab_manager.GetWebContentsData(tab2)->is_purged());
}

// Verify that the |is_in_visible_window| field of TabStats returned by
// GetUnsortedTabStats() is set correctly.
TEST_F(TabManagerTest, GetUnsortedTabStatsIsInVisibleWindow) {
  TabManager tab_manager;
  TabStripDummyDelegate delegate;

  WebContents* web_contents1a = CreateWebContents();
  WebContents* web_contents1b = CreateWebContents();
  WebContents* web_contents2a = CreateWebContents();
  WebContents* web_contents2b = CreateWebContents();
  WebContents* web_contents3a = CreateWebContents();
  WebContents* web_contents3b = CreateWebContents();

  // Create 3 TabStripModels.
  TabStripModel tab_strip1(&delegate, profile());
  tab_strip1.AppendWebContents(web_contents1a, true);
  tab_strip1.AppendWebContents(web_contents1b, false);

  TabStripModel tab_strip2(&delegate, profile());
  tab_strip2.AppendWebContents(web_contents2a, true);
  tab_strip2.AppendWebContents(web_contents2b, false);

  TabStripModel tab_strip3(&delegate, profile());
  tab_strip3.AppendWebContents(web_contents3a, true);
  tab_strip3.AppendWebContents(web_contents3b, false);

  // Add the 3 TabStripModels to the TabManager.
  // The window for |tab_strip1| covers the window for |tab_strip2|. The window
  // for |tab_strip3| is minimized.
  TabManager::BrowserInfo browser_info1;
  browser_info1.tab_strip_model = &tab_strip1;
  browser_info1.window_is_active = true;
  browser_info1.window_is_minimized = false;
  browser_info1.window_bounds = gfx::Rect(0, 0, 100, 100);
  browser_info1.browser_is_app = false;
  tab_manager.test_browser_info_list_.push_back(browser_info1);

  TabManager::BrowserInfo browser_info2;
  browser_info2.tab_strip_model = &tab_strip2;
  browser_info2.window_is_active = false;
  browser_info2.window_is_minimized = false;
  browser_info2.window_bounds = gfx::Rect(10, 10, 50, 50);
  browser_info2.browser_is_app = false;
  tab_manager.test_browser_info_list_.push_back(browser_info2);

  TabManager::BrowserInfo browser_info3;
  browser_info3.tab_strip_model = &tab_strip3;
  browser_info3.window_is_active = false;
  browser_info3.window_is_minimized = true;
  browser_info3.window_bounds = gfx::Rect();
  browser_info3.browser_is_app = false;
  tab_manager.test_browser_info_list_.push_back(browser_info3);

  // Get TabStats and verify the the |is_in_visible_window| field of each
  // TabStats is set correctly.
  auto tab_stats = tab_manager.GetUnsortedTabStats();

  ASSERT_EQ(6U, tab_stats.size());

  EXPECT_EQ(tab_stats[0].tab_contents_id,
            tab_manager.IdFromWebContents(web_contents1a));
  EXPECT_EQ(tab_stats[1].tab_contents_id,
            tab_manager.IdFromWebContents(web_contents1b));
  EXPECT_EQ(tab_stats[2].tab_contents_id,
            tab_manager.IdFromWebContents(web_contents2a));
  EXPECT_EQ(tab_stats[3].tab_contents_id,
            tab_manager.IdFromWebContents(web_contents2b));
  EXPECT_EQ(tab_stats[4].tab_contents_id,
            tab_manager.IdFromWebContents(web_contents3a));
  EXPECT_EQ(tab_stats[5].tab_contents_id,
            tab_manager.IdFromWebContents(web_contents3b));

  EXPECT_TRUE(tab_stats[0].is_in_visible_window);
  EXPECT_TRUE(tab_stats[1].is_in_visible_window);
  EXPECT_FALSE(tab_stats[2].is_in_visible_window);
  EXPECT_FALSE(tab_stats[3].is_in_visible_window);
  EXPECT_FALSE(tab_stats[4].is_in_visible_window);
  EXPECT_FALSE(tab_stats[5].is_in_visible_window);
}

}  // namespace resource_coordinator
