// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/tab_strip.h"

#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/views/tabs/fake_base_tab_strip_controller.h"
#include "chrome/browser/ui/views/tabs/tab.h"
#include "chrome/browser/ui/views/tabs/tab_renderer_data.h"
#include "chrome/browser/ui/views/tabs/tab_strip_controller.h"
#include "chrome/browser/ui/views/tabs/tab_strip_impl.h"
#include "chrome/browser/ui/views/tabs/tab_strip_observer.h"
#include "chrome/test/base/testing_profile.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/material_design/material_design_controller.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/path.h"
#include "ui/gfx/skia_util.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/view.h"
#include "ui/views/view_targeter.h"
#include "ui/views/widget/widget.h"

namespace {

// Walks up the views hierarchy until it finds a tab view. It returns the
// found tab view, on NULL if none is found.
views::View* FindTabView(views::View* view) {
  views::View* current = view;
  while (current && strcmp(current->GetClassName(), Tab::kViewClassName)) {
    current = current->parent();
  }
  return current;
}

}  // namespace

class TestTabStripObserver : public TabStripObserver {
 public:
  explicit TestTabStripObserver(TabStrip* tab_strip) : tab_strip_(tab_strip) {
    tab_strip_->AddObserver(this);
  }

  ~TestTabStripObserver() override {
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
  void TabStripAddedTabAt(TabStrip* tab_strip, int index) override {
    last_tab_added_ = index;
  }

  void TabStripMovedTab(TabStrip* tab_strip,
                        int from_index,
                        int to_index) override {
    last_tab_moved_from_ = from_index;
    last_tab_moved_to_ = to_index;
  }

  void TabStripRemovedTabAt(TabStrip* tab_strip, int index) override {
    last_tab_removed_ = index;
  }

  void TabStripDeleted(TabStrip* tab_strip) override {
    tabstrip_deleted_ = true;
    tab_strip_ = nullptr;
  }

  TabStrip* tab_strip_;
  int last_tab_added_ = -1;
  int last_tab_removed_ = -1;
  int last_tab_moved_from_ = -1;
  int last_tab_moved_to_ = -1;
  bool tabstrip_deleted_ = false;

  DISALLOW_COPY_AND_ASSIGN(TestTabStripObserver);
};

class TabStripTest : public views::ViewsTestBase {
 public:
  TabStripTest() {}

  ~TabStripTest() override {}

  void SetUp() override {
    views::ViewsTestBase::SetUp();

    controller_ = new FakeBaseTabStripController;
    tab_strip_ =
        new TabStripImpl(std::unique_ptr<TabStripController>(controller_));
    controller_->set_tab_strip(tab_strip_);
    // Do this to force TabStrip to create the buttons.
    parent_.AddChildView(tab_strip_);
    parent_.set_owned_by_client();

    widget_.reset(new views::Widget);
    views::Widget::InitParams init_params =
        CreateParams(views::Widget::InitParams::TYPE_POPUP);
    init_params.ownership =
        views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
    init_params.bounds = gfx::Rect(0, 0, 200, 200);
    widget_->Init(init_params);
    widget_->SetContentsView(&parent_);
  }

  void TearDown() override {
    widget_.reset();
    views::ViewsTestBase::TearDown();
  }

 protected:
  bool IsShowingAttentionIndicator(int model_index) {
    return tab_strip_->tab_at(model_index)->current_attention_types_ > 0;
  }

  // Checks whether |tab| contains |point_in_tabstrip_coords|, where the point
  // is in |tab_strip_| coordinates.
  bool IsPointInTab(Tab* tab, const gfx::Point& point_in_tabstrip_coords) {
    gfx::Point point_in_tab_coords(point_in_tabstrip_coords);
    views::View::ConvertPointToTarget(tab_strip_, tab, &point_in_tab_coords);
    return tab->HitTestPoint(point_in_tab_coords);
  }

  void DoLayout() { tab_strip_->DoLayout(); }

