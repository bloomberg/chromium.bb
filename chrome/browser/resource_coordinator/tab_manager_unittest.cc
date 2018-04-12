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
#include "base/message_loop/message_loop.h"
#include "base/metrics/field_trial.h"
#include "base/strings/string16.h"
#include "base/test/mock_entropy_provider.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/resource_coordinator/background_tab_navigation_throttle.h"
#include "chrome/browser/resource_coordinator/tab_lifecycle_unit_external.h"
#include "chrome/browser/resource_coordinator/tab_manager_features.h"
#include "chrome/browser/resource_coordinator/tab_manager_resource_coordinator_signal_observer.h"
#include "chrome/browser/resource_coordinator/tab_manager_stats_collector.h"
#include "chrome/browser/resource_coordinator/tab_manager_web_contents_data.h"
#include "chrome/browser/resource_coordinator/time.h"
#include "chrome/browser/sessions/tab_loader.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tab_ui_helper.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/test_tab_strip_model_delegate.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/test_browser_window.h"
#include "chrome/test/base/testing_profile.h"
#include "components/variations/variations_associated_data.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/web_contents_tester.h"
#include "services/resource_coordinator/public/cpp/resource_coordinator_features.h"
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

bool IsTabDiscarded(content::WebContents* web_contents) {
  return TabLifecycleUnitExternal::FromWebContents(web_contents)->IsDiscarded();
}

}  // namespace

class TabManagerTest : public ChromeRenderViewHostTestHarness {
 public:
  TabManagerTest()
      : scoped_context_(
            std::make_unique<base::TestMockTimeTaskRunner::ScopedContext>(
                task_runner_)),
        scoped_set_tick_clock_for_testing_(task_runner_->GetMockTickClock()) {
    base::MessageLoop::current()->SetTaskRunner(task_runner_);
  }

