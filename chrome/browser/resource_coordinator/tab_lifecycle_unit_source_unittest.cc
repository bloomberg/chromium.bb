// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/resource_coordinator/tab_lifecycle_unit_source.h"

#include <memory>

#include "base/macros.h"
#include "base/test/simple_test_tick_clock.h"
#include "build/build_config.h"
#include "chrome/browser/resource_coordinator/lifecycle_unit_source_observer.h"
#include "chrome/browser/resource_coordinator/tab_lifecycle_observer.h"
#include "chrome/browser/resource_coordinator/tab_lifecycle_unit.h"
#include "chrome/browser/resource_coordinator/time.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/test_tab_strip_model_delegate.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace resource_coordinator {

namespace {

constexpr base::TimeDelta kShortDelay = base::TimeDelta::FromSeconds(1);

class NoUnloadListenerTabStripModelDelegate : public TestTabStripModelDelegate {
 public:
  NoUnloadListenerTabStripModelDelegate() = default;
  bool RunUnloadListenerBeforeClosing(content::WebContents* contents) override {
    // The default TestTabStripModelDelegate prevents tabs from being closed.
    return false;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(NoUnloadListenerTabStripModelDelegate);
};

class MockLifecycleUnitSourceObserver : public LifecycleUnitSourceObserver {
 public:
  MockLifecycleUnitSourceObserver() = default;

  MOCK_METHOD1(OnLifecycleUnitCreated, void(LifecycleUnit*));
  MOCK_METHOD1(OnLifecycleUnitDestroyed, void(LifecycleUnit*));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockLifecycleUnitSourceObserver);
};

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

bool IsFocused(LifecycleUnit* lifecycle_unit) {
  return lifecycle_unit->GetSortKey().last_focused_time ==
         base::TimeTicks::Max();
}

class TabLifecycleUnitSourceTest : public ChromeRenderViewHostTestHarness {
 protected:
  TabLifecycleUnitSourceTest()
      : scoped_set_tick_clock_for_testing_(&test_clock_) {
    source_.AddObserver(&source_observer_);
    source_.AddTabLifecycleObserver(&tab_observer_);
  }

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    tab_strip_model_ =
        std::make_unique<TabStripModel>(&tab_strip_model_delegate_, profile());
    tab_strip_model_->AddObserver(&source_);
  }