  // Owned by TabStrip.
  FakeBaseTabStripController* controller_ = nullptr;
  // Owns |tab_strip_|.
  views::View parent_;
  TabStripImpl* tab_strip_ = nullptr;
  std::unique_ptr<views::Widget> widget_;

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
  std::unique_ptr<TabStripImpl> tab_strip(
      new TabStripImpl(std::unique_ptr<TabStripController>(controller)));
  controller->set_tab_strip(tab_strip.get());
  TestTabStripObserver observer(tab_strip.get());
  tab_strip.reset();
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
  controller_->AddTab(0, false);
  controller_->AddTab(1, false);
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

TEST_F(TabStripTest, VisibilityInOverflow) {
  tab_strip_->SetBounds(0, 0, 200, 20);

  // The first tab added to a reasonable-width strip should be visible.  If we
  // add enough additional tabs, eventually one should be invisible due to
  // overflow.
  int invisible_tab_index = 0;
  for (; invisible_tab_index < 100; ++invisible_tab_index) {
    controller_->AddTab(invisible_tab_index, false);
    if (!tab_strip_->tab_at(invisible_tab_index)->visible())
      break;
  }
  EXPECT_GT(invisible_tab_index, 0);
  EXPECT_LT(invisible_tab_index, 100);

  // The tabs before the invisible tab should still be visible.
  for (int i = 0; i < invisible_tab_index; ++i)
    EXPECT_TRUE(tab_strip_->tab_at(i)->visible());

  // Enlarging the strip should result in the last tab becoming visible.
  tab_strip_->SetBounds(0, 0, 400, 20);
  EXPECT_TRUE(tab_strip_->tab_at(invisible_tab_index)->visible());

  // Shrinking it again should re-hide the last tab.
  tab_strip_->SetBounds(0, 0, 200, 20);
  EXPECT_FALSE(tab_strip_->tab_at(invisible_tab_index)->visible());

  // Shrinking it still more should make more tabs invisible, though not all.
  // All the invisible tabs should be at the end of the strip.
  tab_strip_->SetBounds(0, 0, 100, 20);
  int i = 0;
  for (; i < invisible_tab_index; ++i) {
    if (!tab_strip_->tab_at(i)->visible())
      break;
  }
  ASSERT_GT(i, 0);
  EXPECT_LT(i, invisible_tab_index);
  invisible_tab_index = i;
  for (int i = invisible_tab_index + 1; i < tab_strip_->tab_count(); ++i)
    EXPECT_FALSE(tab_strip_->tab_at(i)->visible());

  // When we're already in overflow, adding tabs at the beginning or end of
  // the strip should not change how many tabs are visible.
  controller_->AddTab(tab_strip_->tab_count(), false);
  EXPECT_TRUE(tab_strip_->tab_at(invisible_tab_index - 1)->visible());
  EXPECT_FALSE(tab_strip_->tab_at(invisible_tab_index)->visible());
  controller_->AddTab(0, false);
  EXPECT_TRUE(tab_strip_->tab_at(invisible_tab_index - 1)->visible());
  EXPECT_FALSE(tab_strip_->tab_at(invisible_tab_index)->visible());

  // If we remove enough tabs, all the tabs should be visible.
  for (int i = tab_strip_->tab_count() - 1; i >= invisible_tab_index; --i)
    controller_->RemoveTab(i);
  EXPECT_TRUE(tab_strip_->tab_at(tab_strip_->tab_count() - 1)->visible());
}

// Creates a tab strip in stacked layout mode and verifies that as we move
// across the strip at the top, middle, and bottom, events will target each tab
// in order.
TEST_F(TabStripTest, TabForEventWhenStacked) {
  tab_strip_->SetBounds(0, 0, 200, GetLayoutConstant(TAB_HEIGHT));

  controller_->AddTab(0, false);
  controller_->AddTab(1, true);
  controller_->AddTab(2, false);
  controller_->AddTab(3, false);
  ASSERT_EQ(4, tab_strip_->tab_count());

  // Switch to stacked layout mode and force a layout to ensure tabs stack.
  tab_strip_->SetStackedLayout(true);
  tab_strip_->DoLayout();

  gfx::Point p;
  for (int y : {0, tab_strip_->height() / 2, tab_strip_->height() - 1}) {
    p.set_y(y);
    int previous_tab = -1;
    for (int x = 0; x < tab_strip_->width(); ++x) {
      p.set_x(x);
      int tab = tab_strip_->GetModelIndexOfTab(tab_strip_->FindTabForEvent(p));
      if (tab == previous_tab)
        continue;
      if ((tab != -1) || (previous_tab != tab_strip_->tab_count() - 1))
        EXPECT_EQ(previous_tab + 1, tab) << "p = " << p.ToString();
      previous_tab = tab;
    }
  }
}

// Tests that the tab close buttons of non-active tabs are hidden when
// the tabstrip is in stacked tab mode.
TEST_F(TabStripTest, TabCloseButtonVisibilityWhenStacked) {
  tab_strip_->SetBounds(0, 0, 300, 20);
  controller_->AddTab(0, false);
  controller_->AddTab(1, true);
  controller_->AddTab(2, false);
  ASSERT_EQ(3, tab_strip_->tab_count());

  Tab* tab0 = tab_strip_->tab_at(0);
  Tab* tab1 = tab_strip_->tab_at(1);
  ASSERT_TRUE(tab1->IsActive());
  Tab* tab2 = tab_strip_->tab_at(2);

  // Ensure that all tab close buttons are initially visible.
  EXPECT_TRUE(tab0->showing_close_button_);
  EXPECT_TRUE(tab1->showing_close_button_);
  EXPECT_TRUE(tab2->showing_close_button_);

  // Enter stacked layout mode and verify this sets |touch_layout_|.
  ASSERT_FALSE(tab_strip_->touch_layout_.get());
  tab_strip_->SetStackedLayout(true);
  ASSERT_TRUE(tab_strip_->touch_layout_.get());

  // Only the close button of the active tab should be visible in stacked
  // layout mode.
  EXPECT_FALSE(tab0->showing_close_button_);
  EXPECT_TRUE(tab1->showing_close_button_);
  EXPECT_FALSE(tab2->showing_close_button_);

  // An inactive tab added to the tabstrip should not show
  // its tab close button.
  controller_->AddTab(3, false);
  Tab* tab3 = tab_strip_->tab_at(3);
  EXPECT_FALSE(tab0->showing_close_button_);
  EXPECT_TRUE(tab1->showing_close_button_);
  EXPECT_FALSE(tab2->showing_close_button_);
  EXPECT_FALSE(tab3->showing_close_button_);

  // After switching tabs, the previously-active tab should have its
  // tab close button hidden and the newly-active tab should show
  // its tab close button.
  tab_strip_->SelectTab(tab2);
  ASSERT_FALSE(tab1->IsActive());
  ASSERT_TRUE(tab2->IsActive());
  EXPECT_FALSE(tab0->showing_close_button_);
  EXPECT_FALSE(tab1->showing_close_button_);
  EXPECT_TRUE(tab2->showing_close_button_);
  EXPECT_FALSE(tab3->showing_close_button_);

  // After closing the active tab, the tab which becomes active should
  // show its tab close button.
  tab_strip_->CloseTab(tab1, CLOSE_TAB_FROM_TOUCH);
  tab1 = nullptr;
  ASSERT_TRUE(tab2->IsActive());
  EXPECT_FALSE(tab0->showing_close_button_);
  EXPECT_TRUE(tab2->showing_close_button_);
  EXPECT_FALSE(tab3->showing_close_button_);

  // All tab close buttons should be shown when disengaging stacked tab mode.
  tab_strip_->SetStackedLayout(false);
  ASSERT_FALSE(tab_strip_->touch_layout_.get());
  EXPECT_TRUE(tab0->showing_close_button_);
  EXPECT_TRUE(tab2->showing_close_button_);
  EXPECT_TRUE(tab3->showing_close_button_);
}

TEST_F(TabStripTest, GetEventHandlerForOverlappingArea) {
  tab_strip_->SetBounds(0, 0, 1000, 20);

  controller_->AddTab(0, false);
  controller_->AddTab(1, true);
  controller_->AddTab(2, false);
  controller_->AddTab(3, false);
  ASSERT_EQ(4, tab_strip_->tab_count());

  // Verify that the active tab will be a tooltip handler for points that hit
  // it.
  Tab* left_tab = tab_strip_->tab_at(0);
  left_tab->SetBoundsRect(gfx::Rect(gfx::Point(0, 0), gfx::Size(200, 20)));

  Tab* active_tab = tab_strip_->tab_at(1);
  active_tab->SetBoundsRect(gfx::Rect(gfx::Point(150, 0), gfx::Size(200, 20)));
  ASSERT_TRUE(active_tab->IsActive());

  Tab* right_tab = tab_strip_->tab_at(2);
  right_tab->SetBoundsRect(gfx::Rect(gfx::Point(300, 0), gfx::Size(200, 20)));

  Tab* most_right_tab = tab_strip_->tab_at(3);
  most_right_tab->SetBoundsRect(
      gfx::Rect(gfx::Point(450, 0), gfx::Size(200, 20)));

  // Test that active tabs gets events from area in which it overlaps with its
  // left neighbour.
  gfx::Point left_overlap(
      (active_tab->x() + left_tab->bounds().right() + 1) / 2,
      active_tab->bounds().bottom() - 1);

  // Sanity check that the point is in both active and left tab.
  ASSERT_TRUE(IsPointInTab(active_tab, left_overlap));
  ASSERT_TRUE(IsPointInTab(left_tab, left_overlap));

  EXPECT_EQ(active_tab,
            FindTabView(tab_strip_->GetEventHandlerForPoint(left_overlap)));

  // Test that active tabs gets events from area in which it overlaps with its
  // right neighbour.
  gfx::Point right_overlap((active_tab->bounds().right() + right_tab->x()) / 2,
                           active_tab->bounds().bottom() - 1);

  // Sanity check that the point is in both active and right tab.
  ASSERT_TRUE(IsPointInTab(active_tab, right_overlap));
  ASSERT_TRUE(IsPointInTab(right_tab, right_overlap));

  EXPECT_EQ(active_tab,
            FindTabView(tab_strip_->GetEventHandlerForPoint(right_overlap)));

  // Test that if neither of tabs is active, the left one is selected.
  gfx::Point unactive_overlap(
      (right_tab->x() + most_right_tab->bounds().right() + 1) / 2,
      right_tab->bounds().bottom() - 1);

  // Sanity check that the point is in both active and left tab.
  ASSERT_TRUE(IsPointInTab(right_tab, unactive_overlap));
  ASSERT_TRUE(IsPointInTab(most_right_tab, unactive_overlap));

  EXPECT_EQ(right_tab,
            FindTabView(tab_strip_->GetEventHandlerForPoint(unactive_overlap)));
}

TEST_F(TabStripTest, GetTooltipHandler) {
  tab_strip_->SetBounds(0, 0, 1000, 20);

  controller_->AddTab(0, false);
  controller_->AddTab(1, true);
  controller_->AddTab(2, false);
  controller_->AddTab(3, false);
  ASSERT_EQ(4, tab_strip_->tab_count());

  // Verify that the active tab will be a tooltip handler for points that hit
  // it.
  Tab* left_tab = tab_strip_->tab_at(0);
  left_tab->SetBoundsRect(gfx::Rect(gfx::Point(0, 0), gfx::Size(200, 20)));

  Tab* active_tab = tab_strip_->tab_at(1);
  active_tab->SetBoundsRect(gfx::Rect(gfx::Point(150, 0), gfx::Size(200, 20)));
  ASSERT_TRUE(active_tab->IsActive());

  Tab* right_tab = tab_strip_->tab_at(2);
  right_tab->SetBoundsRect(gfx::Rect(gfx::Point(300, 0), gfx::Size(200, 20)));

  Tab* most_right_tab = tab_strip_->tab_at(3);
  most_right_tab->SetBoundsRect(
      gfx::Rect(gfx::Point(450, 0), gfx::Size(200, 20)));

  // Test that active_tab handles tooltips from area in which it overlaps with
  // its left neighbour.
  gfx::Point left_overlap(
      (active_tab->x() + left_tab->bounds().right() + 1) / 2,
      active_tab->bounds().bottom() - 1);

  // Sanity check that the point is in both active and left tab.
  ASSERT_TRUE(IsPointInTab(active_tab, left_overlap));
  ASSERT_TRUE(IsPointInTab(left_tab, left_overlap));

  EXPECT_EQ(active_tab,
            FindTabView(tab_strip_->GetTooltipHandlerForPoint(left_overlap)));

  // Test that active_tab handles tooltips from area in which it overlaps with
  // its right neighbour.
  gfx::Point right_overlap((active_tab->bounds().right() + right_tab->x()) / 2,
                           active_tab->bounds().bottom() - 1);

  // Sanity check that the point is in both active and right tab.
  ASSERT_TRUE(IsPointInTab(active_tab, right_overlap));
  ASSERT_TRUE(IsPointInTab(right_tab, right_overlap));

  EXPECT_EQ(active_tab,
            FindTabView(tab_strip_->GetTooltipHandlerForPoint(right_overlap)));

  // Test that if neither of tabs is active, the left one is selected.
  gfx::Point unactive_overlap(
      (right_tab->x() + most_right_tab->bounds().right() + 1) / 2,
      right_tab->bounds().bottom() - 1);

  // Sanity check that the point is in both active and left tab.
  ASSERT_TRUE(IsPointInTab(right_tab, unactive_overlap));
  ASSERT_TRUE(IsPointInTab(most_right_tab, unactive_overlap));

  EXPECT_EQ(
      right_tab,
      FindTabView(tab_strip_->GetTooltipHandlerForPoint(unactive_overlap)));

  // Confirm that tab strip doe not return tooltip handler for points that
  // don't hit it.
  EXPECT_FALSE(tab_strip_->GetTooltipHandlerForPoint(gfx::Point(-1, 2)));
}

TEST_F(TabStripTest, NewTabButtonStaysVisible) {
  const int kTabStripWidth = 500;
  tab_strip_->SetBounds(0, 0, kTabStripWidth, 20);

  for (int i = 0; i < 100; ++i)
    controller_->AddTab(i, (i == 0));

  DoLayout();

  EXPECT_LE(tab_strip_->GetNewTabButtonBounds().right(), kTabStripWidth);
}

TEST_F(TabStripTest, AttentionIndicatorHidesOnSelect) {
  for (int i = 0; i < 2; ++i)
    controller_->AddTab(i, (i == 0));

  // Two tabs, both pinned.
  TabRendererData pinned_data;
  pinned_data.pinned = true;
  tab_strip_->SetTabData(0, pinned_data);
  tab_strip_->SetTabData(1, pinned_data);

  EXPECT_FALSE(IsShowingAttentionIndicator(0));
  EXPECT_FALSE(IsShowingAttentionIndicator(1));

  // Change the title of the second tab (first tab is selected).
  tab_strip_->TabTitleChangedNotLoading(1);
  // Indicator should be shown.
  EXPECT_TRUE(IsShowingAttentionIndicator(1));
  // Select the second tab.
  controller_->SelectTab(1);
  // Indicator should hide.
  EXPECT_FALSE(IsShowingAttentionIndicator(1));
}
