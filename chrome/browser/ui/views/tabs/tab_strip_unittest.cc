// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/tab_strip.h"

#include "base/message_loop/message_loop.h"
#include "chrome/browser/ui/views/tabs/fake_base_tab_strip_controller.h"
#include "chrome/browser/ui/views/tabs/tab.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/browser/ui/views/tabs/tab_strip_controller.h"
#include "chrome/browser/ui/views/tabs/tab_strip_observer.h"
#include "chrome/test/base/testing_profile.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/path.h"
#include "ui/gfx/rect_conversions.h"
#include "ui/gfx/skia_util.h"
#include "ui/views/view.h"

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
  // Returns the rectangular hit test region of |tab| in |tab|'s local
  // coordinate space.
  gfx::Rect GetTabHitTestMask(Tab* tab) {
    gfx::Path mask;
    tab->GetHitTestMask(views::View::HIT_TEST_SOURCE_TOUCH, &mask);
    return gfx::ToEnclosingRect((gfx::SkRectToRectF(mask.getBounds())));
  }

  // Returns the rectangular hit test region of the tab close button of
  // |tab| in |tab|'s coordinate space (including padding if |padding|
  // is true).
  gfx::Rect GetTabCloseHitTestMask(Tab* tab, bool padding) {
    gfx::RectF bounds_f = tab->close_button_->GetContentsBounds();
    if (padding)
      bounds_f = tab->close_button_->GetLocalBounds();
    views::View::ConvertRectToTarget(tab->close_button_, tab, &bounds_f);
    return gfx::ToEnclosingRect(bounds_f);
  }

  // Checks whether |tab| contains |point_in_tabstrip_coords|, where the point
  // is in |tab_strip_| coordinates.
  bool IsPointInTab(Tab* tab, const gfx::Point& point_in_tabstrip_coords) {
    gfx::Point point_in_tab_coords(point_in_tabstrip_coords);
    views::View::ConvertPointToTarget(tab_strip_, tab, &point_in_tab_coords);
    return tab->HitTestPoint(point_in_tab_coords);
  }

  base::MessageLoopForUI ui_loop_;
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