  WebContents* CreateWebContents() {
    content::WebContents* web_contents = CreateTestWebContents();
    // Commit an URL to allow discarding.
    content::WebContentsTester::For(web_contents)
        ->NavigateAndCommit(GURL("https://www.example.com"));
    return web_contents;
  }

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    tab_manager_ = g_browser_process->GetTabManager();
  }

  void TearDown() override {
    // NavigationHandles and NavigationThrottles must be deleted before the
    // associated WebContents.
    nav_handle1_.reset();
    nav_handle2_.reset();
    nav_handle3_.reset();
    throttle1_.reset();
    throttle2_.reset();
    throttle3_.reset();

    // WebContents must be deleted before
    // ChromeRenderViewHostTestHarness::TearDown() deletes the
    // RenderProcessHost.
    contents1_.reset();
    contents2_.reset();
    contents3_.reset();

    task_runner_->RunUntilIdle();
    scoped_context_.reset();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  void PrepareTabs(const char* url1 = kTestUrl,
                   const char* url2 = kTestUrl,
                   const char* url3 = kTestUrl) {
    nav_handle1_ = CreateTabAndNavigation(url1);
    nav_handle2_ = CreateTabAndNavigation(url2);
    nav_handle3_ = CreateTabAndNavigation(url3);
    contents1_ = base::WrapUnique(nav_handle1_->GetWebContents());
    contents2_ = base::WrapUnique(nav_handle2_->GetWebContents());
    contents3_ = base::WrapUnique(nav_handle3_->GetWebContents());

    contents1_->WasHidden();
    contents2_->WasHidden();
    contents3_->WasHidden();

    throttle1_ = std::make_unique<NonResumingBackgroundTabNavigationThrottle>(
        nav_handle1_.get());
    throttle2_ = std::make_unique<NonResumingBackgroundTabNavigationThrottle>(
        nav_handle2_.get());
    throttle3_ = std::make_unique<NonResumingBackgroundTabNavigationThrottle>(
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
    content::WebContents* web_contents = CreateTestWebContents();
    TabUIHelper::CreateForWebContents(web_contents);
    return content::NavigationHandle::CreateNavigationHandleForTesting(
        GURL(url), web_contents->GetMainFrame());
  }

  TabManager* tab_manager_ = nullptr;
  scoped_refptr<base::TestMockTimeTaskRunner> task_runner_ =
      base::MakeRefCounted<base::TestMockTimeTaskRunner>();
  std::unique_ptr<base::TestMockTimeTaskRunner::ScopedContext> scoped_context_;
  ScopedSetTickClockForTesting scoped_set_tick_clock_for_testing_;
  std::unique_ptr<BackgroundTabNavigationThrottle> throttle1_;
  std::unique_ptr<BackgroundTabNavigationThrottle> throttle2_;
  std::unique_ptr<BackgroundTabNavigationThrottle> throttle3_;
  std::unique_ptr<NavigationHandle> nav_handle1_;
  std::unique_ptr<NavigationHandle> nav_handle2_;
  std::unique_ptr<NavigationHandle> nav_handle3_;
  std::unique_ptr<WebContents> contents1_;
  std::unique_ptr<WebContents> contents2_;
  std::unique_ptr<WebContents> contents3_;
};

class TabManagerWithExperimentDisabledTest : public TabManagerTest {
 public:
  void SetUp() override {
    scoped_feature_list_.InitAndDisableFeature(
        features::kStaggeredBackgroundTabOpeningExperiment);
    TabManagerTest::SetUp();
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

TEST_F(TabManagerTest, DefaultTimeToPurgeInCorrectRange) {
  base::TimeDelta time_to_purge =
      tab_manager_->GetTimeToPurge(TabManager::kDefaultMinTimeToPurge,
                                   TabManager::kDefaultMinTimeToPurge * 4);
  EXPECT_GE(time_to_purge, base::TimeDelta::FromMinutes(1));
  EXPECT_LE(time_to_purge, base::TimeDelta::FromMinutes(4));
}

TEST_F(TabManagerTest, ShouldPurgeAtDefaultTime) {
  auto window = std::make_unique<TestBrowserWindow>();
  Browser::CreateParams params(profile(), true);
  params.type = Browser::TYPE_TABBED;
  params.window = window.get();
  auto browser = std::make_unique<Browser>(params);
  TabStripModel* tab_strip = browser->tab_strip_model();

  WebContents* test_contents = CreateWebContents();
  tab_strip->AppendWebContents(test_contents, false);

  tab_manager_->GetWebContentsData(test_contents)->set_is_purged(false);
  tab_manager_->GetWebContentsData(test_contents)
      ->SetLastInactiveTime(NowTicks());
  tab_manager_->GetWebContentsData(test_contents)
      ->set_time_to_purge(base::TimeDelta::FromMinutes(1));

  // Wait 1 minute and verify that the tab is still not to be purged.
  task_runner_->FastForwardBy(base::TimeDelta::FromMinutes(1));
  EXPECT_FALSE(tab_manager_->ShouldPurgeNow(test_contents));

  // Wait another 1 second and verify that it should be purged now .
  task_runner_->FastForwardBy(base::TimeDelta::FromSeconds(1));
  EXPECT_TRUE(tab_manager_->ShouldPurgeNow(test_contents));

  tab_manager_->GetWebContentsData(test_contents)->set_is_purged(true);
  tab_manager_->GetWebContentsData(test_contents)
      ->SetLastInactiveTime(NowTicks());

  // Wait 1 day and verify that the tab is still be purged.
  task_runner_->FastForwardBy(base::TimeDelta::FromHours(24));
  EXPECT_FALSE(tab_manager_->ShouldPurgeNow(test_contents));

  // Tabs with a committed URL must be closed explicitly to avoid DCHECK errors.
  tab_strip->CloseAllTabs();
}

TEST_F(TabManagerTest, ActivateTabResetPurgeState) {
  auto window = std::make_unique<TestBrowserWindow>();
  Browser::CreateParams params(profile(), true);
  params.type = Browser::TYPE_TABBED;
  params.window = window.get();
  auto browser = std::make_unique<Browser>(params);
  TabStripModel* tabstrip = browser->tab_strip_model();

  WebContents* tab1 = CreateWebContents();
  WebContents* tab2 = CreateWebContents();
  tabstrip->AppendWebContents(tab1, true);
  tabstrip->AppendWebContents(tab2, false);

  tab_manager_->GetWebContentsData(tab2)->SetLastInactiveTime(NowTicks());
  static_cast<content::MockRenderProcessHost*>(
      tab2->GetMainFrame()->GetProcess())
      ->set_is_process_backgrounded(true);
  EXPECT_TRUE(tab2->GetMainFrame()->GetProcess()->IsProcessBackgrounded());

  // Initially PurgeAndSuspend state should be NOT_PURGED.
  EXPECT_FALSE(tab_manager_->GetWebContentsData(tab2)->is_purged());
  tab_manager_->GetWebContentsData(tab2)->set_time_to_purge(
      base::TimeDelta::FromMinutes(1));
  task_runner_->FastForwardBy(base::TimeDelta::FromMinutes(2));
  tab_manager_->PurgeBackgroundedTabsIfNeeded();
  // Since tab2 is kept inactive and background for more than time-to-purge,
  // tab2 should be purged.
  EXPECT_TRUE(tab_manager_->GetWebContentsData(tab2)->is_purged());

  // Activate tab2. Tab2's PurgeAndSuspend state should be NOT_PURGED.
  tabstrip->ActivateTabAt(1, true /* user_gesture */);
  EXPECT_FALSE(tab_manager_->GetWebContentsData(tab2)->is_purged());

  // Tabs with a committed URL must be closed explicitly to avoid DCHECK errors.
  tabstrip->CloseAllTabs();
}

// Data race on Linux. http://crbug.com/787842
#if defined(OS_LINUX)
#define MAYBE_DiscardTabWithNonVisibleTabs DISABLED_DiscardTabWithNonVisibleTabs
#else
#define MAYBE_DiscardTabWithNonVisibleTabs DiscardTabWithNonVisibleTabs
#endif

// Verify that:
// - On ChromeOS, DiscardTab can discard every non-visible tab, but cannot
//   discard a visible tab.
// - On other platforms, DiscardTab can discard every tab that is not active in
//   its tab strip.
TEST_F(TabManagerTest, MAYBE_DiscardTabWithNonVisibleTabs) {
  // Create 2 tab strips. Simulate the second tab strip being hidden by hiding
  // its active tab.
  auto window1 = std::make_unique<TestBrowserWindow>();
  Browser::CreateParams params1(profile(), true);
  params1.type = Browser::TYPE_TABBED;
  params1.window = window1.get();
  auto browser1 = std::make_unique<Browser>(params1);
  TabStripModel* tab_strip1 = browser1->tab_strip_model();
  tab_strip1->AppendWebContents(CreateWebContents(), true);
  tab_strip1->AppendWebContents(CreateWebContents(), false);
  tab_strip1->GetWebContentsAt(0)->WasShown();
  tab_strip1->GetWebContentsAt(1)->WasHidden();

  auto window2 = std::make_unique<TestBrowserWindow>();
  Browser::CreateParams params2(profile(), true);
  params2.type = Browser::TYPE_TABBED;
  params2.window = window2.get();
  auto browser2 = std::make_unique<Browser>(params1);
  TabStripModel* tab_strip2 = browser2->tab_strip_model();
  tab_strip2->AppendWebContents(CreateWebContents(), true);
  tab_strip2->AppendWebContents(CreateWebContents(), false);
  tab_strip2->GetWebContentsAt(0)->WasHidden();
  tab_strip2->GetWebContentsAt(1)->WasHidden();

  for (int i = 0; i < 4; ++i)
    tab_manager_->DiscardTab(DiscardReason::kProactive);

  // Active tab in a visible window should not be discarded.
  EXPECT_FALSE(IsTabDiscarded(tab_strip1->GetWebContentsAt(0)));

  // Non-active tabs should be discarded.
  EXPECT_TRUE(IsTabDiscarded(tab_strip1->GetWebContentsAt(1)));
  EXPECT_TRUE(IsTabDiscarded(tab_strip2->GetWebContentsAt(1)));

#if defined(OS_CHROMEOS)
  // On ChromeOS, a non-visible tab should be discarded even if it's active in
  // its tab strip.
  EXPECT_TRUE(IsTabDiscarded(tab_strip2->GetWebContentsAt(0)));
#else
  // On other platforms, an active tab is never discarded, even if it's not
  // visible.
  EXPECT_FALSE(IsTabDiscarded(tab_strip2->GetWebContentsAt(0)));
#endif  // defined(OS_CHROMEOS)

  // Tabs with a committed URL must be closed explicitly to avoid DCHECK errors.
  tab_strip1->CloseAllTabs();
  tab_strip2->CloseAllTabs();
}

TEST_F(TabManagerTest, MaybeThrottleNavigation) {
  tab_manager_->ResetMemoryPressureListenerForTest();
  MaybeThrottleNavigations(tab_manager_);
  tab_manager_->GetWebContentsData(contents1_.get())
      ->DidStartNavigation(nav_handle1_.get());

  // Tab 1 is loading. The other 2 tabs's navigations are delayed.
  EXPECT_TRUE(tab_manager_->IsTabLoadingForTest(contents1_.get()));
  EXPECT_FALSE(tab_manager_->IsTabLoadingForTest(contents2_.get()));
  EXPECT_FALSE(tab_manager_->IsTabLoadingForTest(contents3_.get()));

  EXPECT_FALSE(tab_manager_->IsNavigationDelayedForTest(nav_handle1_.get()));
  EXPECT_TRUE(tab_manager_->IsNavigationDelayedForTest(nav_handle2_.get()));
  EXPECT_TRUE(tab_manager_->IsNavigationDelayedForTest(nav_handle3_.get()));
}

TEST_F(TabManagerTest, OnDidFinishNavigation) {
  tab_manager_->ResetMemoryPressureListenerForTest();
  MaybeThrottleNavigations(tab_manager_);
  tab_manager_->GetWebContentsData(contents1_.get())
      ->DidStartNavigation(nav_handle1_.get());

  EXPECT_TRUE(tab_manager_->IsNavigationDelayedForTest(nav_handle2_.get()));
  tab_manager_->GetWebContentsData(contents2_.get())
      ->DidFinishNavigation(nav_handle2_.get());
  EXPECT_FALSE(tab_manager_->IsNavigationDelayedForTest(nav_handle2_.get()));
}

TEST_F(TabManagerTest, OnTabIsLoaded) {
  tab_manager_->ResetMemoryPressureListenerForTest();
  MaybeThrottleNavigations(tab_manager_);
  tab_manager_->GetWebContentsData(contents1_.get())
      ->DidStartNavigation(nav_handle1_.get());

  EXPECT_TRUE(tab_manager_->IsTabLoadingForTest(contents1_.get()));
  EXPECT_FALSE(tab_manager_->IsTabLoadingForTest(contents2_.get()));
  EXPECT_TRUE(tab_manager_->IsNavigationDelayedForTest(nav_handle2_.get()));

  // Simulate tab 1 has finished loading.
  tab_manager_->GetWebContentsData(contents1_.get())->NotifyTabIsLoaded();

  // After tab 1 has finished loading, TabManager starts loading the next tab.
  EXPECT_FALSE(tab_manager_->IsTabLoadingForTest(contents1_.get()));
  EXPECT_TRUE(tab_manager_->IsTabLoadingForTest(contents2_.get()));
  EXPECT_FALSE(tab_manager_->IsNavigationDelayedForTest(nav_handle2_.get()));
}

TEST_F(TabManagerTest, OnWebContentsDestroyed) {
  tab_manager_->ResetMemoryPressureListenerForTest();
  MaybeThrottleNavigations(tab_manager_);
  tab_manager_->GetWebContentsData(contents1_.get())
      ->DidStartNavigation(nav_handle1_.get());

  // Tab 2 is destroyed when its navigation is still delayed. Its states are
  // cleaned up.
  EXPECT_FALSE(tab_manager_->IsTabLoadingForTest(contents2_.get()));
  EXPECT_TRUE(tab_manager_->IsNavigationDelayedForTest(nav_handle2_.get()));
  tab_manager_->OnWebContentsDestroyed(contents2_.get());
  EXPECT_FALSE(tab_manager_->IsTabLoadingForTest(contents2_.get()));
  EXPECT_FALSE(tab_manager_->IsNavigationDelayedForTest(nav_handle2_.get()));

  // Tab 1 is destroyed when it is still loading. Its states are cleaned up and
  // Tabmanager starts to load the next tab (tab 3).
  EXPECT_TRUE(tab_manager_->IsTabLoadingForTest(contents1_.get()));
  EXPECT_FALSE(tab_manager_->IsTabLoadingForTest(contents3_.get()));
  EXPECT_TRUE(tab_manager_->IsNavigationDelayedForTest(nav_handle3_.get()));
  tab_manager_->GetWebContentsData(contents1_.get())->WebContentsDestroyed();
  EXPECT_FALSE(tab_manager_->IsTabLoadingForTest(contents1_.get()));
  EXPECT_TRUE(tab_manager_->IsTabLoadingForTest(contents3_.get()));
  EXPECT_FALSE(tab_manager_->IsNavigationDelayedForTest(nav_handle3_.get()));
}

TEST_F(TabManagerTest, OnDelayedTabSelected) {
  tab_manager_->ResetMemoryPressureListenerForTest();
  MaybeThrottleNavigations(tab_manager_);
  tab_manager_->GetWebContentsData(contents1_.get())
      ->DidStartNavigation(nav_handle1_.get());

  EXPECT_TRUE(tab_manager_->IsTabLoadingForTest(contents1_.get()));
  EXPECT_FALSE(tab_manager_->IsTabLoadingForTest(contents3_.get()));
  EXPECT_TRUE(tab_manager_->IsNavigationDelayedForTest(nav_handle3_.get()));

  // Simulate selecting tab 3, which should start loading immediately.
  tab_manager_->ActiveTabChanged(
      contents1_.get(), contents3_.get(), 2,
      TabStripModelObserver::CHANGE_REASON_USER_GESTURE);

  EXPECT_TRUE(tab_manager_->IsTabLoadingForTest(contents1_.get()));
  EXPECT_TRUE(tab_manager_->IsTabLoadingForTest(contents3_.get()));
  EXPECT_FALSE(tab_manager_->IsNavigationDelayedForTest(nav_handle3_.get()));

  // Simulate tab 1 has finished loading. TabManager will NOT load the next tab
  // (tab 2) because tab 3 is still loading.
  EXPECT_FALSE(tab_manager_->IsTabLoadingForTest(contents2_.get()));
  EXPECT_TRUE(tab_manager_->IsNavigationDelayedForTest(nav_handle2_.get()));
  tab_manager_->GetWebContentsData(contents1_.get())->NotifyTabIsLoaded();
  EXPECT_FALSE(tab_manager_->IsTabLoadingForTest(contents1_.get()));
  EXPECT_FALSE(tab_manager_->IsTabLoadingForTest(contents2_.get()));
  EXPECT_TRUE(tab_manager_->IsNavigationDelayedForTest(nav_handle2_.get()));

  // Simulate tab 3 has finished loading. TabManager starts loading the next tab
  // (tab 2).
  tab_manager_->GetWebContentsData(contents3_.get())->NotifyTabIsLoaded();
  EXPECT_FALSE(tab_manager_->IsTabLoadingForTest(contents3_.get()));
  EXPECT_TRUE(tab_manager_->IsTabLoadingForTest(contents2_.get()));
  EXPECT_FALSE(tab_manager_->IsNavigationDelayedForTest(nav_handle2_.get()));
}

TEST_F(TabManagerTest, TimeoutWhenLoadingBackgroundTabs) {
  tab_manager_->ResetMemoryPressureListenerForTest();

  MaybeThrottleNavigations(tab_manager_);
  tab_manager_->GetWebContentsData(contents1_.get())
      ->DidStartNavigation(nav_handle1_.get());

  EXPECT_TRUE(tab_manager_->IsTabLoadingForTest(contents1_.get()));
  EXPECT_FALSE(tab_manager_->IsTabLoadingForTest(contents2_.get()));
  EXPECT_FALSE(tab_manager_->IsTabLoadingForTest(contents3_.get()));
  EXPECT_FALSE(tab_manager_->IsNavigationDelayedForTest(nav_handle1_.get()));
  EXPECT_TRUE(tab_manager_->IsNavigationDelayedForTest(nav_handle2_.get()));
  EXPECT_TRUE(tab_manager_->IsNavigationDelayedForTest(nav_handle3_.get()));

  // Simulate timeout when loading the 1st tab. TabManager should start loading
  // the 2nd tab.
  task_runner_->FastForwardBy(base::TimeDelta::FromSeconds(10));

  EXPECT_TRUE(tab_manager_->IsTabLoadingForTest(contents1_.get()));
  EXPECT_TRUE(tab_manager_->IsTabLoadingForTest(contents2_.get()));
  EXPECT_FALSE(tab_manager_->IsTabLoadingForTest(contents3_.get()));
  EXPECT_FALSE(tab_manager_->IsNavigationDelayedForTest(nav_handle1_.get()));
  EXPECT_FALSE(tab_manager_->IsNavigationDelayedForTest(nav_handle2_.get()));
  EXPECT_TRUE(tab_manager_->IsNavigationDelayedForTest(nav_handle3_.get()));

  // Simulate timeout again. TabManager should start loading the 3rd tab.
  task_runner_->FastForwardBy(base::TimeDelta::FromSeconds(10));

  EXPECT_TRUE(tab_manager_->IsTabLoadingForTest(contents1_.get()));
  EXPECT_TRUE(tab_manager_->IsTabLoadingForTest(contents2_.get()));
  EXPECT_TRUE(tab_manager_->IsTabLoadingForTest(contents3_.get()));
  EXPECT_FALSE(tab_manager_->IsNavigationDelayedForTest(nav_handle1_.get()));
  EXPECT_FALSE(tab_manager_->IsNavigationDelayedForTest(nav_handle2_.get()));
  EXPECT_FALSE(tab_manager_->IsNavigationDelayedForTest(nav_handle3_.get()));
}

TEST_F(TabManagerTest, BackgroundTabLoadingMode) {
  tab_manager_->ResetMemoryPressureListenerForTest();

  EXPECT_EQ(TabManager::BackgroundTabLoadingMode::kStaggered,
            tab_manager_->background_tab_loading_mode_);

  MaybeThrottleNavigations(tab_manager_);
  tab_manager_->GetWebContentsData(contents1_.get())
      ->DidStartNavigation(nav_handle1_.get());

  EXPECT_TRUE(tab_manager_->IsTabLoadingForTest(contents1_.get()));
  EXPECT_FALSE(tab_manager_->IsTabLoadingForTest(contents2_.get()));
  EXPECT_FALSE(tab_manager_->IsTabLoadingForTest(contents3_.get()));
  EXPECT_FALSE(tab_manager_->IsNavigationDelayedForTest(nav_handle1_.get()));
  EXPECT_TRUE(tab_manager_->IsNavigationDelayedForTest(nav_handle2_.get()));
  EXPECT_TRUE(tab_manager_->IsNavigationDelayedForTest(nav_handle3_.get()));

  // TabManager pauses loading pending background tabs.
  tab_manager_->PauseBackgroundTabOpeningIfNeeded();
  EXPECT_EQ(TabManager::BackgroundTabLoadingMode::kPaused,
            tab_manager_->background_tab_loading_mode_);

  // Simulate timeout when loading the 1st tab.
  task_runner_->FastForwardBy(base::TimeDelta::FromSeconds(10));

  // Tab 2 and 3 are still pending because of the paused loading mode.
  EXPECT_FALSE(tab_manager_->IsTabLoadingForTest(contents2_.get()));
  EXPECT_FALSE(tab_manager_->IsTabLoadingForTest(contents3_.get()));
  EXPECT_TRUE(tab_manager_->IsNavigationDelayedForTest(nav_handle2_.get()));
  EXPECT_TRUE(tab_manager_->IsNavigationDelayedForTest(nav_handle3_.get()));

  // Simulate tab 1 has finished loading.
  tab_manager_->GetWebContentsData(contents1_.get())->NotifyTabIsLoaded();

  // Tab 2 and 3 are still pending because of the paused loading mode.
  EXPECT_FALSE(tab_manager_->IsTabLoadingForTest(contents2_.get()));
  EXPECT_FALSE(tab_manager_->IsTabLoadingForTest(contents3_.get()));
  EXPECT_TRUE(tab_manager_->IsNavigationDelayedForTest(nav_handle2_.get()));
  EXPECT_TRUE(tab_manager_->IsNavigationDelayedForTest(nav_handle3_.get()));

  // TabManager resumes loading pending background tabs.
  tab_manager_->ResumeBackgroundTabOpeningIfNeeded();
  EXPECT_EQ(TabManager::BackgroundTabLoadingMode::kStaggered,
            tab_manager_->background_tab_loading_mode_);

  // Tab 2 should start loading right away.
  EXPECT_TRUE(tab_manager_->IsTabLoadingForTest(contents2_.get()));
  EXPECT_FALSE(tab_manager_->IsNavigationDelayedForTest(nav_handle2_.get()));

  // Simulate tab 2 has finished loading.
  tab_manager_->GetWebContentsData(contents2_.get())->NotifyTabIsLoaded();

  // Tab 3 should start loading now in staggered loading mode.
  EXPECT_TRUE(tab_manager_->IsTabLoadingForTest(contents3_.get()));
  EXPECT_FALSE(tab_manager_->IsNavigationDelayedForTest(nav_handle3_.get()));
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
  tab_manager_->ResetMemoryPressureListenerForTest();

  MaybeThrottleNavigations(
      tab_manager_, 1, kTestUrl,
      chrome::kChromeUISettingsURL,  // Using internal page URL for tab 2.
      kTestUrl);
  tab_manager_->GetWebContentsData(contents1_.get())
      ->DidStartNavigation(nav_handle1_.get());

  EXPECT_TRUE(tab_manager_->IsTabLoadingForTest(contents1_.get()));
  EXPECT_FALSE(tab_manager_->IsTabLoadingForTest(contents2_.get()));
  EXPECT_FALSE(tab_manager_->IsTabLoadingForTest(contents3_.get()));
  EXPECT_FALSE(tab_manager_->IsNavigationDelayedForTest(nav_handle1_.get()));
  EXPECT_TRUE(tab_manager_->IsNavigationDelayedForTest(nav_handle2_.get()));
  EXPECT_TRUE(tab_manager_->IsNavigationDelayedForTest(nav_handle3_.get()));

  // Simulate tab 1 has finished loading. Tab 3 should be loaded before tab 2,
  // because tab 2 is internal page.
  tab_manager_->GetWebContentsData(contents1_.get())->NotifyTabIsLoaded();

  EXPECT_FALSE(tab_manager_->IsTabLoadingForTest(contents2_.get()));
  EXPECT_TRUE(tab_manager_->IsTabLoadingForTest(contents3_.get()));
  EXPECT_TRUE(tab_manager_->IsNavigationDelayedForTest(nav_handle2_.get()));
  EXPECT_FALSE(tab_manager_->IsNavigationDelayedForTest(nav_handle3_.get()));
}

TEST_F(TabManagerTest, PauseAndResumeBackgroundTabOpening) {
  tab_manager_->ResetMemoryPressureListenerForTest();
  PrepareTabs();

  EXPECT_EQ(TabManager::BackgroundTabLoadingMode::kStaggered,
            tab_manager_->background_tab_loading_mode_);
  EXPECT_FALSE(tab_manager_->IsInBackgroundTabOpeningSession());
  EXPECT_FALSE(
      tab_manager_->stats_collector()->is_in_background_tab_opening_session());

  // Start background tab opening session.
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            tab_manager_->MaybeThrottleNavigation(throttle1_.get()));
  tab_manager_->GetWebContentsData(contents1_.get())
      ->DidStartNavigation(nav_handle1_.get());
  EXPECT_TRUE(tab_manager_->IsInBackgroundTabOpeningSession());
  EXPECT_TRUE(
      tab_manager_->stats_collector()->is_in_background_tab_opening_session());

  // TabManager pauses loading pending background tabs.
  tab_manager_->PauseBackgroundTabOpeningIfNeeded();
  EXPECT_EQ(TabManager::BackgroundTabLoadingMode::kPaused,
            tab_manager_->background_tab_loading_mode_);
  EXPECT_FALSE(tab_manager_->IsInBackgroundTabOpeningSession());
  EXPECT_FALSE(
      tab_manager_->stats_collector()->is_in_background_tab_opening_session());

  // Simulate tab 1 has finished loading, which was scheduled to load before
  // pausing.
  tab_manager_->GetWebContentsData(contents1_.get())->NotifyTabIsLoaded();

  // TabManager cannot enter BackgroundTabOpening session when it is in paused
  // mode.
  EXPECT_EQ(content::NavigationThrottle::DEFER,
            tab_manager_->MaybeThrottleNavigation(throttle2_.get()));
  EXPECT_TRUE(tab_manager_->IsNavigationDelayedForTest(nav_handle2_.get()));
  EXPECT_FALSE(tab_manager_->IsInBackgroundTabOpeningSession());
  EXPECT_FALSE(
      tab_manager_->stats_collector()->is_in_background_tab_opening_session());

  // TabManager resumes loading pending background tabs.
  tab_manager_->ResumeBackgroundTabOpeningIfNeeded();
  EXPECT_EQ(TabManager::BackgroundTabLoadingMode::kStaggered,
            tab_manager_->background_tab_loading_mode_);
  EXPECT_TRUE(tab_manager_->IsInBackgroundTabOpeningSession());
  EXPECT_TRUE(
      tab_manager_->stats_collector()->is_in_background_tab_opening_session());

  // Tab 2 should start loading right away.
  EXPECT_TRUE(tab_manager_->IsTabLoadingForTest(contents2_.get()));
  EXPECT_FALSE(tab_manager_->IsNavigationDelayedForTest(nav_handle2_.get()));
}

TEST_F(TabManagerTest, IsInBackgroundTabOpeningSession) {
  tab_manager_->ResetMemoryPressureListenerForTest();
  EXPECT_FALSE(tab_manager_->IsInBackgroundTabOpeningSession());
  EXPECT_FALSE(
      tab_manager_->stats_collector()->is_in_background_tab_opening_session());

  MaybeThrottleNavigations(tab_manager_);
  tab_manager_->GetWebContentsData(contents1_.get())
      ->DidStartNavigation(nav_handle1_.get());
  EXPECT_TRUE(tab_manager_->IsInBackgroundTabOpeningSession());
  EXPECT_TRUE(
      tab_manager_->stats_collector()->is_in_background_tab_opening_session());

  tab_manager_->GetWebContentsData(contents1_.get())->NotifyTabIsLoaded();
  EXPECT_TRUE(tab_manager_->IsInBackgroundTabOpeningSession());
  EXPECT_TRUE(
      tab_manager_->stats_collector()->is_in_background_tab_opening_session());

  tab_manager_->GetWebContentsData(contents2_.get())->NotifyTabIsLoaded();
  EXPECT_TRUE(tab_manager_->IsInBackgroundTabOpeningSession());
  EXPECT_TRUE(
      tab_manager_->stats_collector()->is_in_background_tab_opening_session());

  // It is still in background tab opening session even if tab3 is brought to
  // foreground. The session only ends after tab1, tab2 and tab3 have all
  // finished loading.
  contents3_->WasShown();
  EXPECT_TRUE(tab_manager_->IsInBackgroundTabOpeningSession());
  EXPECT_TRUE(
      tab_manager_->stats_collector()->is_in_background_tab_opening_session());

  tab_manager_->GetWebContentsData(contents3_.get())->NotifyTabIsLoaded();
  EXPECT_FALSE(tab_manager_->IsInBackgroundTabOpeningSession());
  EXPECT_FALSE(
      tab_manager_->stats_collector()->is_in_background_tab_opening_session());
}

TEST_F(TabManagerWithExperimentDisabledTest, IsInBackgroundTabOpeningSession) {
  EXPECT_FALSE(base::FeatureList::IsEnabled(
      features::kStaggeredBackgroundTabOpeningExperiment));

  tab_manager_->ResetMemoryPressureListenerForTest();
  EXPECT_FALSE(tab_manager_->IsInBackgroundTabOpeningSession());
  EXPECT_FALSE(
      tab_manager_->stats_collector()->is_in_background_tab_opening_session());

  MaybeThrottleNavigations(tab_manager_);
  tab_manager_->GetWebContentsData(contents1_.get())
      ->DidStartNavigation(nav_handle1_.get());
  tab_manager_->GetWebContentsData(contents2_.get())
      ->DidStartNavigation(nav_handle1_.get());
  tab_manager_->GetWebContentsData(contents3_.get())
      ->DidStartNavigation(nav_handle1_.get());

  EXPECT_TRUE(tab_manager_->IsTabLoadingForTest(contents1_.get()));
  EXPECT_TRUE(tab_manager_->IsTabLoadingForTest(contents2_.get()));
  EXPECT_TRUE(tab_manager_->IsTabLoadingForTest(contents3_.get()));
  EXPECT_FALSE(tab_manager_->IsNavigationDelayedForTest(nav_handle1_.get()));
  EXPECT_FALSE(tab_manager_->IsNavigationDelayedForTest(nav_handle2_.get()));
  EXPECT_FALSE(tab_manager_->IsNavigationDelayedForTest(nav_handle3_.get()));
  EXPECT_TRUE(tab_manager_->IsInBackgroundTabOpeningSession());
  EXPECT_TRUE(
      tab_manager_->stats_collector()->is_in_background_tab_opening_session());

  tab_manager_->GetWebContentsData(contents1_.get())->NotifyTabIsLoaded();
  EXPECT_FALSE(tab_manager_->IsTabLoadingForTest(contents1_.get()));
  EXPECT_TRUE(tab_manager_->IsTabLoadingForTest(contents2_.get()));
  EXPECT_TRUE(tab_manager_->IsTabLoadingForTest(contents3_.get()));
  EXPECT_TRUE(tab_manager_->IsInBackgroundTabOpeningSession());
  EXPECT_TRUE(
      tab_manager_->stats_collector()->is_in_background_tab_opening_session());

  tab_manager_->GetWebContentsData(contents2_.get())->NotifyTabIsLoaded();
  EXPECT_FALSE(tab_manager_->IsTabLoadingForTest(contents1_.get()));
  EXPECT_FALSE(tab_manager_->IsTabLoadingForTest(contents2_.get()));
  EXPECT_TRUE(tab_manager_->IsTabLoadingForTest(contents3_.get()));
  EXPECT_TRUE(tab_manager_->IsInBackgroundTabOpeningSession());
  EXPECT_TRUE(
      tab_manager_->stats_collector()->is_in_background_tab_opening_session());

  // It is still in background tab opening session even if tab3 is brought to
  // foreground. The session only ends after tab1, tab2 and tab3 have all
  // finished loading.
  contents3_->WasShown();
  EXPECT_TRUE(tab_manager_->IsInBackgroundTabOpeningSession());
  EXPECT_TRUE(
      tab_manager_->stats_collector()->is_in_background_tab_opening_session());

  tab_manager_->GetWebContentsData(contents3_.get())->NotifyTabIsLoaded();
  EXPECT_FALSE(tab_manager_->IsTabLoadingForTest(contents1_.get()));
  EXPECT_FALSE(tab_manager_->IsTabLoadingForTest(contents2_.get()));
  EXPECT_FALSE(tab_manager_->IsTabLoadingForTest(contents3_.get()));
  EXPECT_FALSE(tab_manager_->IsInBackgroundTabOpeningSession());
  EXPECT_FALSE(
      tab_manager_->stats_collector()->is_in_background_tab_opening_session());
}

TEST_F(TabManagerTest, SessionRestoreBeforeBackgroundTabOpeningSession) {
  tab_manager_->ResetMemoryPressureListenerForTest();
  PrepareTabs();

  // Start session restore.
  tab_manager_->OnSessionRestoreStartedLoadingTabs();
  EXPECT_TRUE(tab_manager_->IsSessionRestoreLoadingTabs());
  EXPECT_FALSE(tab_manager_->IsInBackgroundTabOpeningSession());
  EXPECT_FALSE(
      tab_manager_->stats_collector()->is_in_background_tab_opening_session());

  tab_manager_->GetWebContentsData(contents1_.get())
      ->SetIsInSessionRestore(true);
  tab_manager_->GetWebContentsData(contents2_.get())
      ->SetIsInSessionRestore(false);
  tab_manager_->GetWebContentsData(contents3_.get())
      ->SetIsInSessionRestore(false);

  // Do not enter BackgroundTabOpening session if the background tab is in
  // session restore.
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            tab_manager_->MaybeThrottleNavigation(throttle1_.get()));
  EXPECT_FALSE(tab_manager_->IsInBackgroundTabOpeningSession());
  EXPECT_FALSE(
      tab_manager_->stats_collector()->is_in_background_tab_opening_session());

  // Enter BackgroundTabOpening session when there are background tabs not in
  // session restore, though the first background tab can still proceed.
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            tab_manager_->MaybeThrottleNavigation(throttle2_.get()));
  EXPECT_TRUE(tab_manager_->IsInBackgroundTabOpeningSession());
  EXPECT_TRUE(
      tab_manager_->stats_collector()->is_in_background_tab_opening_session());

  EXPECT_EQ(content::NavigationThrottle::DEFER,
            tab_manager_->MaybeThrottleNavigation(throttle3_.get()));
  EXPECT_TRUE(tab_manager_->IsInBackgroundTabOpeningSession());
  EXPECT_TRUE(
      tab_manager_->stats_collector()->is_in_background_tab_opening_session());

  // Stop session restore.
  tab_manager_->OnSessionRestoreFinishedLoadingTabs();
  EXPECT_FALSE(tab_manager_->IsSessionRestoreLoadingTabs());
  EXPECT_TRUE(tab_manager_->IsInBackgroundTabOpeningSession());
  EXPECT_TRUE(
      tab_manager_->stats_collector()->is_in_background_tab_opening_session());
}