  void TearDown() override {
    // Expect notifications when tabs are closed.
    EXPECT_CALL(source_observer_, OnLifecycleUnitDestroyed(testing::_))
        .Times(tab_strip_model_->count());
    tab_strip_model_->CloseAllTabs();

    tab_strip_model_.reset();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  // If |focus_tab_strip| is true, focuses the tab strip. Then, appends 2 tabs
  // to the tab strip and returns the associated LifecycleUnits via
  // |first_lifecycle_unit| and |second_lifecycle_unit|. The first tab is
  // background and the second tab is active.
  void CreateTwoTabs(bool focus_tab_strip,
                     LifecycleUnit** first_lifecycle_unit,
                     LifecycleUnit** second_lifecycle_unit) {
    if (focus_tab_strip)
      source_.SetFocusedTabStripModelForTesting(tab_strip_model_.get());

    // Add a foreground tab to the tab strip.
    test_clock_.Advance(kShortDelay);
    auto time_before_first_tab = test_clock_.NowTicks();
    EXPECT_CALL(source_observer_, OnLifecycleUnitCreated(testing::_))
        .WillOnce(testing::Invoke([&](LifecycleUnit* lifecycle_unit) {
          *first_lifecycle_unit = lifecycle_unit;

          if (focus_tab_strip) {
            EXPECT_TRUE(IsFocused(*first_lifecycle_unit));
          } else {
            EXPECT_EQ(time_before_first_tab,
                      (*first_lifecycle_unit)->GetSortKey().last_focused_time);
          }
        }));
    content::WebContents* first_web_contents = CreateAndNavigateWebContents();
    tab_strip_model_->AppendWebContents(first_web_contents, true);
    testing::Mock::VerifyAndClear(&source_observer_);
    EXPECT_TRUE(source_.GetTabLifecycleUnitExternal(first_web_contents));

    // Add another foreground tab to the focused tab strip.
    test_clock_.Advance(kShortDelay);
    auto time_before_second_tab = test_clock_.NowTicks();
    EXPECT_CALL(source_observer_, OnLifecycleUnitCreated(testing::_))
        .WillOnce(testing::Invoke([&](LifecycleUnit* lifecycle_unit) {
          *second_lifecycle_unit = lifecycle_unit;

          if (focus_tab_strip) {
            EXPECT_EQ(time_before_second_tab,
                      (*first_lifecycle_unit)->GetSortKey().last_focused_time);
            EXPECT_TRUE(IsFocused(*second_lifecycle_unit));
          } else {
            EXPECT_EQ(time_before_first_tab,
                      (*first_lifecycle_unit)->GetSortKey().last_focused_time);
            EXPECT_EQ(time_before_second_tab,
                      (*second_lifecycle_unit)->GetSortKey().last_focused_time);
          }
        }));
    content::WebContents* second_web_contents = CreateAndNavigateWebContents();
    tab_strip_model_->AppendWebContents(second_web_contents, true);
    testing::Mock::VerifyAndClear(&source_observer_);
    EXPECT_TRUE(source_.GetTabLifecycleUnitExternal(second_web_contents));

    // TabStripModel doesn't update the visibility of its WebContents by itself.
    first_web_contents->WasHidden();
  }

  void TestAppendTabsToTabStrip(bool focus_tab_strip) {
    LifecycleUnit* first_lifecycle_unit = nullptr;
    LifecycleUnit* second_lifecycle_unit = nullptr;
    CreateTwoTabs(focus_tab_strip, &first_lifecycle_unit,
                  &second_lifecycle_unit);

    const base::TimeTicks first_tab_last_focused_time =
        first_lifecycle_unit->GetSortKey().last_focused_time;
    const base::TimeTicks second_tab_last_focused_time =
        second_lifecycle_unit->GetSortKey().last_focused_time;

    // Add a background tab to the focused tab strip.
    test_clock_.Advance(kShortDelay);
    LifecycleUnit* third_lifecycle_unit = nullptr;
    EXPECT_CALL(source_observer_, OnLifecycleUnitCreated(testing::_))
        .WillOnce(testing::Invoke([&](LifecycleUnit* lifecycle_unit) {
          third_lifecycle_unit = lifecycle_unit;

          if (focus_tab_strip) {
            EXPECT_EQ(first_tab_last_focused_time,
                      first_lifecycle_unit->GetSortKey().last_focused_time);
            EXPECT_TRUE(IsFocused(second_lifecycle_unit));
          } else {
            EXPECT_EQ(first_tab_last_focused_time,
                      first_lifecycle_unit->GetSortKey().last_focused_time);
            EXPECT_EQ(second_tab_last_focused_time,
                      second_lifecycle_unit->GetSortKey().last_focused_time);
          }
          EXPECT_EQ(NowTicks(),
                    third_lifecycle_unit->GetSortKey().last_focused_time);
        }));
    content::WebContents* third_web_contents = CreateAndNavigateWebContents();
    tab_strip_model_->AppendWebContents(third_web_contents, false);
    testing::Mock::VerifyAndClear(&source_observer_);
    EXPECT_TRUE(source_.GetTabLifecycleUnitExternal(third_web_contents));

    // Expect notifications when tabs are closed.
    EXPECT_CALL(source_observer_,
                OnLifecycleUnitDestroyed(first_lifecycle_unit));
    EXPECT_CALL(source_observer_,
                OnLifecycleUnitDestroyed(second_lifecycle_unit));
    EXPECT_CALL(source_observer_,
                OnLifecycleUnitDestroyed(third_lifecycle_unit));
    tab_strip_model_->CloseAllTabs();
  }

  TabLifecycleUnitSource source_;
  testing::StrictMock<MockLifecycleUnitSourceObserver> source_observer_;
  testing::StrictMock<MockTabLifecycleObserver> tab_observer_;
  std::unique_ptr<TabStripModel> tab_strip_model_;
  base::SimpleTestTickClock test_clock_;

 private:
  content::WebContents* CreateAndNavigateWebContents() {
    content::WebContents* web_contents = CreateTestWebContents();
    // Commit an URL to allow discarding.
    content::WebContentsTester::For(web_contents)
        ->NavigateAndCommit(GURL("https://www.example.com"));
    return web_contents;
  }

  NoUnloadListenerTabStripModelDelegate tab_strip_model_delegate_;
  ScopedSetTickClockForTesting scoped_set_tick_clock_for_testing_;

  DISALLOW_COPY_AND_ASSIGN(TabLifecycleUnitSourceTest);
};

}  // namespace

