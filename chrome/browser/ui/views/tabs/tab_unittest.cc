// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/tab.h"
#include "chrome/browser/ui/views/tabs/tab_controller.h"

#include "base/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/models/list_selection_model.h"
#include "ui/views/controls/button/image_button.h"
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
                            const ui::LocatedEvent& event) OVERRIDE {}
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

  static bool IconAnimationInvariant(const Tab& tab) {
    bool capture_invariant =
        tab.data().CaptureActive() == (tab.icon_animation_.get() != NULL);
    bool audio_invariant =
        !tab.data().AudioActive() || tab.tab_audio_indicator_->IsAnimating();
    return capture_invariant && audio_invariant;
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

// Regression test for http://crbug.com/226253. Calling Layout() more than once
// shouldn't change the insets of the close button.
TEST_F(TabTest, CloseButtonLayout) {
  FakeTabController tab_controller;
  Tab tab(&tab_controller);
  tab.SetBounds(0, 0, 100, 50);
  tab.Layout();
  gfx::Insets close_button_insets = tab.close_button_->GetInsets();
  tab.Layout();
  gfx::Insets close_button_insets_2 = tab.close_button_->GetInsets();
  EXPECT_EQ(close_button_insets.top(), close_button_insets_2.top());
  EXPECT_EQ(close_button_insets.left(), close_button_insets_2.left());
  EXPECT_EQ(close_button_insets.bottom(), close_button_insets_2.bottom());
  EXPECT_EQ(close_button_insets.right(), close_button_insets_2.right());

  // Also make sure the close button is sized as large as the tab.
  EXPECT_EQ(50, tab.close_button_->bounds().height());
}

TEST_F(TabTest, ActivityIndicators) {
  FakeTabController controller;
  Tab tab(&controller);

  SkBitmap bitmap;
  bitmap.setConfig(SkBitmap::kARGB_8888_Config, 16, 16);
  bitmap.allocPixels();

  TabRendererData data;
  data.favicon = gfx::ImageSkia::CreateFrom1xBitmap(bitmap);
  tab.SetData(data);

  // Audio starts and stops.
  data.audio_state = TabRendererData::AUDIO_STATE_PLAYING;
  tab.SetData(data);
  EXPECT_TRUE(IconAnimationInvariant(tab));
  EXPECT_EQ(TabRendererData::AUDIO_STATE_PLAYING, tab.data().audio_state);
  EXPECT_EQ(TabRendererData::CAPTURE_STATE_NONE, tab.data().capture_state);
  data.audio_state = TabRendererData::AUDIO_STATE_NONE;
  tab.SetData(data);
  EXPECT_TRUE(IconAnimationInvariant(tab));
  EXPECT_EQ(TabRendererData::AUDIO_STATE_NONE, tab.data().audio_state);
  EXPECT_EQ(TabRendererData::CAPTURE_STATE_NONE, tab.data().capture_state);
  EXPECT_TRUE(IconAnimationInvariant(tab));

  // Capture starts and stops.
  data.capture_state = TabRendererData::CAPTURE_STATE_RECORDING;
  tab.SetData(data);
  EXPECT_TRUE(IconAnimationInvariant(tab));
  EXPECT_EQ(TabRendererData::AUDIO_STATE_NONE, tab.data().audio_state);
  EXPECT_EQ(TabRendererData::CAPTURE_STATE_RECORDING, tab.data().capture_state);
  data.capture_state = TabRendererData::CAPTURE_STATE_NONE;
  tab.SetData(data);
  EXPECT_TRUE(IconAnimationInvariant(tab));
  EXPECT_EQ(TabRendererData::AUDIO_STATE_NONE, tab.data().audio_state);
  EXPECT_EQ(TabRendererData::CAPTURE_STATE_NONE, tab.data().capture_state);
  EXPECT_TRUE(IconAnimationInvariant(tab));

  // Audio starts then capture starts, then audio stops then capture stops.
  data.audio_state = TabRendererData::AUDIO_STATE_PLAYING;
  tab.SetData(data);
  data.capture_state = TabRendererData::CAPTURE_STATE_RECORDING;
  tab.SetData(data);
  EXPECT_TRUE(IconAnimationInvariant(tab));
  EXPECT_EQ(TabRendererData::AUDIO_STATE_PLAYING, tab.data().audio_state);
  EXPECT_EQ(TabRendererData::CAPTURE_STATE_RECORDING, tab.data().capture_state);

  data.title = ASCIIToUTF16("test X");
  tab.SetData(data);
  EXPECT_TRUE(IconAnimationInvariant(tab));

  data.audio_state = TabRendererData::AUDIO_STATE_NONE;
  tab.SetData(data);
  EXPECT_TRUE(IconAnimationInvariant(tab));
  EXPECT_EQ(TabRendererData::CAPTURE_STATE_RECORDING, tab.data().capture_state);
  data.capture_state = TabRendererData::CAPTURE_STATE_NONE;
  tab.SetData(data);
  EXPECT_TRUE(IconAnimationInvariant(tab));
  EXPECT_EQ(TabRendererData::AUDIO_STATE_NONE, tab.data().audio_state);
  EXPECT_EQ(TabRendererData::CAPTURE_STATE_NONE, tab.data().capture_state);

  // Audio starts then capture starts, then capture stops then audio stops.
  data.audio_state = TabRendererData::AUDIO_STATE_PLAYING;
  tab.SetData(data);
  data.capture_state = TabRendererData::CAPTURE_STATE_RECORDING;
  tab.SetData(data);
  EXPECT_TRUE(IconAnimationInvariant(tab));
  EXPECT_EQ(TabRendererData::AUDIO_STATE_PLAYING, tab.data().audio_state);
  EXPECT_EQ(TabRendererData::CAPTURE_STATE_RECORDING, tab.data().capture_state);

  data.title = ASCIIToUTF16("test Y");
  tab.SetData(data);
  EXPECT_TRUE(IconAnimationInvariant(tab));

  data.capture_state = TabRendererData::CAPTURE_STATE_NONE;
  tab.SetData(data);
  EXPECT_TRUE(IconAnimationInvariant(tab));
  EXPECT_EQ(TabRendererData::CAPTURE_STATE_NONE, tab.data().capture_state);

  data.audio_state = TabRendererData::AUDIO_STATE_NONE;
  tab.SetData(data);
  EXPECT_TRUE(IconAnimationInvariant(tab));
  EXPECT_EQ(TabRendererData::AUDIO_STATE_NONE, tab.data().audio_state);
  EXPECT_EQ(TabRendererData::CAPTURE_STATE_NONE, tab.data().capture_state);
  EXPECT_TRUE(IconAnimationInvariant(tab));
}