TEST_F(TabManagerTest, SessionRestoreAfterBackgroundTabOpeningSession) {
  tab_manager_->ResetMemoryPressureListenerForTest();
  PrepareTabs();

  EXPECT_FALSE(tab_manager_->IsSessionRestoreLoadingTabs());
  EXPECT_FALSE(tab_manager_->IsInBackgroundTabOpeningSession());
  EXPECT_FALSE(
      tab_manager_->stats_collector()->is_in_background_tab_opening_session());

  // Start background tab opening session.
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            tab_manager_->MaybeThrottleNavigation(throttle1_.get()));
  EXPECT_TRUE(tab_manager_->IsInBackgroundTabOpeningSession());
  EXPECT_TRUE(
      tab_manager_->stats_collector()->is_in_background_tab_opening_session());

  EXPECT_EQ(content::NavigationThrottle::DEFER,
            tab_manager_->MaybeThrottleNavigation(throttle2_.get()));
  EXPECT_TRUE(tab_manager_->IsInBackgroundTabOpeningSession());
  EXPECT_TRUE(
      tab_manager_->stats_collector()->is_in_background_tab_opening_session());

  // Now session restore starts after background tab opening session starts.
  tab_manager_->OnSessionRestoreStartedLoadingTabs();
  EXPECT_TRUE(tab_manager_->IsSessionRestoreLoadingTabs());
  EXPECT_TRUE(tab_manager_->IsInBackgroundTabOpeningSession());
  EXPECT_TRUE(
      tab_manager_->stats_collector()->is_in_background_tab_opening_session());

  // The following background tabs are still delayed if they are not in session
  // restore.
  tab_manager_->GetWebContentsData(contents3_.get())
      ->SetIsInSessionRestore(false);
  EXPECT_EQ(content::NavigationThrottle::DEFER,
            tab_manager_->MaybeThrottleNavigation(throttle3_.get()));

  // The background tab opening session ends after existing tracked tabs have
  // finished loading.
  tab_manager_->GetWebContentsData(contents1_.get())->NotifyTabIsLoaded();
  tab_manager_->GetWebContentsData(contents2_.get())->NotifyTabIsLoaded();
  tab_manager_->GetWebContentsData(contents3_.get())->NotifyTabIsLoaded();
  EXPECT_FALSE(tab_manager_->IsInBackgroundTabOpeningSession());
  EXPECT_FALSE(
      tab_manager_->stats_collector()->is_in_background_tab_opening_session());
}

