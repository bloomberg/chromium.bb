// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/ash/tab_scrubber.h"

#include "base/command_line.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/message_loop_proxy.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/tabs/tab.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/test_utils.h"
#include "ui/aura/test/event_generator.h"
#include "ui/aura/window.h"
#include "ui/base/events/event_utils.h"

namespace {

class TabScrubberTest : public InProcessBrowserTest,
                        public TabStripModelObserver {
 public:
  TabScrubberTest()
      : tab_strip_(NULL),
        event_generator_(NULL),
        target_index_(-1) {
  }

  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    command_line->AppendSwitch(switches::kNaturalScrollDefault);
    command_line->AppendSwitch(switches::kAshEnableTabScrubbing);
  }

  virtual void SetUpOnMainThread() OVERRIDE {
    aura::Window* window = browser()->window()->GetNativeWindow();
    BrowserView* browser_view =
        BrowserView::GetBrowserViewForNativeWindow(window);
    tab_strip_ = browser_view->tabstrip();
    browser()->tab_strip_model()->AddObserver(this);
    aura::RootWindow* root = window->GetRootWindow();
    event_generator_.reset(new aura::test::EventGenerator(root, window));
    event_generator_->set_async(true);
    TabScrubber::GetInstance()->set_activation_delay(
        base::TimeDelta::FromMilliseconds(0));
  }

  virtual void CleanUpOnMainThread() {
    browser()->tab_strip_model()->RemoveObserver(this);
  }

  int GetStartX(int index, TabScrubber::Direction direction) {
    return TabScrubber::GetStartPoint(tab_strip_, index, direction).x();
  }

  int GetTabCenter(int index) {
    return tab_strip_->tab_at(index)->bounds().CenterPoint().x();
  }

  ui::ScrollEvent GetScrubEvent(int index) {
    int active_index = browser()->tab_strip_model()->active_index();
    TabScrubber::Direction direction = index < active_index ?
        TabScrubber::LEFT : TabScrubber::RIGHT;
    int offset = GetTabCenter(index) - GetStartX(active_index, direction);
    return ui::ScrollEvent(ui::ET_SCROLL,
                           gfx::Point(0, 0),
                           ui::EventTimeForNow(),
                           0,
                           offset,
                           0,
                           3);
  }

  enum ScrubType {
    EACH_TAB,
    SKIP_TABS,
    REPEAT_TABS,
  };

  void Scrub(int index, ScrubType scrub_type) {
    activation_order_.clear();
    int active_index = browser()->tab_strip_model()->active_index();
    ASSERT_NE(index, active_index);
    ASSERT_TRUE(scrub_type != SKIP_TABS || ((index - active_index) % 2) == 0);
    TabScrubber::Direction direction;
    int increment;
    if (index < active_index) {
      direction = TabScrubber::LEFT;
      increment = -1;
    } else {
      direction = TabScrubber::RIGHT;
      increment = 1;
    }
    if (scrub_type == SKIP_TABS)
      increment *= 2;
    int last = GetStartX(active_index, direction);
    std::vector<gfx::Point> offsets;
    for (int i = active_index + increment; i != (index + increment);
        i += increment) {
      int tab_center = GetTabCenter(i);
      offsets.push_back(gfx::Point(tab_center - last, 0));
      last = GetStartX(i, direction);
      if (scrub_type == REPEAT_TABS) {
        offsets.push_back(gfx::Point(increment, 0));
        last += increment;
      }
    }
    event_generator_->ScrollSequence(gfx::Point(0, 0),
                      base::TimeDelta::FromMilliseconds(100),
                      offsets,
                      3);
  }

  void AddTabs(int num_tabs) {
    for (int i = 0; i < num_tabs; ++i)
      AddBlankTabAndShow(browser());
    ASSERT_EQ(num_tabs + 1, browser()->tab_strip_model()->count());
    ASSERT_EQ(num_tabs, browser()->tab_strip_model()->active_index());
    tab_strip_->StopAnimating(true);
    ASSERT_FALSE(tab_strip_->IsAnimating());
  }

  void RunUntilTabActive(int target) {
    base::RunLoop run_loop;
    quit_closure_ = content::GetQuitTaskForRunLoop(&run_loop);
    target_index_ = target;
    content::RunThisRunLoop(&run_loop);
    target_index_ = -1;
  }

  // TabStripModelObserver overrides.
  virtual void TabInsertedAt(content::WebContents* contents,
                             int index,
                             bool foreground) {}
  virtual void TabClosingAt(TabStripModel* tab_strip_model,
                            content::WebContents* contents,
                            int index) {}
  virtual void TabDetachedAt(content::WebContents* contents, int index) {}
  virtual void TabDeactivated(content::WebContents* contents) {}
  virtual void ActiveTabChanged(content::WebContents* old_contents,
                                content::WebContents* new_contents,
                                int index,
                                bool user_gesture) {
    activation_order_.push_back(index);
    if (index == target_index_)
      quit_closure_.Run();
  }