TEST_F(TabLifecycleUnitSourceTest, AppendTabsToFocusedTabStrip) {
  TestAppendTabsToTabStrip(true /* focus_tab_strip */);
}

TEST_F(TabLifecycleUnitSourceTest, AppendTabsToNonFocusedTabStrip) {
  TestAppendTabsToTabStrip(false /* focus_tab_strip */);
}

TEST_F(TabLifecycleUnitSourceTest, SwitchTabInFocusedTabStrip) {
  LifecycleUnit* first_lifecycle_unit = nullptr;
  LifecycleUnit* second_lifecycle_unit = nullptr;
  CreateTwoTabs(true /* focus_tab_strip */, &first_lifecycle_unit,
                &second_lifecycle_unit);

  // Activate the first tab.
  test_clock_.Advance(kShortDelay);
  auto time_before_activate = test_clock_.NowTicks();
  tab_strip_model_->ActivateTabAt(0, true);
  EXPECT_TRUE(IsFocused(first_lifecycle_unit));
  EXPECT_EQ(time_before_activate,
            second_lifecycle_unit->GetSortKey().last_focused_time);

  // Expect notifications when tabs are closed.
  EXPECT_CALL(source_observer_, OnLifecycleUnitDestroyed(first_lifecycle_unit));
  EXPECT_CALL(source_observer_,
              OnLifecycleUnitDestroyed(second_lifecycle_unit));
  tab_strip_model_->CloseAllTabs();
}

TEST_F(TabLifecycleUnitSourceTest, CloseTabInFocusedTabStrip) {
  LifecycleUnit* first_lifecycle_unit = nullptr;
  LifecycleUnit* second_lifecycle_unit = nullptr;
  CreateTwoTabs(true /* focus_tab_strip */, &first_lifecycle_unit,
                &second_lifecycle_unit);

  // Close the second tab. The first tab should be focused.
  test_clock_.Advance(kShortDelay);
  EXPECT_CALL(source_observer_,
              OnLifecycleUnitDestroyed(second_lifecycle_unit));
  tab_strip_model_->CloseWebContentsAt(1, 0);
  testing::Mock::VerifyAndClear(&source_observer_);
  EXPECT_TRUE(IsFocused(first_lifecycle_unit));

  // Expect notifications when tabs are closed.
  EXPECT_CALL(source_observer_, OnLifecycleUnitDestroyed(first_lifecycle_unit));
  tab_strip_model_->CloseAllTabs();
}

TEST_F(TabLifecycleUnitSourceTest, ReplaceWebContents) {
  LifecycleUnit* first_lifecycle_unit = nullptr;
  LifecycleUnit* second_lifecycle_unit = nullptr;
  CreateTwoTabs(true /* focus_tab_strip */, &first_lifecycle_unit,
                &second_lifecycle_unit);

  // Replace the WebContents in the active tab with a second WebContents. Expect
  // GetTabLifecycleUnitExternal() to return the TabLifecycleUnitExternal when
  // called with the second WebContents as argument.
  content::WebContents* original_web_contents =
      tab_strip_model_->GetWebContentsAt(1);
  TabLifecycleUnitExternal* tab_lifecycle_unit_external =
      source_.GetTabLifecycleUnitExternal(original_web_contents);
  content::WebContents* new_web_contents = CreateTestWebContents();
  EXPECT_EQ(original_web_contents,
            tab_strip_model_->ReplaceWebContentsAt(1, new_web_contents));
  EXPECT_FALSE(source_.GetTabLifecycleUnitExternal(original_web_contents));
  EXPECT_EQ(tab_lifecycle_unit_external,
            source_.GetTabLifecycleUnitExternal(new_web_contents));

  delete original_web_contents;

  // Expect notifications when tabs are closed.
  EXPECT_CALL(source_observer_, OnLifecycleUnitDestroyed(first_lifecycle_unit));
  EXPECT_CALL(source_observer_,
              OnLifecycleUnitDestroyed(second_lifecycle_unit));
  tab_strip_model_->CloseAllTabs();
}