TEST_F(TabManagerTest, IsTabRestoredInForeground) {
  std::unique_ptr<WebContents> contents(CreateWebContents());
  contents->WasShown();
  tab_manager_->OnWillRestoreTab(contents.get());
  EXPECT_TRUE(tab_manager_->IsTabRestoredInForeground(contents.get()));

  contents.reset(CreateWebContents());
  contents->WasHidden();
  tab_manager_->OnWillRestoreTab(contents.get());
  EXPECT_FALSE(tab_manager_->IsTabRestoredInForeground(contents.get()));
}

TEST_F(TabManagerTest, EnablePageAlmostIdleSignal) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kPageAlmostIdle);
  EXPECT_TRUE(base::FeatureList::IsEnabled(features::kPageAlmostIdle));

  tab_manager_->resource_coordinator_signal_observer_.reset(
      new TabManager::ResourceCoordinatorSignalObserver());
  tab_manager_->ResetMemoryPressureListenerForTest();

  EXPECT_EQ(TabManager::BackgroundTabLoadingMode::kStaggered,
            tab_manager_->background_tab_loading_mode_);

  MaybeThrottleNavigations(tab_manager_);
  tab_manager_->GetWebContentsData(contents1_.get())
      ->DidStartNavigation(nav_handle1_.get());

  EXPECT_TRUE(tab_manager_->IsTabLoadingForTest(contents1_.get()));
  EXPECT_FALSE(tab_manager_->IsTabLoadingForTest(contents2_.get()));
  EXPECT_FALSE(tab_manager_->IsTabLoadingForTest(contents3_.get()));
  EXPECT_FALSE(tab_manager_->IsNavigationDelayedForTest(nav_handle1_.get()));
  EXPECT_TRUE(tab_manager_->IsNavigationDelayedForTest(nav_handle2_.get()));
  EXPECT_TRUE(tab_manager_->IsNavigationDelayedForTest(nav_handle3_.get()));

  // Simulate tab 1 has finished loading through WebContentsObserver API.
  // Since the page idle signal feature is enabled, this shouldn't start
  // next loading.
  tab_manager_->GetWebContentsData(contents1_.get())->DidStopLoading();

  // Tab 2 and Tab 3 are still pending because the DidStopLoading signal from
  // WebContentsObserver is disabled when PageAlmostIdle signal is enabled.
  EXPECT_FALSE(tab_manager_->IsTabLoadingForTest(contents2_.get()));
  EXPECT_FALSE(tab_manager_->IsTabLoadingForTest(contents3_.get()));
  EXPECT_TRUE(tab_manager_->IsNavigationDelayedForTest(nav_handle2_.get()));
  EXPECT_TRUE(tab_manager_->IsNavigationDelayedForTest(nav_handle3_.get()));

  // Simulate tab 1 has finished loading by receiving idle signal from resource
  // coordinator. Since the page idle signal feature is enabled, this should
  // start next loading.
  tab_manager_->resource_coordinator_signal_observer_->OnPageAlmostIdle(
      contents1_.get());

  // Tab 2 should start loading right away.
  EXPECT_TRUE(tab_manager_->IsTabLoadingForTest(contents2_.get()));
  EXPECT_FALSE(tab_manager_->IsNavigationDelayedForTest(nav_handle2_.get()));

  // Tab 3 is still pending.
  EXPECT_FALSE(tab_manager_->IsTabLoadingForTest(contents3_.get()));
  EXPECT_TRUE(tab_manager_->IsNavigationDelayedForTest(nav_handle3_.get()));

  // Simulate tab 2 has finished loading by receiving idle signal from resource
  // coordinator.
  tab_manager_->resource_coordinator_signal_observer_->OnPageAlmostIdle(
      contents2_.get());

  // Tab 3 should start loading now in staggered loading mode.
  EXPECT_TRUE(tab_manager_->IsTabLoadingForTest(contents3_.get()));
  EXPECT_FALSE(tab_manager_->IsNavigationDelayedForTest(nav_handle3_.get()));

  // |ignored_web_contents| is not managed by TabManager, thus will be ignored
  // and shouldn't cause any crash or side effect.
  std::unique_ptr<WebContents> ignored_web_contents(
      WebContentsTester::CreateTestWebContents(browser_context(), nullptr));
  tab_manager_->resource_coordinator_signal_observer_->OnPageAlmostIdle(
      ignored_web_contents.get());
  EXPECT_TRUE(tab_manager_->IsTabLoadingForTest(contents3_.get()));
  EXPECT_FALSE(tab_manager_->IsNavigationDelayedForTest(nav_handle3_.get()));
}

