// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/resource_coordinator/tab_manager.h"

#include <algorithm>
#include <map>
#include <memory>
#include <vector>

#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/field_trial.h"
#include "base/strings/string16.h"
#include "base/test/mock_entropy_provider.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/resource_coordinator/background_tab_navigation_throttle.h"
#include "chrome/browser/resource_coordinator/tab_manager_stats_collector.h"
#include "chrome/browser/resource_coordinator/tab_manager_web_contents_data.h"
#include "chrome/browser/resource_coordinator/tab_stats.h"
#include "chrome/browser/sessions/tab_loader.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/test_tab_strip_model_delegate.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/variations/variations_associated_data.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/web_contents_tester.h"
#include "content/test/test_web_contents.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using content::NavigationHandle;
using content::NavigationThrottle;
using content::WebContents;
using content::WebContentsTester;

namespace resource_coordinator {
namespace {

const char kTestUrl[] = "http://www.example.com";

class NonResumingBackgroundTabNavigationThrottle
    : public BackgroundTabNavigationThrottle {
 public:
  explicit NonResumingBackgroundTabNavigationThrottle(
      content::NavigationHandle* handle)
      : BackgroundTabNavigationThrottle(handle) {}

  void ResumeNavigation() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(NonResumingBackgroundTabNavigationThrottle);
};

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
    content::TestWebContents* web_contents =
        content::TestWebContents::Create(profile(), nullptr);
    // Commit an URL to allow discarding.
    web_contents->NavigateAndCommit(GURL("https://www.example.com"));
    return web_contents;
  }

  void PrepareTabs(const char* url1 = kTestUrl,
                   const char* url2 = kTestUrl,
                   const char* url3 = kTestUrl) {
    nav_handle1_ = CreateTabAndNavigation(url1);
    nav_handle2_ = CreateTabAndNavigation(url2);
    nav_handle3_ = CreateTabAndNavigation(url3);
    contents1_ = nav_handle1_->GetWebContents();
    contents2_ = nav_handle2_->GetWebContents();
    contents3_ = nav_handle3_->GetWebContents();

    contents1_->WasHidden();
    contents2_->WasHidden();
    contents3_->WasHidden();

    throttle1_ = base::MakeUnique<NonResumingBackgroundTabNavigationThrottle>(
        nav_handle1_.get());
    throttle2_ = base::MakeUnique<NonResumingBackgroundTabNavigationThrottle>(
        nav_handle2_.get());
    throttle3_ = base::MakeUnique<NonResumingBackgroundTabNavigationThrottle>(
        nav_handle3_.get());
  }

  // Simulate creating 3 tabs and their navigations.
  void MaybeThrottleNavigations(TabManager* tab_manager,
                                size_t loading_slots = 1,
                                const char* url1 = kTestUrl,
                                const char* url2 = kTestUrl,
                                const char* url3 = kTestUrl) {
    PrepareTabs(url1, url2, url3);

    NavigationThrottle::ThrottleCheckResult result1 =
        tab_manager->MaybeThrottleNavigation(throttle1_.get());
    NavigationThrottle::ThrottleCheckResult result2 =
        tab_manager->MaybeThrottleNavigation(throttle2_.get());
    NavigationThrottle::ThrottleCheckResult result3 =
        tab_manager->MaybeThrottleNavigation(throttle3_.get());

    CheckThrottleResults(result1, result2, result3, loading_slots);
  }

  virtual void CheckThrottleResults(
      NavigationThrottle::ThrottleCheckResult result1,
      NavigationThrottle::ThrottleCheckResult result2,
      NavigationThrottle::ThrottleCheckResult result3,
      size_t loading_slots) {
    // First tab starts navigation right away because there is no tab loading.
    EXPECT_EQ(content::NavigationThrottle::PROCEED, result1);
    switch (loading_slots) {
      case 1:
        EXPECT_EQ(content::NavigationThrottle::DEFER, result2);
        EXPECT_EQ(content::NavigationThrottle::DEFER, result3);
        break;
      case 2:
        EXPECT_EQ(content::NavigationThrottle::PROCEED, result2);
        EXPECT_EQ(content::NavigationThrottle::DEFER, result3);
        break;
      case 3:
        EXPECT_EQ(content::NavigationThrottle::PROCEED, result2);
        EXPECT_EQ(content::NavigationThrottle::PROCEED, result3);
        break;
      default:
        NOTREACHED();
    }
  }

 protected:
  std::unique_ptr<NavigationHandle> CreateTabAndNavigation(const char* url) {
    content::TestWebContents* web_contents =
        content::TestWebContents::Create(profile(), nullptr);
    return content::NavigationHandle::CreateNavigationHandleForTesting(
        GURL(url), web_contents->GetMainFrame());
  }

  std::unique_ptr<BackgroundTabNavigationThrottle> throttle1_;
  std::unique_ptr<BackgroundTabNavigationThrottle> throttle2_;
  std::unique_ptr<BackgroundTabNavigationThrottle> throttle3_;
  std::unique_ptr<NavigationHandle> nav_handle1_;
  std::unique_ptr<NavigationHandle> nav_handle2_;
  std::unique_ptr<NavigationHandle> nav_handle3_;
  WebContents* contents1_;
  WebContents* contents2_;
  WebContents* contents3_;
};

class TabManagerWithExperimentDisabledTest : public TabManagerTest {
 public:
  void SetUp() override {
    scoped_feature_list_.InitAndDisableFeature(
        features::kStaggeredBackgroundTabOpeningExperiment);
    ChromeRenderViewHostTestHarness::SetUp();
  }

