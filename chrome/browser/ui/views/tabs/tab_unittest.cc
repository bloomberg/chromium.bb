// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/tab.h"
#include "chrome/browser/ui/views/tabs/tab_controller.h"

#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/models/list_selection_model.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/widget/widget.h"

using views::Widget;

class FakeTabController : public TabController {
 public:
  FakeTabController() : immersive_style_(false), active_tab_(false) {
  }
  virtual ~FakeTabController() {}

  void set_immersive_style(bool value) { immersive_style_ = value; }
  void set_active_tab(bool value) { active_tab_ = value; }

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
                                     const gfx::Point& p,
                                     ui::MenuSourceType source_type) OVERRIDE {}
  virtual bool IsActiveTab(const Tab* tab) const OVERRIDE {
    return active_tab_;
  }
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
  bool active_tab_;

  DISALLOW_COPY_AND_ASSIGN(FakeTabController);
};

class TabTest : public views::ViewsTestBase {
 public:
  TabTest() {}
  virtual ~TabTest() {}

  static bool IconAnimationInvariant(const Tab& tab) {
    return tab.data().CaptureActive() == !!tab.capture_icon_animation_.get();
  }

  static void CheckForExpectedLayoutAndVisibilityOfElements(const Tab& tab) {
    // Check whether elements are visible when they are supposed to be, given
    // Tab size and TabRendererData state.
    if (tab.data_.mini) {
      if (tab.data_.CaptureActive())
        EXPECT_TRUE(tab.ShouldShowIcon());
      else
        EXPECT_TRUE(tab.ShouldShowIcon() != tab.ShouldShowAudioIndicator());
      EXPECT_FALSE(tab.ShouldShowCloseBox());
    } else if (tab.IsActive()) {
      EXPECT_TRUE(tab.ShouldShowCloseBox());
      switch (tab.IconCapacity()) {
        case 0:
        case 1:
          EXPECT_FALSE(tab.ShouldShowIcon());
          EXPECT_FALSE(tab.ShouldShowAudioIndicator());
          break;
        case 2:
          if (tab.data_.CaptureActive())
            EXPECT_TRUE(tab.ShouldShowIcon());
          else
            EXPECT_TRUE(tab.ShouldShowIcon() != tab.ShouldShowAudioIndicator());
          break;
        default:
          EXPECT_LE(3, tab.IconCapacity());
          EXPECT_TRUE(tab.ShouldShowIcon());
          if (tab.data_.CaptureActive()) {
            EXPECT_FALSE(tab.ShouldShowAudioIndicator());
          } else {
            EXPECT_TRUE(tab.data_.AudioActive() ==
                            tab.ShouldShowAudioIndicator());
          }
          break;
      }
    } else {  // Tab not active and not mini tab.
      switch (tab.IconCapacity()) {
        case 0:
          EXPECT_FALSE(tab.ShouldShowCloseBox());
          EXPECT_FALSE(tab.ShouldShowIcon());
          EXPECT_FALSE(tab.ShouldShowAudioIndicator());
          break;
        case 1:
          EXPECT_FALSE(tab.ShouldShowCloseBox());
          if (tab.data_.CaptureActive())
            EXPECT_TRUE(tab.ShouldShowIcon());
          else
            EXPECT_TRUE(tab.ShouldShowIcon() != tab.ShouldShowAudioIndicator());
          break;
        default:
          EXPECT_LE(2, tab.IconCapacity());
          EXPECT_TRUE(tab.ShouldShowIcon());
          if (tab.data_.CaptureActive()) {
            EXPECT_FALSE(tab.ShouldShowAudioIndicator());
          } else {
            EXPECT_TRUE(tab.data_.AudioActive() ==
                            tab.ShouldShowAudioIndicator());
          }
          break;
      }
    }

    // Check positioning of elements with respect to each other, and that they
    // are fully within the contents bounds.
    const gfx::Rect contents_bounds = tab.GetContentsBounds();
    if (tab.ShouldShowIcon()) {
      EXPECT_LE(contents_bounds.x(), tab.favicon_bounds_.x());
      if (tab.title_bounds_.width() > 0)
        EXPECT_LE(tab.favicon_bounds_.right(), tab.title_bounds_.x());
      EXPECT_LE(contents_bounds.y(), tab.favicon_bounds_.y());
      EXPECT_LE(tab.favicon_bounds_.bottom(), contents_bounds.bottom());
    }
    if (tab.ShouldShowIcon() && tab.ShouldShowAudioIndicator())
      EXPECT_LE(tab.favicon_bounds_.right(), tab.audio_indicator_bounds_.x());
    if (tab.ShouldShowAudioIndicator()) {
      if (tab.title_bounds_.width() > 0)
        EXPECT_LE(tab.title_bounds_.right(), tab.audio_indicator_bounds_.x());
      EXPECT_LE(tab.audio_indicator_bounds_.right(), contents_bounds.right());
      EXPECT_LE(contents_bounds.y(), tab.audio_indicator_bounds_.y());
      EXPECT_LE(tab.audio_indicator_bounds_.bottom(), contents_bounds.bottom());
    }
    if (tab.ShouldShowAudioIndicator() && tab.ShouldShowCloseBox()) {
      // Note: The audio indicator can overlap the left-insets of the close box,
      // but should otherwise be to the left of the close button.
      EXPECT_LE(tab.audio_indicator_bounds_.right(),
                tab.close_button_->bounds().x() +
                    tab.close_button_->GetInsets().left());
    }
    if (tab.ShouldShowCloseBox()) {
      // Note: The title bounds can overlap the left-insets of the close box,
      // but should otherwise be to the left of the close button.
      if (tab.title_bounds_.width() > 0) {
        EXPECT_LE(tab.title_bounds_.right(),
                  tab.close_button_->bounds().x() +
                      tab.close_button_->GetInsets().left());
      }
      EXPECT_LE(tab.close_button_->bounds().right(), contents_bounds.right());
      EXPECT_LE(contents_bounds.y(), tab.close_button_->bounds().y());
      EXPECT_LE(tab.close_button_->bounds().bottom(), contents_bounds.bottom());
    }
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

TEST_F(TabTest, LayoutAndVisibilityOfElements) {
  FakeTabController controller;
  Tab tab(&controller);

  SkBitmap bitmap;
  bitmap.setConfig(SkBitmap::kARGB_8888_Config, 16, 16);
  bitmap.allocPixels();
  TabRendererData data;
  data.favicon = gfx::ImageSkia::CreateFrom1xBitmap(bitmap);

  // Perform layout over all possible combinations, checking for correct
  // results.
  for (int is_mini_tab = 0; is_mini_tab < 2; ++is_mini_tab) {
    for (int is_active_tab = 0; is_active_tab < 2; ++is_active_tab) {
      for (int audio_state = TabRendererData::AUDIO_STATE_NONE;
           audio_state <= TabRendererData::AUDIO_STATE_PLAYING; ++audio_state) {
        for (int capture_state = TabRendererData::CAPTURE_STATE_NONE;
             capture_state <= TabRendererData::CAPTURE_STATE_PROJECTING;
             ++capture_state) {
          SCOPED_TRACE(::testing::Message()
                       << (is_active_tab ? "Active" : "Inactive") << ' '
                       << (is_mini_tab ? "Mini " : "")
                       << "Tab with audio_state=" << audio_state
                       << " and capture_state=" << capture_state);
          data.mini = !!is_mini_tab;
          controller.set_active_tab(!!is_active_tab);
          data.audio_state =
              static_cast<TabRendererData::AudioState>(audio_state);
          data.capture_state =
              static_cast<TabRendererData::CaptureState>(capture_state);
          tab.SetData(data);

          // Test layout for every width from standard to minimum.
          gfx::Rect bounds(gfx::Point(0, 0), Tab::GetStandardSize());
          int min_width;
          if (is_mini_tab) {
            bounds.set_width(Tab::GetMiniWidth());
            min_width = Tab::GetMiniWidth();
          } else {
            min_width = is_active_tab ? Tab::GetMinimumSelectedSize().width() :
                Tab::GetMinimumUnselectedSize().width();
          }
          while (bounds.width() >= min_width) {
            SCOPED_TRACE(::testing::Message()
                         << "bounds=" << bounds.ToString());
            tab.SetBoundsRect(bounds);  // Invokes Tab::Layout().
            CheckForExpectedLayoutAndVisibilityOfElements(tab);
            bounds.set_width(bounds.width() - 1);
          }
        }
      }
    }
  }
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

TEST_F(TabTest, RecordingAndProjectingActivityIndicators) {
  FakeTabController controller;
  Tab tab(&controller);
  tab.SetBoundsRect(gfx::Rect(gfx::Point(0, 0), Tab::GetStandardSize()));

  SkBitmap bitmap;
  bitmap.setConfig(SkBitmap::kARGB_8888_Config, 16, 16);
  bitmap.allocPixels();

  TabRendererData data;
  data.favicon = gfx::ImageSkia::CreateFrom1xBitmap(bitmap);
  tab.SetData(data);

  // Recording starts and stops.
  data.capture_state = TabRendererData::CAPTURE_STATE_RECORDING;
  tab.SetData(data);
  EXPECT_TRUE(IconAnimationInvariant(tab));
  EXPECT_EQ(TabRendererData::CAPTURE_STATE_RECORDING, tab.data().capture_state);
  data.capture_state = TabRendererData::CAPTURE_STATE_NONE;
  tab.SetData(data);
  EXPECT_TRUE(IconAnimationInvariant(tab));
  EXPECT_EQ(TabRendererData::CAPTURE_STATE_NONE, tab.data().capture_state);
  EXPECT_TRUE(IconAnimationInvariant(tab));

  // Recording starts then tab capture starts, then back to just recording, then
  // recording stops.
  data.capture_state = TabRendererData::CAPTURE_STATE_RECORDING;
  tab.SetData(data);
  EXPECT_TRUE(IconAnimationInvariant(tab));
  EXPECT_EQ(TabRendererData::CAPTURE_STATE_RECORDING, tab.data().capture_state);

  data.title = ASCIIToUTF16("test X");
  tab.SetData(data);
  EXPECT_TRUE(IconAnimationInvariant(tab));

  data.capture_state = TabRendererData::CAPTURE_STATE_PROJECTING;
  tab.SetData(data);
  EXPECT_TRUE(IconAnimationInvariant(tab));
  EXPECT_EQ(TabRendererData::CAPTURE_STATE_PROJECTING,
            tab.data().capture_state);

  data.title = ASCIIToUTF16("test Y");
  tab.SetData(data);
  EXPECT_TRUE(IconAnimationInvariant(tab));

  data.capture_state = TabRendererData::CAPTURE_STATE_RECORDING;
  tab.SetData(data);
  EXPECT_TRUE(IconAnimationInvariant(tab));
  EXPECT_EQ(TabRendererData::CAPTURE_STATE_RECORDING, tab.data().capture_state);

  data.capture_state = TabRendererData::CAPTURE_STATE_NONE;
  tab.SetData(data);
  EXPECT_TRUE(IconAnimationInvariant(tab));
  EXPECT_EQ(TabRendererData::CAPTURE_STATE_NONE, tab.data().capture_state);
}
