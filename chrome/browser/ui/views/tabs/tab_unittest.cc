// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/tab.h"

#include "base/i18n/rtl.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/tabs/tab_utils.h"
#include "chrome/browser/ui/views/tabs/media_indicator_button.h"
#include "chrome/browser/ui/views/tabs/tab_controller.h"
#include "grit/theme_resources.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/models/list_selection_model.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/widget/widget.h"

using views::Widget;

class FakeTabController : public TabController {
 public:
  FakeTabController() : immersive_style_(false), active_tab_(false) {
  }
  ~FakeTabController() override {}

  void set_immersive_style(bool value) { immersive_style_ = value; }
  void set_active_tab(bool value) { active_tab_ = value; }

  const ui::ListSelectionModel& GetSelectionModel() const override {
    return selection_model_;
  }
  bool SupportsMultipleSelection() override { return false; }
  bool ShouldHideCloseButtonForInactiveTabs() override {
    return false;
  }
  void SelectTab(Tab* tab) override {}
  void ExtendSelectionTo(Tab* tab) override {}
  void ToggleSelected(Tab* tab) override {}
  void AddSelectionFromAnchorTo(Tab* tab) override {}
  void CloseTab(Tab* tab, CloseTabSource source) override {}
  void ToggleTabAudioMute(Tab* tab) override {}
  void ShowContextMenuForTab(Tab* tab,
                             const gfx::Point& p,
                             ui::MenuSourceType source_type) override {}
  bool IsActiveTab(const Tab* tab) const override { return active_tab_; }
  bool IsTabSelected(const Tab* tab) const override { return false; }
  bool IsTabPinned(const Tab* tab) const override { return false; }
  void MaybeStartDrag(
      Tab* tab,
      const ui::LocatedEvent& event,
      const ui::ListSelectionModel& original_selection) override {}
  void ContinueDrag(views::View* view, const ui::LocatedEvent& event) override {
  }
  bool EndDrag(EndDragReason reason) override { return false; }
  Tab* GetTabAt(Tab* tab, const gfx::Point& tab_in_tab_coordinates) override {
    return NULL;
  }
  void OnMouseEventInTab(views::View* source,
                         const ui::MouseEvent& event) override {}
  bool ShouldPaintTab(const Tab* tab, gfx::Rect* clip) override { return true; }
  bool IsImmersiveStyle() const override { return immersive_style_; }
  int GetBackgroundResourceId(bool* custom_image) const override {
    *custom_image = false;
    return IDR_THEME_TAB_BACKGROUND;
  }
  void UpdateTabAccessibilityState(const Tab* tab,
                                   ui::AXViewState* state) override{};

 private:
  ui::ListSelectionModel selection_model_;
  bool immersive_style_;
  bool active_tab_;

  DISALLOW_COPY_AND_ASSIGN(FakeTabController);
};

