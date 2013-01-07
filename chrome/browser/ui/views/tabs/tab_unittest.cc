// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/tab.h"

#include "chrome/browser/ui/views/tabs/tab_controller.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/models/list_selection_model.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/widget/widget.h"

using views::Widget;

class FakeTabController : public TabController {
 public:
  FakeTabController() : immersive_style_(false) {
  }
  virtual ~FakeTabController() {}

  void set_immersive_style(bool value) { immersive_style_ = value; }

  virtual const ui::ListSelectionModel& GetSelectionModel() OVERRIDE {
    return selection_model_;
  }
  virtual bool SupportsMultipleSelection() OVERRIDE { return false; }
  virtual void SelectTab(Tab* tab) OVERRIDE {}
  virtual void ExtendSelectionTo(Tab* tab) OVERRIDE {}
  virtual void ToggleSelected(Tab* tab) OVERRIDE {}
  virtual void AddSelectionFromAnchorTo(Tab* tab) OVERRIDE {}
  virtual void CloseTab(Tab* tab, CloseTabSource source) OVERRIDE {}
  virtual void ShowContextMenuForTab(Tab* tab,
                                     const gfx::Point& p) OVERRIDE {}
  virtual bool IsActiveTab(const Tab* tab) const OVERRIDE { return false; }
  virtual bool IsTabSelected(const Tab* tab) const OVERRIDE {
    return false;
  }
  virtual bool IsTabPinned(const Tab* tab) const OVERRIDE { return false; }
  virtual void MaybeStartDrag(
      Tab* tab,
      const ui::LocatedEvent& event,
      const ui::ListSelectionModel& original_selection) OVERRIDE {}
  virtual void ContinueDrag(views::View* view,
                            const gfx::Point& location) OVERRIDE {}
  virtual bool EndDrag(EndDragReason reason) OVERRIDE { return false; }
  virtual Tab* GetTabAt(Tab* tab,
                        const gfx::Point& tab_in_tab_coordinates) OVERRIDE {
    return NULL;
  }
  virtual void OnMouseEventInTab(views::View* source,
                                 const ui::MouseEvent& event) OVERRIDE {}
  virtual bool ShouldPaintTab(const Tab* tab, gfx::Rect* clip) OVERRIDE {
    return true;
  }
  virtual bool IsImmersiveStyle() const OVERRIDE { return immersive_style_; }

 private:
  ui::ListSelectionModel selection_model_;
  bool immersive_style_;

  DISALLOW_COPY_AND_ASSIGN(FakeTabController);
};

class TabTest : public views::ViewsTestBase {
 public:
  TabTest() {}
  virtual ~TabTest() {}

  static SkColor GetIconDominantColor(const Tab& tab) {
    return tab.icon_dominant_color_;
  }
};

TEST_F(TabTest, HitTestTopPixel) {
  Widget widget;
  Widget::InitParams params(CreateParams(Widget::InitParams::TYPE_WINDOW));
  params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.bounds.SetRect(10, 20, 300, 400);
  widget.Init(params);

  FakeTabController tab_controller;
  Tab tab(&tab_controller);
  widget.GetContentsView()->AddChildView(&tab);
  tab.SetBoundsRect(gfx::Rect(gfx::Point(0, 0), Tab::GetStandardSize()));

  // Tabs have some shadow in the top, so by default we don't hit the tab there.
  int middle_x = tab.width() / 2;
  EXPECT_FALSE(tab.HitTestPoint(gfx::Point(middle_x, 0)));

  // Tabs are slanted, so a click halfway down the left edge won't hit it.
  int middle_y = tab.height() / 2;
  EXPECT_FALSE(tab.HitTestPoint(gfx::Point(0, middle_y)));

  // If the window is maximized, however, we want clicks in the top edge to
  // select the tab.
  widget.Maximize();
  EXPECT_TRUE(tab.HitTestPoint(gfx::Point(middle_x, 0)));

  // But clicks in the area above the slanted sides should still miss.
  EXPECT_FALSE(tab.HitTestPoint(gfx::Point(0, 0)));
  EXPECT_FALSE(tab.HitTestPoint(gfx::Point(tab.width() - 1, 0)));
}

TEST_F(TabTest, IconDominantColor) {
  FakeTabController controller;
  Tab tab(&controller);

  // Dominant color defaults to white.
  EXPECT_EQ(SK_ColorWHITE, GetIconDominantColor(tab));

  // Make a red favicon.
  SkBitmap red_bitmap;
  red_bitmap.setConfig(SkBitmap::kARGB_8888_Config, 16, 16);
  red_bitmap.allocPixels();
  red_bitmap.eraseColor(SK_ColorRED);

  // Update the tab's renderer data with the red icon.
  TabRendererData red_data;
  red_data.favicon = gfx::ImageSkia(red_bitmap);
  tab.SetData(red_data);

  // Since we're not using immersive style yet, the color is not updated.
  EXPECT_EQ(SK_ColorWHITE, GetIconDominantColor(tab));

  // Switch to immersive style and make an arbitrary change to the data.
  controller.set_immersive_style(true);
  red_data.mini = true;
  tab.SetData(red_data);

  // Color is not updated because the new data has the same image as before.
  EXPECT_EQ(SK_ColorWHITE, GetIconDominantColor(tab));

  // Forcing an update makes it red.
  tab.UpdateIconDominantColor();
  EXPECT_EQ(SK_ColorRED, GetIconDominantColor(tab));

  // Change the favicon to a green icon.
  SkBitmap green_bitmap;
  green_bitmap.setConfig(SkBitmap::kARGB_8888_Config, 16, 16);
  green_bitmap.allocPixels();
  green_bitmap.eraseColor(SK_ColorGREEN);
  TabRendererData green_data;
  green_data.favicon = gfx::ImageSkia(green_bitmap);
  tab.SetData(green_data);

  // Icon updates automatically since we're in immersive mode and the image
  // changed.
  EXPECT_EQ(SK_ColorGREEN, GetIconDominantColor(tab));
}