  virtual void TabSelectionChanged(TabStripModel* tab_strip_model,
                                   const ui::ListSelectionModel& old_model) {}
  virtual void TabMoved(content::WebContents* contents,
                        int from_index,
                        int to_index) {}
  virtual void TabChangedAt(content::WebContents* contents,
                            int index,
                            TabChangeType change_type) {}
  virtual void TabReplacedAt(TabStripModel* tab_strip_model,
                             content::WebContents* old_contents,
                             content::WebContents* new_contents,
                             int index) {}
  virtual void TabPinnedStateChanged(content::WebContents* contents,
                                     int index) {}
  virtual void TabMiniStateChanged(content::WebContents* contents, int index) {}
  virtual void TabBlockedStateChanged(content::WebContents* contents,
                                      int index) {}
  virtual void TabStripEmpty() {}
  virtual void TabStripModelDeleted() {}

  // Shortcut to tab_strip of browser().
  TabStrip* tab_strip_;
  // History of tab activation. Scrub() resets it.
  std::vector<int> activation_order_;
  scoped_ptr<aura::test::EventGenerator> event_generator_;

 private:
  base::Closure quit_closure_;
  int target_index_;

  DISALLOW_COPY_AND_ASSIGN(TabScrubberTest);
};

}  // namespace

// Swipe a single tab in each direction.
IN_PROC_BROWSER_TEST_F(TabScrubberTest, Single) {
  AddTabs(1);

  Scrub(0, EACH_TAB);
  RunUntilTabActive(0);
  EXPECT_EQ(1U, activation_order_.size());
  EXPECT_EQ(0, activation_order_[0]);
  EXPECT_EQ(0, browser()->tab_strip_model()->active_index());

  Scrub(1, EACH_TAB);
  RunUntilTabActive(1);
  EXPECT_EQ(1U, activation_order_.size());
  EXPECT_EQ(1, activation_order_[0]);
  EXPECT_EQ(1, browser()->tab_strip_model()->active_index());
}

// Swipe 4 tabs in each direction. Each of the tabs should become active.
IN_PROC_BROWSER_TEST_F(TabScrubberTest, Multi) {
  AddTabs(4);

  Scrub(0, EACH_TAB);
  RunUntilTabActive(0);
  ASSERT_EQ(4U, activation_order_.size());
  EXPECT_EQ(3, activation_order_[0]);
  EXPECT_EQ(2, activation_order_[1]);
  EXPECT_EQ(1, activation_order_[2]);
  EXPECT_EQ(0, activation_order_[3]);
  EXPECT_EQ(0, browser()->tab_strip_model()->active_index());

  Scrub(4, EACH_TAB);
  RunUntilTabActive(4);
  ASSERT_EQ(4U, activation_order_.size());
  EXPECT_EQ(1, activation_order_[0]);
  EXPECT_EQ(2, activation_order_[1]);
  EXPECT_EQ(3, activation_order_[2]);
  EXPECT_EQ(4, activation_order_[3]);
  EXPECT_EQ(4, browser()->tab_strip_model()->active_index());
}

// Swipe 4 tabs in each direction with an extra swipe within each. The same
// 4 tabs should become active.
IN_PROC_BROWSER_TEST_F(TabScrubberTest, Repeated) {
  AddTabs(4);

  Scrub(0, REPEAT_TABS);
  RunUntilTabActive(0);
  ASSERT_EQ(4U, activation_order_.size());
  EXPECT_EQ(3, activation_order_[0]);
  EXPECT_EQ(2, activation_order_[1]);
  EXPECT_EQ(1, activation_order_[2]);
  EXPECT_EQ(0, activation_order_[3]);
  EXPECT_EQ(0, browser()->tab_strip_model()->active_index());

  Scrub(4, REPEAT_TABS);
  RunUntilTabActive(4);
  ASSERT_EQ(4U, activation_order_.size());
  EXPECT_EQ(1, activation_order_[0]);
  EXPECT_EQ(2, activation_order_[1]);
  EXPECT_EQ(3, activation_order_[2]);
  EXPECT_EQ(4, activation_order_[3]);
  EXPECT_EQ(4, browser()->tab_strip_model()->active_index());
}

// Confirm that we get the last tab made active when we skip tabs.
// These tests have 5 total tabs. We will only received scroll events
// on tabs 0, 2 and 4.
IN_PROC_BROWSER_TEST_F(TabScrubberTest, Skipped) {
  AddTabs(4);

  Scrub(0, SKIP_TABS);
  RunUntilTabActive(0);
  EXPECT_EQ(2U, activation_order_.size());
  EXPECT_EQ(2, activation_order_[0]);
  EXPECT_EQ(0, activation_order_[1]);
  EXPECT_EQ(0, browser()->tab_strip_model()->active_index());

  Scrub(4, SKIP_TABS);
  RunUntilTabActive(4);
  EXPECT_EQ(2U, activation_order_.size());
  EXPECT_EQ(2, activation_order_[0]);
  EXPECT_EQ(4, activation_order_[1]);
  EXPECT_EQ(4, browser()->tab_strip_model()->active_index());
}

