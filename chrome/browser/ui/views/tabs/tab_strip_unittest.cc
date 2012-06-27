// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/tab_strip.h"

#include "base/message_loop.h"
#include "chrome/browser/ui/views/tabs/fake_base_tab_strip_controller.h"
#include "chrome/browser/ui/views/tabs/tab_strip_controller.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "testing/gtest/include/gtest/gtest.h"

class TabStripTest : public testing::Test {
 public:
  TabStripTest()
      : controller_(new FakeBaseTabStripController),
        tab_strip_(new TabStrip(controller_, false)) {
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
  scoped_ptr<BaseTab> tab(tab_strip_->CreateTabForDragging());
}

TEST_F(TabStripTest, AddTabAt) {
  tab_strip_->AddTabAt(0, TabRendererData(), false);
  ASSERT_EQ(1, tab_strip_->tab_count());
  Tab* tab = tab_strip_->tab_at(0);
  EXPECT_FALSE(tab == NULL);
}

// Verifies child views are deleted after an animation completes.
TEST_F(TabStripTest, RemoveTab) {
  controller_->AddTab(0);
  controller_->AddTab(1);
  const int child_view_count = tab_strip_->child_count();
  EXPECT_EQ(2, tab_strip_->tab_count());
  controller_->RemoveTab(0);
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
}