TEST_F(TabManagerTest, TrackingNumberOfLoadedLifecycleUnits) {
  auto window = std::make_unique<TestBrowserWindow>();
  Browser::CreateParams params(profile(), true);
  params.type = Browser::TYPE_TABBED;
  params.window = window.get();
  auto browser = std::make_unique<Browser>(params);
  TabStripModel* tab_strip = browser->tab_strip_model();

  // TabManager should start out with 0 loaded LifecycleUnits.
  EXPECT_EQ(tab_manager_->num_loaded_lifecycle_units_, 0);

  // Number of loaded LifecycleUnits should go up by 1 for each new WebContents.
  for (int i = 1; i <= 5; i++) {
    tab_strip->AppendWebContents(CreateWebContents(), false);
    EXPECT_EQ(tab_manager_->num_loaded_lifecycle_units_, i);
  }

  // Closing loaded tabs should reduce |num_loaded_lifecycle_units_| back to the
  // original amount.
  tab_strip->CloseAllTabs();
  EXPECT_EQ(tab_manager_->num_loaded_lifecycle_units_, 0);

  // Number of loaded LifecycleUnits should go up by 1 for each new WebContents.
  for (int i = 1; i <= 5; i++) {
    tab_strip->AppendWebContents(CreateWebContents(), false);
    EXPECT_EQ(tab_manager_->num_loaded_lifecycle_units_, i);
  }

  // Number of loaded LifecycleUnits should go down by 1 for each discarded
  // WebContents.
  for (int i = 0; i < 5; i++) {
    TabLifecycleUnitExternal::FromWebContents(tab_strip->GetWebContentsAt(i))
        ->DiscardTab();
    EXPECT_EQ(tab_manager_->num_loaded_lifecycle_units_, 4 - i);
  }

  // All tabs were discarded, so there should be no loaded LifecycleUnits.
  EXPECT_EQ(tab_manager_->num_loaded_lifecycle_units_, 0);

  tab_strip->CloseAllTabs();

  // Closing discarded tabs shouldn't affect |num_loaded_lifecycle_units_|.
  EXPECT_EQ(tab_manager_->num_loaded_lifecycle_units_, 0);
}

}  // namespace resource_coordinator