  void CheckThrottleResults(NavigationThrottle::ThrottleCheckResult result1,
                            NavigationThrottle::ThrottleCheckResult result2,
                            NavigationThrottle::ThrottleCheckResult result3,
                            size_t loading_slots) override {
    EXPECT_EQ(content::NavigationThrottle::PROCEED, result1);
    EXPECT_EQ(content::NavigationThrottle::PROCEED, result2);
    EXPECT_EQ(content::NavigationThrottle::PROCEED, result3);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// TODO(georgesak): Add tests for protection to tabs with form input and
// playing audio;

// Tests the sorting comparator to make sure it's producing the desired order.
TEST_F(TabManagerTest, Comparator) {
  TabStatsList test_list;
  const base::TimeTicks now = base::TimeTicks::Now();

  // Add kAutoDiscardable last to verify that the array is being sorted.

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

  // This entry sorts to the front, so by adding it last, it verifies that the
  // array is being sorted.
  {
    TabStats stats;
    stats.last_active = now;
    stats.is_auto_discardable = false;
    stats.child_process_host_id = kAutoDiscardable;
    test_list.push_back(stats);
  }

  std::sort(test_list.begin(), test_list.end(), TabManager::CompareTabStats);

  int index = 0;
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

  // Create a tab strip in a visible and active window.
  TabStripDummyDelegate delegate;
  TabStripModel tabstrip(&delegate, profile());
  tabstrip.AddObserver(&tab_manager);

  BrowserInfo browser_info;
  browser_info.tab_strip_model = &tabstrip;
  browser_info.window_is_minimized = false;
  browser_info.browser_is_app = false;
  tab_manager.test_browser_info_list_.push_back(browser_info);

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
  WebContents* null_contents1 = tab_manager.DiscardWebContentsAt(
      0, &tabstrip, TabManager::kProactiveShutdown);
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
  WebContents* null_contents2 = tab_manager.DiscardWebContentsAt(
      0, &tabstrip, TabManager::kProactiveShutdown);
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

  tab_manager.DiscardWebContentsAt(1, &tabstrip,
                                   TabManager::kProactiveShutdown);
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

  WebContents* null_contents = tab_manager.DiscardWebContentsAt(
      1, &tabstrip, TabManager::kProactiveShutdown);
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
      tab_manager.GetTimeToPurge(TabManager::kDefaultMinTimeToPurge,
                                 TabManager::kDefaultMinTimeToPurge * 4);
  EXPECT_GE(time_to_purge, base::TimeDelta::FromMinutes(1));
  EXPECT_LE(time_to_purge, base::TimeDelta::FromMinutes(4));
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

  // Tabs with a committed URL must be closed explicitly to avoid DCHECK errors.
  tabstrip.CloseAllTabs();
}

TEST_F(TabManagerTest, ActivateTabResetPurgeState) {
  TabManager tab_manager;
  TabStripDummyDelegate delegate;
  TabStripModel tabstrip(&delegate, profile());
  tabstrip.AddObserver(&tab_manager);

  BrowserInfo browser_info;
  browser_info.tab_strip_model = &tabstrip;
  browser_info.window_is_minimized = false;
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
  static_cast<content::MockRenderProcessHost*>(
      tab2->GetMainFrame()->GetProcess())
      ->set_is_process_backgrounded(true);
  EXPECT_TRUE(tab2->GetMainFrame()->GetProcess()->IsProcessBackgrounded());

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

  // Tabs with a committed URL must be closed explicitly to avoid DCHECK errors.
  tabstrip.CloseAllTabs();
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

  // Create 2 TabStripModels.
  TabStripModel tab_strip1(&delegate, profile());
  tab_strip1.AppendWebContents(web_contents1a, true);
  tab_strip1.AppendWebContents(web_contents1b, false);

  TabStripModel tab_strip2(&delegate, profile());
  tab_strip2.AppendWebContents(web_contents2a, true);
  tab_strip2.AppendWebContents(web_contents2b, false);

  // Add the 2 TabStripModels to the TabManager.
  // The window for |tab_strip1| is visible while the window for |tab_strip2| is
  // minimized.
  BrowserInfo browser_info1;
  browser_info1.tab_strip_model = &tab_strip1;
  browser_info1.window_is_minimized = false;
  browser_info1.browser_is_app = false;
  tab_manager.test_browser_info_list_.push_back(browser_info1);

  BrowserInfo browser_info2;
  browser_info2.tab_strip_model = &tab_strip2;
  browser_info2.window_is_minimized = true;
  browser_info2.browser_is_app = false;
  tab_manager.test_browser_info_list_.push_back(browser_info2);

  // Get TabStats and verify the the |is_in_visible_window| field of each
  // TabStats is set correctly.
  auto tab_stats = tab_manager.GetUnsortedTabStats();

  ASSERT_EQ(4U, tab_stats.size());

  EXPECT_EQ(tab_stats[0].tab_contents_id,
            tab_manager.IdFromWebContents(web_contents1a));
  EXPECT_EQ(tab_stats[1].tab_contents_id,
            tab_manager.IdFromWebContents(web_contents1b));
  EXPECT_EQ(tab_stats[2].tab_contents_id,
            tab_manager.IdFromWebContents(web_contents2a));
  EXPECT_EQ(tab_stats[3].tab_contents_id,
            tab_manager.IdFromWebContents(web_contents2b));

  EXPECT_TRUE(tab_stats[0].is_in_visible_window);
  EXPECT_TRUE(tab_stats[1].is_in_visible_window);
  EXPECT_FALSE(tab_stats[2].is_in_visible_window);
  EXPECT_FALSE(tab_stats[3].is_in_visible_window);

  // Tabs with a committed URL must be closed explicitly to avoid DCHECK errors.
  tab_strip1.CloseAllTabs();
  tab_strip2.CloseAllTabs();
}

// Verify that:
// - On ChromeOS, DiscardTab can discard every tab in a non-visible window, but
//   cannot discard the active tab in a visible window.
// - On other platforms, DiscardTab can discard every non-active tab.
TEST_F(TabManagerTest, DiscardTabWithNonVisibleTabs) {
  TabManager tab_manager;
  TabStripDummyDelegate delegate;

  // Create 2 TabStripModels.
  TabStripModel tab_strip1(&delegate, profile());
  tab_strip1.AppendWebContents(CreateWebContents(), true);
  tab_strip1.AppendWebContents(CreateWebContents(), false);

  TabStripModel tab_strip2(&delegate, profile());
  tab_strip2.AppendWebContents(CreateWebContents(), true);
  tab_strip2.AppendWebContents(CreateWebContents(), false);

  // Add the 2 TabStripModels to the TabManager.
  // The window for |tab_strip1| is visible while the window for |tab_strip2|
  // is minimized.
  BrowserInfo browser_info1;
  browser_info1.tab_strip_model = &tab_strip1;
  browser_info1.window_is_minimized = false;
  browser_info1.browser_is_app = false;
  tab_manager.test_browser_info_list_.push_back(browser_info1);

  BrowserInfo browser_info2;
  browser_info2.tab_strip_model = &tab_strip2;
  browser_info2.window_is_minimized = true;
  browser_info2.browser_is_app = false;
  tab_manager.test_browser_info_list_.push_back(browser_info2);

  for (int i = 0; i < 4; ++i)
    tab_manager.DiscardTab(TabManager::kProactiveShutdown);

  // Active tab in a visible window should not be discarded.
  EXPECT_FALSE(tab_manager.IsTabDiscarded(tab_strip1.GetWebContentsAt(0)));

  // Non-active tabs should be discarded.
  EXPECT_TRUE(tab_manager.IsTabDiscarded(tab_strip1.GetWebContentsAt(1)));
  EXPECT_TRUE(tab_manager.IsTabDiscarded(tab_strip2.GetWebContentsAt(1)));

#if defined(OS_CHROMEOS)
  // On ChromeOS, active tab in a minimized window should be discarded.
  EXPECT_TRUE(tab_manager.IsTabDiscarded(tab_strip2.GetWebContentsAt(0)));
#else
  // On other platforms, an active tab is never discarded, even if its window is
  // minimized.
  EXPECT_FALSE(tab_manager.IsTabDiscarded(tab_strip2.GetWebContentsAt(0)));
#endif  // defined(OS_CHROMEOS)

  // Tabs with a committed URL must be closed explicitly to avoid DCHECK errors.
  tab_strip1.CloseAllTabs();
  tab_strip2.CloseAllTabs();
}

TEST_F(TabManagerTest, MaybeThrottleNavigation) {
  TabManager* tab_manager = g_browser_process->GetTabManager();
  tab_manager->ResetMemoryPressureListenerForTest();
  MaybeThrottleNavigations(tab_manager);
  tab_manager->GetWebContentsData(contents1_)
      ->DidStartNavigation(nav_handle1_.get());

  // Tab 1 is loading. The other 2 tabs's navigations are delayed.
  EXPECT_TRUE(tab_manager->IsTabLoadingForTest(contents1_));
  EXPECT_FALSE(tab_manager->IsTabLoadingForTest(contents2_));
  EXPECT_FALSE(tab_manager->IsTabLoadingForTest(contents3_));

  EXPECT_FALSE(tab_manager->IsNavigationDelayedForTest(nav_handle1_.get()));
  EXPECT_TRUE(tab_manager->IsNavigationDelayedForTest(nav_handle2_.get()));
  EXPECT_TRUE(tab_manager->IsNavigationDelayedForTest(nav_handle3_.get()));
}

TEST_F(TabManagerTest, OnDidFinishNavigation) {
  TabManager* tab_manager = g_browser_process->GetTabManager();
  tab_manager->ResetMemoryPressureListenerForTest();
  MaybeThrottleNavigations(tab_manager);
  tab_manager->GetWebContentsData(contents1_)
      ->DidStartNavigation(nav_handle1_.get());

  EXPECT_TRUE(tab_manager->IsNavigationDelayedForTest(nav_handle2_.get()));
  tab_manager->GetWebContentsData(contents2_)
      ->DidFinishNavigation(nav_handle2_.get());
  EXPECT_FALSE(tab_manager->IsNavigationDelayedForTest(nav_handle2_.get()));
}

TEST_F(TabManagerTest, OnDidStopLoading) {
  TabManager* tab_manager = g_browser_process->GetTabManager();
  tab_manager->ResetMemoryPressureListenerForTest();
  MaybeThrottleNavigations(tab_manager);
  tab_manager->GetWebContentsData(contents1_)
      ->DidStartNavigation(nav_handle1_.get());

  EXPECT_TRUE(tab_manager->IsTabLoadingForTest(contents1_));
  EXPECT_FALSE(tab_manager->IsTabLoadingForTest(contents2_));
  EXPECT_TRUE(tab_manager->IsNavigationDelayedForTest(nav_handle2_.get()));

  // Simulate tab 1 has finished loading.
  tab_manager->GetWebContentsData(contents1_)->DidStopLoading();

  // After tab 1 has finished loading, TabManager starts loading the next tab.
  EXPECT_FALSE(tab_manager->IsTabLoadingForTest(contents1_));
  EXPECT_TRUE(tab_manager->IsTabLoadingForTest(contents2_));
  EXPECT_FALSE(tab_manager->IsNavigationDelayedForTest(nav_handle2_.get()));
}

TEST_F(TabManagerTest, OnWebContentsDestroyed) {
  TabManager* tab_manager = g_browser_process->GetTabManager();
  tab_manager->ResetMemoryPressureListenerForTest();
  MaybeThrottleNavigations(tab_manager);
  tab_manager->GetWebContentsData(contents1_)
      ->DidStartNavigation(nav_handle1_.get());

  // Tab 2 is destroyed when its navigation is still delayed. Its states are
  // cleaned up.
  EXPECT_FALSE(tab_manager->IsTabLoadingForTest(contents2_));
  EXPECT_TRUE(tab_manager->IsNavigationDelayedForTest(nav_handle2_.get()));
  tab_manager->OnWebContentsDestroyed(contents2_);
  EXPECT_FALSE(tab_manager->IsTabLoadingForTest(contents2_));
  EXPECT_FALSE(tab_manager->IsNavigationDelayedForTest(nav_handle2_.get()));

  // Tab 1 is destroyed when it is still loading. Its states are cleaned up and
  // Tabmanager starts to load the next tab (tab 3).
  EXPECT_TRUE(tab_manager->IsTabLoadingForTest(contents1_));
  EXPECT_FALSE(tab_manager->IsTabLoadingForTest(contents3_));
  EXPECT_TRUE(tab_manager->IsNavigationDelayedForTest(nav_handle3_.get()));
  tab_manager->GetWebContentsData(contents1_)->WebContentsDestroyed();
  EXPECT_FALSE(tab_manager->IsTabLoadingForTest(contents1_));
  EXPECT_TRUE(tab_manager->IsTabLoadingForTest(contents3_));
  EXPECT_FALSE(tab_manager->IsNavigationDelayedForTest(nav_handle3_.get()));
}

TEST_F(TabManagerTest, OnDelayedTabSelected) {
  TabManager* tab_manager = g_browser_process->GetTabManager();
  tab_manager->ResetMemoryPressureListenerForTest();
  MaybeThrottleNavigations(tab_manager);
  tab_manager->GetWebContentsData(contents1_)
      ->DidStartNavigation(nav_handle1_.get());

  EXPECT_TRUE(tab_manager->IsTabLoadingForTest(contents1_));
  EXPECT_FALSE(tab_manager->IsTabLoadingForTest(contents3_));
  EXPECT_TRUE(tab_manager->IsNavigationDelayedForTest(nav_handle3_.get()));

  // Simulate selecting tab 3, which should start loading immediately.
  tab_manager->ActiveTabChanged(
      contents1_, contents3_, 2,
      TabStripModelObserver::CHANGE_REASON_USER_GESTURE);

  EXPECT_TRUE(tab_manager->IsTabLoadingForTest(contents1_));
  EXPECT_TRUE(tab_manager->IsTabLoadingForTest(contents3_));
  EXPECT_FALSE(tab_manager->IsNavigationDelayedForTest(nav_handle3_.get()));

  // Simulate tab 1 has finished loading. TabManager will NOT load the next tab
  // (tab 2) because tab 3 is still loading.
  EXPECT_FALSE(tab_manager->IsTabLoadingForTest(contents2_));
  EXPECT_TRUE(tab_manager->IsNavigationDelayedForTest(nav_handle2_.get()));
  tab_manager->GetWebContentsData(contents1_)->DidStopLoading();
  EXPECT_FALSE(tab_manager->IsTabLoadingForTest(contents1_));
  EXPECT_FALSE(tab_manager->IsTabLoadingForTest(contents2_));
  EXPECT_TRUE(tab_manager->IsNavigationDelayedForTest(nav_handle2_.get()));

  // Simulate tab 3 has finished loading. TabManager starts loading the next tab
  // (tab 2).
  tab_manager->GetWebContentsData(contents3_)->DidStopLoading();
  EXPECT_FALSE(tab_manager->IsTabLoadingForTest(contents3_));
  EXPECT_TRUE(tab_manager->IsTabLoadingForTest(contents2_));
  EXPECT_FALSE(tab_manager->IsNavigationDelayedForTest(nav_handle2_.get()));
}

TEST_F(TabManagerTest, TimeoutWhenLoadingBackgroundTabs) {
  TabManager* tab_manager = g_browser_process->GetTabManager();
  tab_manager->ResetMemoryPressureListenerForTest();
  base::SimpleTestTickClock test_clock;
  tab_manager->set_test_tick_clock(&test_clock);

  MaybeThrottleNavigations(tab_manager);
  tab_manager->GetWebContentsData(contents1_)
      ->DidStartNavigation(nav_handle1_.get());

  EXPECT_TRUE(tab_manager->IsTabLoadingForTest(contents1_));
  EXPECT_FALSE(tab_manager->IsTabLoadingForTest(contents2_));
  EXPECT_FALSE(tab_manager->IsTabLoadingForTest(contents3_));
  EXPECT_FALSE(tab_manager->IsNavigationDelayedForTest(nav_handle1_.get()));
  EXPECT_TRUE(tab_manager->IsNavigationDelayedForTest(nav_handle2_.get()));
  EXPECT_TRUE(tab_manager->IsNavigationDelayedForTest(nav_handle3_.get()));

  // Simulate timout when loading the 1st tab. TabManager should start loading
  // the 2nd tab.
  test_clock.Advance(base::TimeDelta::FromMinutes(1));
  EXPECT_TRUE(tab_manager->TriggerForceLoadTimerForTest());

  EXPECT_TRUE(tab_manager->IsTabLoadingForTest(contents1_));
  EXPECT_TRUE(tab_manager->IsTabLoadingForTest(contents2_));
  EXPECT_FALSE(tab_manager->IsTabLoadingForTest(contents3_));
  EXPECT_FALSE(tab_manager->IsNavigationDelayedForTest(nav_handle1_.get()));
  EXPECT_FALSE(tab_manager->IsNavigationDelayedForTest(nav_handle2_.get()));
  EXPECT_TRUE(tab_manager->IsNavigationDelayedForTest(nav_handle3_.get()));

  // Simulate timout again. TabManager should start loading the 3rd tab.
  test_clock.Advance(base::TimeDelta::FromMinutes(1));
  EXPECT_TRUE(tab_manager->TriggerForceLoadTimerForTest());

  EXPECT_TRUE(tab_manager->IsTabLoadingForTest(contents1_));
  EXPECT_TRUE(tab_manager->IsTabLoadingForTest(contents2_));
  EXPECT_TRUE(tab_manager->IsTabLoadingForTest(contents3_));
  EXPECT_FALSE(tab_manager->IsNavigationDelayedForTest(nav_handle1_.get()));
  EXPECT_FALSE(tab_manager->IsNavigationDelayedForTest(nav_handle2_.get()));
  EXPECT_FALSE(tab_manager->IsNavigationDelayedForTest(nav_handle3_.get()));
}

TEST_F(TabManagerTest, BackgroundTabLoadingMode) {
  TabManager* tab_manager = g_browser_process->GetTabManager();
  tab_manager->ResetMemoryPressureListenerForTest();
  base::SimpleTestTickClock test_clock;
  tab_manager->set_test_tick_clock(&test_clock);

  EXPECT_EQ(TabManager::BackgroundTabLoadingMode::kStaggered,
            tab_manager->background_tab_loading_mode_);

  MaybeThrottleNavigations(tab_manager);
  tab_manager->GetWebContentsData(contents1_)
      ->DidStartNavigation(nav_handle1_.get());

  EXPECT_TRUE(tab_manager->IsTabLoadingForTest(contents1_));
  EXPECT_FALSE(tab_manager->IsTabLoadingForTest(contents2_));
  EXPECT_FALSE(tab_manager->IsTabLoadingForTest(contents3_));
  EXPECT_FALSE(tab_manager->IsNavigationDelayedForTest(nav_handle1_.get()));
  EXPECT_TRUE(tab_manager->IsNavigationDelayedForTest(nav_handle2_.get()));
  EXPECT_TRUE(tab_manager->IsNavigationDelayedForTest(nav_handle3_.get()));

  // Simulate memory pressure getting high. TabManager pause loading pending
  // background tabs.
  tab_manager->OnMemoryPressure(
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE);
  EXPECT_EQ(TabManager::BackgroundTabLoadingMode::kPaused,
            tab_manager->background_tab_loading_mode_);

  // Simulate timout when loading the 1st tab.
  test_clock.Advance(base::TimeDelta::FromMinutes(1));
  EXPECT_TRUE(tab_manager->TriggerForceLoadTimerForTest());

  // Tab 2 and 3 are still pending because of the paused loading mode.
  EXPECT_FALSE(tab_manager->IsTabLoadingForTest(contents2_));
  EXPECT_FALSE(tab_manager->IsTabLoadingForTest(contents3_));
  EXPECT_TRUE(tab_manager->IsNavigationDelayedForTest(nav_handle2_.get()));
  EXPECT_TRUE(tab_manager->IsNavigationDelayedForTest(nav_handle3_.get()));

  // Simulate tab 1 has finished loading.
  tab_manager->GetWebContentsData(contents1_)->DidStopLoading();

  // Tab 2 and 3 are still pending because of the paused loading mode.
  EXPECT_FALSE(tab_manager->IsTabLoadingForTest(contents2_));
  EXPECT_FALSE(tab_manager->IsTabLoadingForTest(contents3_));
  EXPECT_TRUE(tab_manager->IsNavigationDelayedForTest(nav_handle2_.get()));
  EXPECT_TRUE(tab_manager->IsNavigationDelayedForTest(nav_handle3_.get()));

  // Simulate memory pressure is relieved. TabManager will reset the loading
  // mode and try to load the next tab.
  tab_manager->OnMemoryPressure(
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE);
  EXPECT_EQ(TabManager::BackgroundTabLoadingMode::kStaggered,
            tab_manager->background_tab_loading_mode_);

  // Tab 2 should start loading right away.
  EXPECT_TRUE(tab_manager->IsTabLoadingForTest(contents2_));
  EXPECT_FALSE(tab_manager->IsNavigationDelayedForTest(nav_handle2_.get()));

  // Simulate tab 2 has finished loading.
  tab_manager->GetWebContentsData(contents2_)->DidStopLoading();

  // Tab 3 should start loading now in staggered loading mode.
  EXPECT_TRUE(tab_manager->IsTabLoadingForTest(contents3_));
  EXPECT_FALSE(tab_manager->IsNavigationDelayedForTest(nav_handle3_.get()));
}

TEST_F(TabManagerTest, BackgroundTabLoadingSlots) {
  TabManager tab_manager1;
  MaybeThrottleNavigations(&tab_manager1, 1);
  EXPECT_FALSE(tab_manager1.IsNavigationDelayedForTest(nav_handle1_.get()));
  EXPECT_TRUE(tab_manager1.IsNavigationDelayedForTest(nav_handle2_.get()));
  EXPECT_TRUE(tab_manager1.IsNavigationDelayedForTest(nav_handle3_.get()));

  TabManager tab_manager2;
  tab_manager2.SetLoadingSlotsForTest(2);
  MaybeThrottleNavigations(&tab_manager2, 2);
  EXPECT_FALSE(tab_manager2.IsNavigationDelayedForTest(nav_handle1_.get()));
  EXPECT_FALSE(tab_manager2.IsNavigationDelayedForTest(nav_handle2_.get()));
  EXPECT_TRUE(tab_manager2.IsNavigationDelayedForTest(nav_handle3_.get()));

  TabManager tab_manager3;
  tab_manager3.SetLoadingSlotsForTest(3);
  MaybeThrottleNavigations(&tab_manager3, 3);
  EXPECT_FALSE(tab_manager3.IsNavigationDelayedForTest(nav_handle1_.get()));
  EXPECT_FALSE(tab_manager3.IsNavigationDelayedForTest(nav_handle2_.get()));
  EXPECT_FALSE(tab_manager3.IsNavigationDelayedForTest(nav_handle3_.get()));
}

TEST_F(TabManagerTest, BackgroundTabsLoadingOrdering) {
  TabManager* tab_manager = g_browser_process->GetTabManager();
  tab_manager->ResetMemoryPressureListenerForTest();
  base::SimpleTestTickClock test_clock;
  tab_manager->set_test_tick_clock(&test_clock);

  MaybeThrottleNavigations(
      tab_manager, 1, kTestUrl,
      chrome::kChromeUISettingsURL,  // Using internal page URL for tab 2.
      kTestUrl);
  tab_manager->GetWebContentsData(contents1_)
      ->DidStartNavigation(nav_handle1_.get());

  EXPECT_TRUE(tab_manager->IsTabLoadingForTest(contents1_));
  EXPECT_FALSE(tab_manager->IsTabLoadingForTest(contents2_));
  EXPECT_FALSE(tab_manager->IsTabLoadingForTest(contents3_));
  EXPECT_FALSE(tab_manager->IsNavigationDelayedForTest(nav_handle1_.get()));
  EXPECT_TRUE(tab_manager->IsNavigationDelayedForTest(nav_handle2_.get()));
  EXPECT_TRUE(tab_manager->IsNavigationDelayedForTest(nav_handle3_.get()));

  // Simulate tab 1 has finished loading. Tab 3 should be loaded before tab 2,
  // because tab 2 is internal page.
  tab_manager->GetWebContentsData(contents1_)->DidStopLoading();

  EXPECT_FALSE(tab_manager->IsTabLoadingForTest(contents2_));
  EXPECT_TRUE(tab_manager->IsTabLoadingForTest(contents3_));
  EXPECT_TRUE(tab_manager->IsNavigationDelayedForTest(nav_handle2_.get()));
  EXPECT_FALSE(tab_manager->IsNavigationDelayedForTest(nav_handle3_.get()));
}

TEST_F(TabManagerTest, PauseAndResumeBackgroundTabOpening) {
  TabManager* tab_manager = g_browser_process->GetTabManager();
  tab_manager->ResetMemoryPressureListenerForTest();
  PrepareTabs();

  EXPECT_EQ(TabManager::BackgroundTabLoadingMode::kStaggered,
            tab_manager->background_tab_loading_mode_);
  EXPECT_FALSE(tab_manager->IsInBackgroundTabOpeningSession());
  EXPECT_FALSE(
      tab_manager->stats_collector()->is_in_background_tab_opening_session());

  // Start background tab opening session.
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            tab_manager->MaybeThrottleNavigation(throttle1_.get()));
  tab_manager->GetWebContentsData(contents1_)
      ->DidStartNavigation(nav_handle1_.get());
  EXPECT_TRUE(tab_manager->IsInBackgroundTabOpeningSession());
  EXPECT_TRUE(
      tab_manager->stats_collector()->is_in_background_tab_opening_session());

  // Simulate memory pressure getting high. TabManager pause loading pending
  // background tabs, and ends the background tab opening session.
  tab_manager->OnMemoryPressure(
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE);
  EXPECT_EQ(TabManager::BackgroundTabLoadingMode::kPaused,
            tab_manager->background_tab_loading_mode_);
  EXPECT_FALSE(tab_manager->IsInBackgroundTabOpeningSession());
  EXPECT_FALSE(
      tab_manager->stats_collector()->is_in_background_tab_opening_session());

  // Simulate tab 1 has finished loading, which was scheduled to load before
  // pausing.
  tab_manager->GetWebContentsData(contents1_)->DidStopLoading();

  // TabManager cannot enter BackgroundTabOpening session when it is in paused
  // mode.
  EXPECT_EQ(content::NavigationThrottle::DEFER,
            tab_manager->MaybeThrottleNavigation(throttle2_.get()));
  EXPECT_TRUE(tab_manager->IsNavigationDelayedForTest(nav_handle2_.get()));
  EXPECT_FALSE(tab_manager->IsInBackgroundTabOpeningSession());
  EXPECT_FALSE(
      tab_manager->stats_collector()->is_in_background_tab_opening_session());

  // Simulate memory pressure is relieved. TabManager will reset the loading
  // mode and try to load the next tab. If there is pending tab, TabManager
  // starts a new BackgroundTabOpening session.
  tab_manager->OnMemoryPressure(
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE);
  EXPECT_EQ(TabManager::BackgroundTabLoadingMode::kStaggered,
            tab_manager->background_tab_loading_mode_);
  EXPECT_TRUE(tab_manager->IsInBackgroundTabOpeningSession());
  EXPECT_TRUE(
      tab_manager->stats_collector()->is_in_background_tab_opening_session());

  // Tab 2 should start loading right away.
  EXPECT_TRUE(tab_manager->IsTabLoadingForTest(contents2_));
  EXPECT_FALSE(tab_manager->IsNavigationDelayedForTest(nav_handle2_.get()));
}

TEST_F(TabManagerTest, IsInBackgroundTabOpeningSession) {
  TabManager* tab_manager = g_browser_process->GetTabManager();
  tab_manager->ResetMemoryPressureListenerForTest();
  EXPECT_FALSE(tab_manager->IsInBackgroundTabOpeningSession());
  EXPECT_FALSE(
      tab_manager->stats_collector()->is_in_background_tab_opening_session());

  MaybeThrottleNavigations(tab_manager);
  tab_manager->GetWebContentsData(contents1_)
      ->DidStartNavigation(nav_handle1_.get());
  EXPECT_TRUE(tab_manager->IsInBackgroundTabOpeningSession());
  EXPECT_TRUE(
      tab_manager->stats_collector()->is_in_background_tab_opening_session());

  tab_manager->GetWebContentsData(contents1_)->DidStopLoading();
  EXPECT_TRUE(tab_manager->IsInBackgroundTabOpeningSession());
  EXPECT_TRUE(
      tab_manager->stats_collector()->is_in_background_tab_opening_session());

  tab_manager->GetWebContentsData(contents2_)->DidStopLoading();
  EXPECT_TRUE(tab_manager->IsInBackgroundTabOpeningSession());
  EXPECT_TRUE(
      tab_manager->stats_collector()->is_in_background_tab_opening_session());

  // It is still in background tab opening session even if tab3 is brought to
  // foreground. The session only ends after tab1, tab2 and tab3 have all
  // finished loading.
  contents3_->WasShown();
  EXPECT_TRUE(tab_manager->IsInBackgroundTabOpeningSession());
  EXPECT_TRUE(
      tab_manager->stats_collector()->is_in_background_tab_opening_session());

  tab_manager->GetWebContentsData(contents3_)->DidStopLoading();
  EXPECT_FALSE(tab_manager->IsInBackgroundTabOpeningSession());
  EXPECT_FALSE(
      tab_manager->stats_collector()->is_in_background_tab_opening_session());
}

TEST_F(TabManagerWithExperimentDisabledTest, IsInBackgroundTabOpeningSession) {
  EXPECT_FALSE(base::FeatureList::IsEnabled(
      features::kStaggeredBackgroundTabOpeningExperiment));

  TabManager* tab_manager = g_browser_process->GetTabManager();
  tab_manager->ResetMemoryPressureListenerForTest();
  EXPECT_FALSE(tab_manager->IsInBackgroundTabOpeningSession());
  EXPECT_FALSE(
      tab_manager->stats_collector()->is_in_background_tab_opening_session());

  MaybeThrottleNavigations(tab_manager);
  tab_manager->GetWebContentsData(contents1_)
      ->DidStartNavigation(nav_handle1_.get());
  tab_manager->GetWebContentsData(contents2_)
      ->DidStartNavigation(nav_handle1_.get());
  tab_manager->GetWebContentsData(contents3_)
      ->DidStartNavigation(nav_handle1_.get());

  EXPECT_TRUE(tab_manager->IsTabLoadingForTest(contents1_));
  EXPECT_TRUE(tab_manager->IsTabLoadingForTest(contents2_));
  EXPECT_TRUE(tab_manager->IsTabLoadingForTest(contents3_));
  EXPECT_FALSE(tab_manager->IsNavigationDelayedForTest(nav_handle1_.get()));
  EXPECT_FALSE(tab_manager->IsNavigationDelayedForTest(nav_handle2_.get()));
  EXPECT_FALSE(tab_manager->IsNavigationDelayedForTest(nav_handle3_.get()));
  EXPECT_TRUE(tab_manager->IsInBackgroundTabOpeningSession());
  EXPECT_TRUE(
      tab_manager->stats_collector()->is_in_background_tab_opening_session());

  tab_manager->GetWebContentsData(contents1_)->DidStopLoading();
  EXPECT_FALSE(tab_manager->IsTabLoadingForTest(contents1_));
  EXPECT_TRUE(tab_manager->IsTabLoadingForTest(contents2_));
  EXPECT_TRUE(tab_manager->IsTabLoadingForTest(contents3_));
  EXPECT_TRUE(tab_manager->IsInBackgroundTabOpeningSession());
  EXPECT_TRUE(
      tab_manager->stats_collector()->is_in_background_tab_opening_session());

  tab_manager->GetWebContentsData(contents2_)->DidStopLoading();
  EXPECT_FALSE(tab_manager->IsTabLoadingForTest(contents1_));
  EXPECT_FALSE(tab_manager->IsTabLoadingForTest(contents2_));
  EXPECT_TRUE(tab_manager->IsTabLoadingForTest(contents3_));
  EXPECT_TRUE(tab_manager->IsInBackgroundTabOpeningSession());
  EXPECT_TRUE(
      tab_manager->stats_collector()->is_in_background_tab_opening_session());

  // It is still in background tab opening session even if tab3 is brought to
  // foreground. The session only ends after tab1, tab2 and tab3 have all
  // finished loading.
  contents3_->WasShown();
  EXPECT_TRUE(tab_manager->IsInBackgroundTabOpeningSession());
  EXPECT_TRUE(
      tab_manager->stats_collector()->is_in_background_tab_opening_session());

  tab_manager->GetWebContentsData(contents3_)->DidStopLoading();
  EXPECT_FALSE(tab_manager->IsTabLoadingForTest(contents1_));
  EXPECT_FALSE(tab_manager->IsTabLoadingForTest(contents2_));
  EXPECT_FALSE(tab_manager->IsTabLoadingForTest(contents3_));
  EXPECT_FALSE(tab_manager->IsInBackgroundTabOpeningSession());
  EXPECT_FALSE(
      tab_manager->stats_collector()->is_in_background_tab_opening_session());
}

TEST_F(TabManagerTest, SessionRestoreBeforeBackgroundTabOpeningSession) {
  TabManager* tab_manager = g_browser_process->GetTabManager();
  tab_manager->ResetMemoryPressureListenerForTest();
  PrepareTabs();

  // Start session restore.
  tab_manager->OnSessionRestoreStartedLoadingTabs();
  EXPECT_TRUE(tab_manager->IsSessionRestoreLoadingTabs());
  EXPECT_FALSE(tab_manager->IsInBackgroundTabOpeningSession());
  EXPECT_FALSE(
      tab_manager->stats_collector()->is_in_background_tab_opening_session());

  tab_manager->GetWebContentsData(contents1_)->SetIsInSessionRestore(true);
  tab_manager->GetWebContentsData(contents2_)->SetIsInSessionRestore(false);
  tab_manager->GetWebContentsData(contents3_)->SetIsInSessionRestore(false);

  // Do not enter BackgroundTabOpening session if the background tab is in
  // session restore.
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            tab_manager->MaybeThrottleNavigation(throttle1_.get()));
  EXPECT_FALSE(tab_manager->IsInBackgroundTabOpeningSession());
  EXPECT_FALSE(
      tab_manager->stats_collector()->is_in_background_tab_opening_session());