// Creates a tab strip in stacked layout mode and verifies the correctness
// of hit tests against the visible/occluded regions of a tab and
// visible/occluded tab close buttons.
TEST_F(TabStripTest, TabHitTestMaskWhenStacked) {
  tab_strip_->SetBounds(0, 0, 300, 20);

  controller_->AddTab(0, false);
  controller_->AddTab(1, true);
  controller_->AddTab(2, false);
  controller_->AddTab(3, false);
  ASSERT_EQ(4, tab_strip_->tab_count());

  Tab* left_tab = tab_strip_->tab_at(0);
  left_tab->SetBoundsRect(gfx::Rect(gfx::Point(0, 0), gfx::Size(200, 20)));

  Tab* active_tab = tab_strip_->tab_at(1);
  active_tab->SetBoundsRect(gfx::Rect(gfx::Point(150, 0), gfx::Size(200, 20)));
  ASSERT_TRUE(active_tab->IsActive());

  Tab* right_tab = tab_strip_->tab_at(2);
  right_tab->SetBoundsRect(gfx::Rect(gfx::Point(300, 0), gfx::Size(200, 20)));

  Tab* most_right_tab = tab_strip_->tab_at(3);
  most_right_tab->SetBoundsRect(gfx::Rect(gfx::Point(450, 0),
                                          gfx::Size(200, 20)));

  // Switch to stacked layout mode and force a layout to ensure tabs stack.
  tab_strip_->SetLayoutType(TAB_STRIP_LAYOUT_STACKED, false);
  tab_strip_->DoLayout();


  // Tests involving |left_tab|, which has part of its bounds and its tab
  // close button occluded by |active_tab|.

  // Bounds of the tab's hit test mask.
  gfx::Rect tab_bounds = GetTabHitTestMask(left_tab);
  EXPECT_EQ(gfx::Rect(6, 2, 61, 27).ToString(), tab_bounds.ToString());

  // Bounds of the tab close button (without padding) in the tab's
  // coordinate space.
  gfx::Rect contents_bounds = GetTabCloseHitTestMask(left_tab, false);
  // TODO(tdanderson): Uncomment this line once crbug.com/311609 is resolved.
  //EXPECT_EQ(gfx::Rect(84, 8, 18, 18).ToString(), contents_bounds.ToString());

  // Verify that the tab close button is occluded.
  EXPECT_FALSE(tab_bounds.Contains(contents_bounds));

  // Hit tests in the non-occuluded region of the tab.
  EXPECT_TRUE(left_tab->HitTestRect(gfx::Rect(6, 2, 2, 2)));
  EXPECT_TRUE(left_tab->HitTestRect(gfx::Rect(6, 2, 1, 1)));
  EXPECT_TRUE(left_tab->HitTestRect(gfx::Rect(30, 15, 1, 1)));
  EXPECT_TRUE(left_tab->HitTestRect(gfx::Rect(30, 15, 25, 35)));
  EXPECT_TRUE(left_tab->HitTestRect(gfx::Rect(-10, -5, 20, 30)));

  // Hit tests in the occluded region of the tab.
  EXPECT_FALSE(left_tab->HitTestRect(gfx::Rect(70, 15, 2, 2)));
  EXPECT_FALSE(left_tab->HitTestRect(gfx::Rect(70, -15, 30, 40)));
  EXPECT_FALSE(left_tab->HitTestRect(gfx::Rect(87, 20, 5, 3)));

  // Hit tests completely outside of the tab.
  EXPECT_FALSE(left_tab->HitTestRect(gfx::Rect(-20, -25, 1, 1)));
  EXPECT_FALSE(left_tab->HitTestRect(gfx::Rect(-20, -25, 3, 19)));

  // All hit tests against the tab close button should fail because
  // it is occluded by |active_tab|.
  views::ImageButton* left_close = left_tab->close_button_;
  EXPECT_FALSE(left_close->HitTestRect(gfx::Rect(1, 1, 1, 1)));
  EXPECT_FALSE(left_close->HitTestRect(gfx::Rect(1, 1, 5, 10)));
  EXPECT_FALSE(left_close->HitTestRect(gfx::Rect(10, 10, 1, 1)));
  EXPECT_FALSE(left_close->HitTestRect(gfx::Rect(10, 10, 3, 4)));


  // Tests involving |active_tab|, which is completely visible.

  tab_bounds = GetTabHitTestMask(active_tab);
  EXPECT_EQ(gfx::Rect(6, 2, 108, 27).ToString(), tab_bounds.ToString());
  contents_bounds = GetTabCloseHitTestMask(active_tab, false);
  // TODO(tdanderson): Uncomment this line once crbug.com/311609 is resolved.
  //EXPECT_EQ(gfx::Rect(84, 8, 18, 18).ToString(), contents_bounds.ToString());

  // Verify that the tab close button is not occluded.
  EXPECT_TRUE(tab_bounds.Contains(contents_bounds));

  // Bounds of the tab close button (without padding) in the tab's
  // coordinate space.
  gfx::Rect local_bounds = GetTabCloseHitTestMask(active_tab, true);
  EXPECT_EQ(gfx::Rect(81, 0, 39, 29).ToString(), local_bounds.ToString());

  // Hit tests within the tab.
  EXPECT_TRUE(active_tab->HitTestRect(gfx::Rect(30, 15, 1, 1)));
  EXPECT_TRUE(active_tab->HitTestRect(gfx::Rect(30, 15, 2, 2)));

  // Hit tests against the tab close button. Note that if the hit test
  // source is a mouse, a hit test within the button's padding should fail.
  views::ImageButton* active_close = active_tab->close_button_;
  EXPECT_FALSE(active_close->HitTestRect(gfx::Rect(1, 1, 1, 1)));
  EXPECT_TRUE(active_close->HitTestRect(gfx::Rect(1, 1, 2, 2)));
  EXPECT_TRUE(active_close->HitTestRect(gfx::Rect(10, 10, 1, 1)));
  EXPECT_TRUE(active_close->HitTestRect(gfx::Rect(10, 10, 25, 35)));


  // Tests involving |most_right_tab|, which has part of its bounds occluded
  // by |right_tab| but has its tab close button completely visible.

  tab_bounds = GetTabHitTestMask(most_right_tab);
  EXPECT_EQ(gfx::Rect(84, 2, 30, 27).ToString(), tab_bounds.ToString());
  contents_bounds = GetTabCloseHitTestMask(active_tab, false);
  // TODO(tdanderson): Uncomment this line once crbug.com/311609 is resolved.
  //EXPECT_EQ(gfx::Rect(84, 8, 18, 18).ToString(), contents_bounds.ToString());
  local_bounds = GetTabCloseHitTestMask(active_tab, true);
  EXPECT_EQ(gfx::Rect(81, 0, 39, 29).ToString(), local_bounds.ToString());

  // Verify that the tab close button is not occluded.
  EXPECT_TRUE(tab_bounds.Contains(contents_bounds));

  // Hit tests in the occluded region of the tab.
  EXPECT_FALSE(most_right_tab->HitTestRect(gfx::Rect(20, 15, 1, 1)));
  EXPECT_FALSE(most_right_tab->HitTestRect(gfx::Rect(20, 15, 5, 6)));

  // Hit tests in the non-occluded region of the tab.
  EXPECT_TRUE(most_right_tab->HitTestRect(gfx::Rect(85, 15, 1, 1)));
  EXPECT_TRUE(most_right_tab->HitTestRect(gfx::Rect(85, 15, 2, 2)));

  // Hit tests against the tab close button. Note that if the hit test
  // source is a mouse, a hit test within the button's padding should fail.
  views::ImageButton* most_right_close = most_right_tab->close_button_;
  EXPECT_FALSE(most_right_close->HitTestRect(gfx::Rect(1, 1, 1, 1)));
  EXPECT_TRUE(most_right_close->HitTestRect(gfx::Rect(1, 1, 2, 2)));
  EXPECT_TRUE(most_right_close->HitTestRect(gfx::Rect(10, 10, 1, 1)));
  EXPECT_TRUE(most_right_close->HitTestRect(gfx::Rect(10, 10, 25, 35)));
  EXPECT_TRUE(most_right_close->HitTestRect(gfx::Rect(-10, 10, 25, 35)));
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
  most_right_tab->SetBoundsRect(gfx::Rect(gfx::Point(450, 0),
                                          gfx::Size(200, 20)));

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
  most_right_tab->SetBoundsRect(gfx::Rect(gfx::Point(450, 0),
                                          gfx::Size(200, 20)));

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