// Tab discarding is tested here rather than in TabLifecycleUnitTest because
// collaboration from the TabLifecycleUnitSource is required to replace the
// WebContents in the TabLifecycleUnit.

TEST_F(TabLifecycleUnitSourceTest, Discard) {
  const base::TimeTicks kDummyLastActiveTime = base::TimeTicks() + kShortDelay;

  LifecycleUnit* background_lifecycle_unit = nullptr;
  LifecycleUnit* foreground_lifecycle_unit = nullptr;
  CreateTwoTabs(true /* focus_tab_strip */, &background_lifecycle_unit,
                &foreground_lifecycle_unit);
  content::WebContents* initial_web_contents =
      tab_strip_model_->GetWebContentsAt(0);
  initial_web_contents->SetLastActiveTime(kDummyLastActiveTime);

  // Discard the tab.
  EXPECT_EQ(LifecycleUnit::State::LOADED,
            background_lifecycle_unit->GetState());
  EXPECT_CALL(tab_observer_, OnDiscardedStateChange(testing::_, true));
  background_lifecycle_unit->Discard(DiscardReason::kProactive);
  testing::Mock::VerifyAndClear(&tab_observer_);

  // Expect the tab to be discarded and the last active time to be preserved.
  EXPECT_EQ(LifecycleUnit::State::DISCARDED,
            background_lifecycle_unit->GetState());
  EXPECT_NE(initial_web_contents, tab_strip_model_->GetWebContentsAt(0));
  EXPECT_FALSE(
      tab_strip_model_->GetWebContentsAt(0)->GetController().GetPendingEntry());
  EXPECT_EQ(kDummyLastActiveTime,
            tab_strip_model_->GetWebContentsAt(0)->GetLastActiveTime());

  source_.SetFocusedTabStripModelForTesting(nullptr);
}

TEST_F(TabLifecycleUnitSourceTest, DiscardAndActivate) {
  LifecycleUnit* background_lifecycle_unit = nullptr;
  LifecycleUnit* foreground_lifecycle_unit = nullptr;
  CreateTwoTabs(true /* focus_tab_strip */, &background_lifecycle_unit,
                &foreground_lifecycle_unit);
  content::WebContents* initial_web_contents =
      tab_strip_model_->GetWebContentsAt(0);

  // Discard the tab.
  EXPECT_EQ(LifecycleUnit::State::LOADED,
            background_lifecycle_unit->GetState());
  EXPECT_CALL(tab_observer_, OnDiscardedStateChange(testing::_, true));
  background_lifecycle_unit->Discard(DiscardReason::kProactive);
  testing::Mock::VerifyAndClear(&tab_observer_);
  EXPECT_EQ(LifecycleUnit::State::DISCARDED,
            background_lifecycle_unit->GetState());
  EXPECT_NE(initial_web_contents, tab_strip_model_->GetWebContentsAt(0));
  EXPECT_FALSE(
      tab_strip_model_->GetWebContentsAt(0)->GetController().GetPendingEntry());

  // Focus the tab. Expect the state to be LOADED.
  EXPECT_CALL(tab_observer_, OnDiscardedStateChange(testing::_, false));
  tab_strip_model_->ActivateTabAt(0, true);
  testing::Mock::VerifyAndClear(&tab_observer_);
  EXPECT_EQ(LifecycleUnit::State::LOADED,
            background_lifecycle_unit->GetState());
  EXPECT_TRUE(
      tab_strip_model_->GetWebContentsAt(0)->GetController().GetPendingEntry());
}