  // Enter BackgroundTabOpening session when there are background tabs not in
  // session restore, though the first background tab can still proceed.
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            tab_manager->MaybeThrottleNavigation(throttle2_.get()));
  EXPECT_TRUE(tab_manager->IsInBackgroundTabOpeningSession());
  EXPECT_TRUE(
      tab_manager->stats_collector()->is_in_background_tab_opening_session());

  EXPECT_EQ(content::NavigationThrottle::DEFER,
            tab_manager->MaybeThrottleNavigation(throttle3_.get()));
  EXPECT_TRUE(tab_manager->IsInBackgroundTabOpeningSession());
  EXPECT_TRUE(
      tab_manager->stats_collector()->is_in_background_tab_opening_session());

  // Stop session restore.
  tab_manager->OnSessionRestoreFinishedLoadingTabs();
  EXPECT_FALSE(tab_manager->IsSessionRestoreLoadingTabs());
  EXPECT_TRUE(tab_manager->IsInBackgroundTabOpeningSession());
  EXPECT_TRUE(
      tab_manager->stats_collector()->is_in_background_tab_opening_session());
}

TEST_F(TabManagerTest, SessionRestoreAfterBackgroundTabOpeningSession) {
  TabManager* tab_manager = g_browser_process->GetTabManager();
  tab_manager->ResetMemoryPressureListenerForTest();
  PrepareTabs();

  EXPECT_FALSE(tab_manager->IsSessionRestoreLoadingTabs());
  EXPECT_FALSE(tab_manager->IsInBackgroundTabOpeningSession());
  EXPECT_FALSE(
      tab_manager->stats_collector()->is_in_background_tab_opening_session());

  // Start background tab opening session.
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            tab_manager->MaybeThrottleNavigation(throttle1_.get()));
  EXPECT_TRUE(tab_manager->IsInBackgroundTabOpeningSession());
  EXPECT_TRUE(
      tab_manager->stats_collector()->is_in_background_tab_opening_session());

  EXPECT_EQ(content::NavigationThrottle::DEFER,
            tab_manager->MaybeThrottleNavigation(throttle2_.get()));
  EXPECT_TRUE(tab_manager->IsInBackgroundTabOpeningSession());
  EXPECT_TRUE(
      tab_manager->stats_collector()->is_in_background_tab_opening_session());

  // Now session restore starts after background tab opening session starts.
  tab_manager->OnSessionRestoreStartedLoadingTabs();
  EXPECT_TRUE(tab_manager->IsSessionRestoreLoadingTabs());
  EXPECT_TRUE(tab_manager->IsInBackgroundTabOpeningSession());
  EXPECT_TRUE(
      tab_manager->stats_collector()->is_in_background_tab_opening_session());

  // The following background tabs are still delayed if they are not in session
  // restore.
  tab_manager->GetWebContentsData(contents3_)->SetIsInSessionRestore(false);
  EXPECT_EQ(content::NavigationThrottle::DEFER,
            tab_manager->MaybeThrottleNavigation(throttle3_.get()));

  // The background tab opening session ends after existing tracked tabs have
  // finished loading.
  tab_manager->GetWebContentsData(contents1_)->DidStopLoading();
  tab_manager->GetWebContentsData(contents2_)->DidStopLoading();
  tab_manager->GetWebContentsData(contents3_)->DidStopLoading();
  EXPECT_FALSE(tab_manager->IsInBackgroundTabOpeningSession());
  EXPECT_FALSE(
      tab_manager->stats_collector()->is_in_background_tab_opening_session());
}

TEST_F(TabManagerTest, IsTabRestoredInForeground) {
  TabManager* tab_manager = g_browser_process->GetTabManager();

  std::unique_ptr<WebContents> contents(CreateWebContents());
  contents->WasShown();
  tab_manager->OnWillRestoreTab(contents.get());
  EXPECT_TRUE(tab_manager->IsTabRestoredInForeground(contents.get()));

  contents.reset(CreateWebContents());
  contents->WasHidden();
  tab_manager->OnWillRestoreTab(contents.get());
  EXPECT_FALSE(tab_manager->IsTabRestoredInForeground(contents.get()));
}

}  // namespace resource_coordinator