// Confirm that nothing happens when the swipe is small.
IN_PROC_BROWSER_TEST_F(TabScrubberTest, NoChange) {
  AddTabs(1);

  event_generator_->ScrollSequence(gfx::Point(0, 0),
                                   base::TimeDelta::FromMilliseconds(100),
                                   -1,
                                   0,
                                   1,
                                   3);
  EXPECT_EQ(1, browser()->tab_strip_model()->active_index());

  event_generator_->ScrollSequence(gfx::Point(0, 0),
                                   base::TimeDelta::FromMilliseconds(100),
                                   1,
                                   0,
                                   1,
                                   3);
  EXPECT_EQ(1, browser()->tab_strip_model()->active_index());
}

// Confirm that very large swipes go to the beginning and and of the tabstrip.
IN_PROC_BROWSER_TEST_F(TabScrubberTest, Bounds) {
  AddTabs(1);

  event_generator_->ScrollSequence(gfx::Point(0, 0),
                                   base::TimeDelta::FromMilliseconds(100),
                                   -10000,
                                   0,
                                   1,
                                   3);
  RunUntilTabActive(0);
  EXPECT_EQ(0, browser()->tab_strip_model()->active_index());

  event_generator_->ScrollSequence(gfx::Point(0, 0),
                                   base::TimeDelta::FromMilliseconds(100),
                                   10000,
                                   0,
                                   1,
                                   3);
  RunUntilTabActive(1);
  EXPECT_EQ(1, browser()->tab_strip_model()->active_index());
}

IN_PROC_BROWSER_TEST_F(TabScrubberTest, DeleteHighlighted) {
  AddTabs(1);

  event_generator_->set_async(false);
  ui::ScrollEvent scroll = GetScrubEvent(0);
  event_generator_->Dispatch(&scroll);
  EXPECT_TRUE(TabScrubber::GetInstance()->IsActivationPending());
  browser()->tab_strip_model()->CloseWebContentsAt(0,
                                                   TabStripModel::CLOSE_NONE);
  EXPECT_FALSE(TabScrubber::GetInstance()->IsActivationPending());
}

// Delete the currently highlighted tab. Make sure the TabScrubber is aware.
IN_PROC_BROWSER_TEST_F(TabScrubberTest, DeleteBeforeHighlighted) {
  AddTabs(2);

  event_generator_->set_async(false);
  ui::ScrollEvent scroll = GetScrubEvent(1);
  event_generator_->Dispatch(&scroll);
  EXPECT_TRUE(TabScrubber::GetInstance()->IsActivationPending());
  browser()->tab_strip_model()->CloseWebContentsAt(0,
                                                   TabStripModel::CLOSE_NONE);
  EXPECT_EQ(0, TabScrubber::GetInstance()->highlighted_tab());
}

// Move the currently highlighted tab and confirm it gets tracked.
IN_PROC_BROWSER_TEST_F(TabScrubberTest, MoveHighlighted) {
  AddTabs(1);

  event_generator_->set_async(false);
  ui::ScrollEvent scroll = GetScrubEvent(0);
  event_generator_->Dispatch(&scroll);
  EXPECT_TRUE(TabScrubber::GetInstance()->IsActivationPending());
  browser()->tab_strip_model()->ToggleSelectionAt(0);
  browser()->tab_strip_model()->ToggleSelectionAt(1);
  browser()->tab_strip_model()->MoveSelectedTabsTo(1);
  EXPECT_EQ(1, TabScrubber::GetInstance()->highlighted_tab());
}

// Move a tab to before  the highlighted one.
IN_PROC_BROWSER_TEST_F(TabScrubberTest, MoveBefore) {
  AddTabs(2);

  event_generator_->set_async(false);
  ui::ScrollEvent scroll = GetScrubEvent(1);
  event_generator_->Dispatch(&scroll);
  EXPECT_TRUE(TabScrubber::GetInstance()->IsActivationPending());
  browser()->tab_strip_model()->ToggleSelectionAt(0);
  browser()->tab_strip_model()->ToggleSelectionAt(2);
  browser()->tab_strip_model()->MoveSelectedTabsTo(2);
  EXPECT_EQ(0, TabScrubber::GetInstance()->highlighted_tab());
}

// Move a tab to after the highlighted one.
IN_PROC_BROWSER_TEST_F(TabScrubberTest, MoveAfter) {
  AddTabs(2);

  event_generator_->set_async(false);
  ui::ScrollEvent scroll = GetScrubEvent(1);
  event_generator_->Dispatch(&scroll);
  EXPECT_TRUE(TabScrubber::GetInstance()->IsActivationPending());
  browser()->tab_strip_model()->MoveSelectedTabsTo(0);
  EXPECT_EQ(2, TabScrubber::GetInstance()->highlighted_tab());
}

