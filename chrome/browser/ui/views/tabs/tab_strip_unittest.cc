// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/tab_strip.h"

#include "base/message_loop.h"
#include "chrome/browser/ui/views/tabs/fake_base_tab_strip_controller.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/browser/ui/views/tabs/tab_strip_controller.h"
#include "chrome/browser/ui/views/tabs/tab_strip_observer.h"
#include "chrome/test/base/testing_profile.h"
#include "testing/gtest/include/gtest/gtest.h"

class TestTabStripObserver : public TabStripObserver {
 public:
  explicit TestTabStripObserver(TabStrip* tab_strip)
      : tab_strip_(tab_strip),
        last_tab_added_(-1),
        last_tab_removed_(-1),
        last_tab_moved_from_(-1),
        last_tab_moved_to_(-1),
        tabstrip_deleted_(false) {
    tab_strip_->AddObserver(this);
  }

  virtual ~TestTabStripObserver() {
    if (tab_strip_)
      tab_strip_->RemoveObserver(this);
  }

  int last_tab_added() const { return last_tab_added_; }
  int last_tab_removed() const { return last_tab_removed_; }
  int last_tab_moved_from() const { return last_tab_moved_from_; }
  int last_tab_moved_to() const { return last_tab_moved_to_; }
  bool tabstrip_deleted() const { return tabstrip_deleted_; }

 private:
  // TabStripObserver overrides.
  virtual void TabStripAddedTabAt(TabStrip* tab_strip, int index) OVERRIDE {
    last_tab_added_ = index;
  }

  virtual void TabStripMovedTab(TabStrip* tab_strip,
                                int from_index,
                                int to_index) OVERRIDE {
    last_tab_moved_from_ = from_index;
    last_tab_moved_to_ = to_index;
  }

  virtual void TabStripRemovedTabAt(TabStrip* tab_strip, int index) OVERRIDE {
    last_tab_removed_ = index;
  }

  virtual void TabStripDeleted(TabStrip* tab_strip) OVERRIDE {
    tabstrip_deleted_ = true;
    tab_strip_ = NULL;
  }

  TabStrip* tab_strip_;
  int last_tab_added_;
  int last_tab_removed_;
  int last_tab_moved_from_;
  int last_tab_moved_to_;
  bool tabstrip_deleted_;

  DISALLOW_COPY_AND_ASSIGN(TestTabStripObserver);
};

class TabStripTest : public testing::Test {
 public:
  TabStripTest()
      : controller_(new FakeBaseTabStripController) {
    tab_strip_ = new TabStrip(controller_);
    controller_->set_tab_strip(tab_strip_);
    // Do this to force TabStrip to create the buttons.
    parent_.AddChildView(tab_strip_);
  }

 protected:
  MessageLoopForUI ui_loop_;
  // Owned by TabStrip.
  FakeBaseTabStripController* controller_;
  // Owns |tab_strip_|.
  views::View parent_;
  TabStrip* tab_strip_;

 private:
  DISALLOW_COPY_AND_ASSIGN(TabStripTest);
};

TEST_F(TabStripTest, GetModelCount) {
  EXPECT_EQ(0, tab_strip_->GetModelCount());
}

TEST_F(TabStripTest, IsValidModelIndex) {
  EXPECT_FALSE(tab_strip_->IsValidModelIndex(0));
}

TEST_F(TabStripTest, tab_count) {
  EXPECT_EQ(0, tab_strip_->tab_count());
}

TEST_F(TabStripTest, CreateTabForDragging) {
  // Any result is good, as long as it doesn't crash.
  scoped_ptr<Tab> tab(tab_strip_->CreateTabForDragging());
}

TEST_F(TabStripTest, AddTabAt) {
  TestTabStripObserver observer(tab_strip_);
  tab_strip_->AddTabAt(0, TabRendererData(), false);
  ASSERT_EQ(1, tab_strip_->tab_count());
  EXPECT_EQ(0, observer.last_tab_added());
  Tab* tab = tab_strip_->tab_at(0);
  EXPECT_FALSE(tab == NULL);
}

// Confirms that TabStripObserver::TabStripDeleted() is sent.
TEST_F(TabStripTest, TabStripDeleted) {
  FakeBaseTabStripController* controller = new FakeBaseTabStripController;
  TabStrip* tab_strip = new TabStrip(controller);
  controller->set_tab_strip(tab_strip);
  TestTabStripObserver observer(tab_strip);
  delete tab_strip;
  EXPECT_TRUE(observer.tabstrip_deleted());
}

TEST_F(TabStripTest, MoveTab) {
  TestTabStripObserver observer(tab_strip_);
  tab_strip_->AddTabAt(0, TabRendererData(), false);
  tab_strip_->AddTabAt(1, TabRendererData(), false);
  tab_strip_->AddTabAt(2, TabRendererData(), false);
  ASSERT_EQ(3, tab_strip_->tab_count());
  EXPECT_EQ(2, observer.last_tab_added());
  Tab* tab = tab_strip_->tab_at(0);
  tab_strip_->MoveTab(0, 1, TabRendererData());
  EXPECT_EQ(0, observer.last_tab_moved_from());
  EXPECT_EQ(1, observer.last_tab_moved_to());
  EXPECT_EQ(tab, tab_strip_->tab_at(1));
}

// Verifies child views are deleted after an animation completes.
TEST_F(TabStripTest, RemoveTab) {
  TestTabStripObserver observer(tab_strip_);
  controller_->AddTab(0);
  controller_->AddTab(1);
  const int child_view_count = tab_strip_->child_count();
  EXPECT_EQ(2, tab_strip_->tab_count());
  controller_->RemoveTab(0);
  EXPECT_EQ(0, observer.last_tab_removed());
  // When removing a tab the tabcount should immediately decrement.
  EXPECT_EQ(1, tab_strip_->tab_count());
  // But the number of views should remain the same (it's animatining closed).
  EXPECT_EQ(child_view_count, tab_strip_->child_count());
  tab_strip_->SetBounds(0, 0, 200, 20);
  // Layout at a different size should force the animation to end and delete
  // the tab that was removed.
  tab_strip_->Layout();
  EXPECT_EQ(child_view_count - 1, tab_strip_->child_count());

  // Remove the last tab to make sure things are cleaned up correctly when
  // the TabStrip is destroyed and an animation is ongoing.
  controller_->RemoveTab(0);
  EXPECT_EQ(0, observer.last_tab_removed());
}

TEST_F(TabStripTest, ImmersiveMode) {
  // Immersive mode defaults to off.
  EXPECT_FALSE(tab_strip_->IsImmersiveStyle());

  // Tab strip defaults to normal tab height.
  int normal_height = Tab::GetMinimumUnselectedSize().height();
  EXPECT_EQ(normal_height, tab_strip_->GetPreferredSize().height());

  // Tab strip can toggle immersive mode.
  tab_strip_->SetImmersiveStyle(true);
  EXPECT_TRUE(tab_strip_->IsImmersiveStyle());

  // Now tabs have the immersive height.
  int immersive_height = Tab::GetImmersiveHeight();
  EXPECT_EQ(immersive_height, tab_strip_->GetPreferredSize().height());

  // Sanity-check immersive tabs are shorter than normal tabs.
  EXPECT_LT(immersive_height, normal_height);
}