class TabTest : public views::ViewsTestBase,
                public ::testing::WithParamInterface<bool> {
 public:
  TabTest() {}
  virtual ~TabTest() {}

  bool testing_for_rtl_locale() const { return GetParam(); }

  void SetUp() override {
    if (testing_for_rtl_locale()) {
      original_locale_ = base::i18n::GetConfiguredLocale();
      base::i18n::SetICUDefaultLocale("he");
    }
    views::ViewsTestBase::SetUp();
  }

  void TearDown() override {
    views::ViewsTestBase::TearDown();
    if (testing_for_rtl_locale())
      base::i18n::SetICUDefaultLocale(original_locale_);
  }

  static void CheckForExpectedLayoutAndVisibilityOfElements(const Tab& tab) {
    // Check whether elements are visible when they are supposed to be, given
    // Tab size and TabRendererData state.
    if (tab.data_.pinned) {
      EXPECT_EQ(1, tab.IconCapacity());
      if (tab.data_.media_state != TAB_MEDIA_STATE_NONE) {
        EXPECT_FALSE(tab.ShouldShowIcon());
        EXPECT_TRUE(tab.ShouldShowMediaIndicator());
      } else {
        EXPECT_TRUE(tab.ShouldShowIcon());
        EXPECT_FALSE(tab.ShouldShowMediaIndicator());
      }
      EXPECT_FALSE(tab.ShouldShowCloseBox());
    } else if (tab.IsActive()) {
      EXPECT_TRUE(tab.ShouldShowCloseBox());
      switch (tab.IconCapacity()) {
        case 0:
        case 1:
          EXPECT_FALSE(tab.ShouldShowIcon());
          EXPECT_FALSE(tab.ShouldShowMediaIndicator());
          break;
        case 2:
          if (tab.data_.media_state != TAB_MEDIA_STATE_NONE) {
            EXPECT_FALSE(tab.ShouldShowIcon());
            EXPECT_TRUE(tab.ShouldShowMediaIndicator());
          } else {
            EXPECT_TRUE(tab.ShouldShowIcon());
            EXPECT_FALSE(tab.ShouldShowMediaIndicator());
          }
          break;
        default:
          EXPECT_LE(3, tab.IconCapacity());
          EXPECT_TRUE(tab.ShouldShowIcon());
          if (tab.data_.media_state != TAB_MEDIA_STATE_NONE)
            EXPECT_TRUE(tab.ShouldShowMediaIndicator());
          else
            EXPECT_FALSE(tab.ShouldShowMediaIndicator());
          break;
      }
    } else {  // Tab not active and not pinned tab.
      switch (tab.IconCapacity()) {
        case 0:
          EXPECT_FALSE(tab.ShouldShowCloseBox());
          EXPECT_FALSE(tab.ShouldShowIcon());
          EXPECT_FALSE(tab.ShouldShowMediaIndicator());
          break;
        case 1:
          EXPECT_FALSE(tab.ShouldShowCloseBox());
          if (tab.data_.media_state != TAB_MEDIA_STATE_NONE) {
            EXPECT_FALSE(tab.ShouldShowIcon());
            EXPECT_TRUE(tab.ShouldShowMediaIndicator());
          } else {
            EXPECT_TRUE(tab.ShouldShowIcon());
            EXPECT_FALSE(tab.ShouldShowMediaIndicator());
          }
          break;
        default:
          EXPECT_LE(2, tab.IconCapacity());
          EXPECT_TRUE(tab.ShouldShowIcon());
          if (tab.data_.media_state != TAB_MEDIA_STATE_NONE)
            EXPECT_TRUE(tab.ShouldShowMediaIndicator());
          else
            EXPECT_FALSE(tab.ShouldShowMediaIndicator());
          break;
      }
    }

    // Check positioning of elements with respect to each other, and that they
    // are fully within the contents bounds.
    const gfx::Rect contents_bounds = tab.GetContentsBounds();
    if (tab.ShouldShowIcon()) {
      EXPECT_LE(contents_bounds.x(), tab.favicon_bounds_.x());
      if (tab.title_->width() > 0)
        EXPECT_LE(tab.favicon_bounds_.right(), tab.title_->x());
      EXPECT_LE(contents_bounds.y(), tab.favicon_bounds_.y());
      EXPECT_LE(tab.favicon_bounds_.bottom(), contents_bounds.bottom());
    }
    if (tab.ShouldShowIcon() && tab.ShouldShowMediaIndicator())
      EXPECT_LE(tab.favicon_bounds_.right(), GetMediaIndicatorBounds(tab).x());
    if (tab.ShouldShowMediaIndicator()) {
      if (tab.title_->width() > 0) {
        EXPECT_LE(tab.title_->bounds().right(),
                  GetMediaIndicatorBounds(tab).x());
      }
      EXPECT_LE(GetMediaIndicatorBounds(tab).right(), contents_bounds.right());
      EXPECT_LE(contents_bounds.y(), GetMediaIndicatorBounds(tab).y());
      EXPECT_LE(GetMediaIndicatorBounds(tab).bottom(),
                contents_bounds.bottom());
    }
    if (tab.ShouldShowMediaIndicator() && tab.ShouldShowCloseBox()) {
      // Note: The media indicator can overlap the left-insets of the close box,
      // but should otherwise be to the left of the close button.
      EXPECT_LE(GetMediaIndicatorBounds(tab).right(),
                tab.close_button_->bounds().x() +
                    tab.close_button_->GetInsets().left());
    }
    if (tab.ShouldShowCloseBox()) {
      // Note: The title bounds can overlap the left-insets of the close box,
      // but should otherwise be to the left of the close button.
      if (tab.title_->width() > 0) {
        EXPECT_LE(tab.title_->bounds().right(),
                  tab.close_button_->bounds().x() +
                      tab.close_button_->GetInsets().left());
      }
      // We need to use the close button contents bounds instead of its bounds,
      // since it has an empty border around it to extend its clickable area for
      // touch.
      // Note: The close button right edge can be outside the nominal contents
      // bounds, but shouldn't leave the local bounds.
      const gfx::Rect close_bounds = tab.close_button_->GetContentsBounds();
      EXPECT_LE(close_bounds.right(), tab.GetLocalBounds().right());
      EXPECT_LE(contents_bounds.y(), close_bounds.y());
      EXPECT_LE(close_bounds.bottom(), contents_bounds.bottom());
    }
  }

 protected:
  void InitWidget(Widget* widget) {
    Widget::InitParams params(CreateParams(Widget::InitParams::TYPE_WINDOW));
    params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
    params.bounds.SetRect(10, 20, 300, 400);
    widget->Init(params);
  }

 private:
  static gfx::Rect GetMediaIndicatorBounds(const Tab& tab) {
    if (!tab.media_indicator_button_) {
      ADD_FAILURE();
      return gfx::Rect();
    }
    return tab.media_indicator_button_->bounds();
  }

  std::string original_locale_;
};