TEST_F(TabLifecycleUnitSourceTest, DiscardAndExplicitlyReload) {
  LifecycleUnit* background_lifecycle_unit = nullptr;
  LifecycleUnit* foreground_lifecycle_unit = nullptr;
  CreateTwoTabs(true /* focus_tab_strip */, &background_lifecycle_unit,
                &foreground_lifecycle_unit);
  content::WebContents* initial_web_contents =
      tab_strip_model_->GetWebContentsAt(0);

  // Discard the tab.
  EXPECT_EQ(LifecycleUnit::State::LOADED,
            background_lifecycle_unit->GetState());
  EXPECT_CALL(tab_observer_, OnDiscardedStateChange(testing::_, true));
  background_lifecycle_unit->Discard(DiscardReason::kProactive);
  testing::Mock::VerifyAndClear(&tab_observer_);
  EXPECT_EQ(LifecycleUnit::State::DISCARDED,
            background_lifecycle_unit->GetState());
  EXPECT_NE(initial_web_contents, tab_strip_model_->GetWebContentsAt(0));
  EXPECT_FALSE(
      tab_strip_model_->GetWebContentsAt(0)->GetController().GetPendingEntry());

  // Explicitly reload the tab. Expect the state to be LOADED.
  EXPECT_CALL(tab_observer_, OnDiscardedStateChange(testing::_, false));
  tab_strip_model_->GetWebContentsAt(0)->GetController().Reload(
      content::ReloadType::NORMAL, false);
  testing::Mock::VerifyAndClear(&tab_observer_);
  EXPECT_EQ(LifecycleUnit::State::LOADED,
            background_lifecycle_unit->GetState());
  EXPECT_TRUE(
      tab_strip_model_->GetWebContentsAt(0)->GetController().GetPendingEntry());
}

TEST_F(TabLifecycleUnitSourceTest, CanOnlyDiscardOnce) {
  LifecycleUnit* background_lifecycle_unit = nullptr;
  LifecycleUnit* foreground_lifecycle_unit = nullptr;
  CreateTwoTabs(true /* focus_tab_strip */, &background_lifecycle_unit,
                &foreground_lifecycle_unit);
  content::WebContents* initial_web_contents =
      tab_strip_model_->GetWebContentsAt(0);
  test_clock_.Advance(kTabFocusedProtectionTime);

  // It should be possible to discard the background tab.
  EXPECT_TRUE(background_lifecycle_unit->CanDiscard(DiscardReason::kExternal));
  EXPECT_TRUE(background_lifecycle_unit->CanDiscard(DiscardReason::kProactive));
  EXPECT_TRUE(background_lifecycle_unit->CanDiscard(DiscardReason::kUrgent));

  // Discard the tab.
  EXPECT_EQ(LifecycleUnit::State::LOADED,
            background_lifecycle_unit->GetState());
  EXPECT_CALL(tab_observer_, OnDiscardedStateChange(testing::_, true));
  background_lifecycle_unit->Discard(DiscardReason::kProactive);
  testing::Mock::VerifyAndClear(&tab_observer_);
  EXPECT_EQ(LifecycleUnit::State::DISCARDED,
            background_lifecycle_unit->GetState());
  EXPECT_NE(initial_web_contents, tab_strip_model_->GetWebContentsAt(0));
  EXPECT_FALSE(
      tab_strip_model_->GetWebContentsAt(0)->GetController().GetPendingEntry());

  // Explicitly reload the tab. Expect the state to be LOADED.
  EXPECT_CALL(tab_observer_, OnDiscardedStateChange(testing::_, false));
  tab_strip_model_->GetWebContentsAt(0)->GetController().Reload(
      content::ReloadType::NORMAL, false);
  testing::Mock::VerifyAndClear(&tab_observer_);
  EXPECT_EQ(LifecycleUnit::State::LOADED,
            background_lifecycle_unit->GetState());
  EXPECT_TRUE(
      tab_strip_model_->GetWebContentsAt(0)->GetController().GetPendingEntry());

  // It shouldn't be possible to discard the background tab again, except for an
  // urgent discard on ChromeOS.
  EXPECT_FALSE(background_lifecycle_unit->CanDiscard(DiscardReason::kExternal));
  EXPECT_FALSE(
      background_lifecycle_unit->CanDiscard(DiscardReason::kProactive));
#if defined(OS_CHROMEOS)
  EXPECT_TRUE(background_lifecycle_unit->CanDiscard(DiscardReason::kUrgent));
#else
  EXPECT_FALSE(background_lifecycle_unit->CanDiscard(DiscardReason::kUrgent));
#endif
}

}  // namespace resource_coordinator