TEST_P(TabTest, HitTestTopPixel) {
  if (testing_for_rtl_locale() && !base::i18n::IsRTL()) {
    LOG(WARNING) << "Testing of RTL locale not supported on current platform.";
    return;
  }

  Widget widget;
  InitWidget(&widget);

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

TEST_P(TabTest, LayoutAndVisibilityOfElements) {
  if (testing_for_rtl_locale() && !base::i18n::IsRTL()) {
    LOG(WARNING) << "Testing of RTL locale not supported on current platform.";
    return;
  }

  static const TabMediaState kMediaStatesToTest[] = {
    TAB_MEDIA_STATE_NONE, TAB_MEDIA_STATE_CAPTURING,
    TAB_MEDIA_STATE_AUDIO_PLAYING, TAB_MEDIA_STATE_AUDIO_MUTING
  };

  Widget widget;
  InitWidget(&widget);

  FakeTabController controller;
  Tab tab(&controller);
  widget.GetContentsView()->AddChildView(&tab);

  SkBitmap bitmap;
  bitmap.allocN32Pixels(16, 16);
  TabRendererData data;
  data.favicon = gfx::ImageSkia::CreateFrom1xBitmap(bitmap);

  // Perform layout over all possible combinations, checking for correct
  // results.
  for (int is_pinned_tab = 0; is_pinned_tab < 2; ++is_pinned_tab) {
    for (int is_active_tab = 0; is_active_tab < 2; ++is_active_tab) {
      for (size_t media_state_index = 0;
           media_state_index < arraysize(kMediaStatesToTest);
           ++media_state_index) {
        const TabMediaState media_state = kMediaStatesToTest[media_state_index];
        SCOPED_TRACE(::testing::Message()
                     << (is_active_tab ? "Active" : "Inactive") << ' '
                     << (is_pinned_tab ? "Pinned " : "")
                     << "Tab with media indicator state " << media_state);

        data.pinned = !!is_pinned_tab;
        controller.set_active_tab(!!is_active_tab);
        data.media_state = media_state;
        tab.SetData(data);

        // Test layout for every width from standard to minimum.
        gfx::Rect bounds(gfx::Point(0, 0), Tab::GetStandardSize());
        int min_width;
        if (is_pinned_tab) {
          bounds.set_width(Tab::GetPinnedWidth());
          min_width = Tab::GetPinnedWidth();
        } else {
          min_width = is_active_tab ? Tab::GetMinimumActiveSize().width()
                                    : Tab::GetMinimumInactiveSize().width();
        }
        while (bounds.width() >= min_width) {
          SCOPED_TRACE(::testing::Message() << "bounds=" << bounds.ToString());
          tab.SetBoundsRect(bounds);  // Invokes Tab::Layout().
          CheckForExpectedLayoutAndVisibilityOfElements(tab);
          bounds.set_width(bounds.width() - 1);
        }
      }
    }
  }
}

// Regression test for http://crbug.com/420313: Confirms that any child Views of
// Tab do not attempt to provide their own tooltip behavior/text. It also tests
// that Tab provides the expected tooltip text (according to tab_utils).
TEST_P(TabTest, TooltipProvidedByTab) {
  if (testing_for_rtl_locale() && !base::i18n::IsRTL()) {
    LOG(WARNING) << "Testing of RTL locale not supported on current platform.";
    return;
  }

  Widget widget;
  InitWidget(&widget);

  FakeTabController controller;
  Tab tab(&controller);
  widget.GetContentsView()->AddChildView(&tab);
  tab.SetBoundsRect(gfx::Rect(Tab::GetStandardSize()));

  SkBitmap bitmap;
  bitmap.allocN32Pixels(16, 16);
  TabRendererData data;
  data.favicon = gfx::ImageSkia::CreateFrom1xBitmap(bitmap);

  data.title = base::UTF8ToUTF16(
      "This is a really long tab title that would case views::Label to provide "
      "its own tooltip; but Tab should disable that feature so it can provide "
      "the tooltip instead.");

  // Test both with and without an indicator showing since the tab tooltip text
  // should include a description of the media state when the indicator is
  // present.
  for (int i = 0; i < 2; ++i) {
    data.media_state =
        (i == 0 ? TAB_MEDIA_STATE_NONE : TAB_MEDIA_STATE_AUDIO_PLAYING);
    SCOPED_TRACE(::testing::Message()
                 << "Tab with media indicator state " << data.media_state);
    tab.SetData(data);

    for (int j = 0; j < tab.child_count(); ++j) {
      views::View& child = *tab.child_at(j);
      if (!strcmp(child.GetClassName(), "TabCloseButton"))
        continue;  // Close button is excepted.
      if (!child.visible())
        continue;
      SCOPED_TRACE(::testing::Message() << "child_at(" << j << "): "
                   << child.GetClassName());

      const gfx::Point midpoint(child.width() / 2, child.height() / 2);
      EXPECT_FALSE(child.GetTooltipHandlerForPoint(midpoint));
      const gfx::Point mouse_hover_point =
          midpoint + child.GetMirroredPosition().OffsetFromOrigin();
      base::string16 tooltip;
      EXPECT_TRUE(static_cast<views::View&>(tab).GetTooltipText(
          mouse_hover_point, &tooltip));
      EXPECT_EQ(chrome::AssembleTabTooltipText(data.title, data.media_state),
                tooltip);
    }
  }
}

// Regression test for http://crbug.com/226253. Calling Layout() more than once
// shouldn't change the insets of the close button.
TEST_P(TabTest, CloseButtonLayout) {
  if (testing_for_rtl_locale() && !base::i18n::IsRTL()) {
    LOG(WARNING) << "Testing of RTL locale not supported on current platform.";
    return;
  }

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

// Test in both a LTR and a RTL locale.  Note: The fact that the UI code is
// configured for an RTL locale does *not* change how the coordinates are
// examined in the tests above because views::View and friends are supposed to
// auto-mirror the widgets when painting.  Thus, what we're testing here is that
// there's no code in Tab that will erroneously subvert this automatic
// coordinate translation.  http://crbug.com/384179
INSTANTIATE_TEST_CASE_P(, TabTest, ::testing::Values(false, true));
