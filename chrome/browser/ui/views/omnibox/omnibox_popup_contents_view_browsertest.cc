// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/omnibox/omnibox_popup_contents_view.h"

#include <memory>

#include "base/command_line.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/omnibox/omnibox_result_view.h"
#include "chrome/browser/ui/views/omnibox/omnibox_view_views.h"
#include "chrome/browser/ui/views/omnibox/rounded_omnibox_results_frame.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/omnibox/browser/omnibox_edit_model.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "components/omnibox/browser/omnibox_popup_model.h"
#include "ui/base/ui_base_switches.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/widget/widget.h"

namespace {

enum PopupType { WIDE, NARROW, ROUNDED };

// A View that positions itself over another View to intercept clicks.
class ClickTrackingOverlayView : public views::View {
 public:
  ClickTrackingOverlayView(views::View* over, gfx::Point* last_click)
      : last_click_(last_click) {
    SetBoundsRect(over->bounds());
    over->parent()->AddChildView(this);
  }

  // views::View:
  void OnMouseEvent(ui::MouseEvent* event) override {
    *last_click_ = event->location();
  }

 private:
  gfx::Point* last_click_;
};

std::string PrintPopupType(const testing::TestParamInfo<PopupType>& info) {
  switch (info.param) {
    case WIDE:
      return "Wide";
    case NARROW:
      return "Narrow";
    case ROUNDED:
      return "Rounded";
  }
  NOTREACHED();
  return std::string();
}

}  // namespace

class OmniboxPopupContentsViewTest
    : public InProcessBrowserTest,
      public ::testing::WithParamInterface<PopupType> {
 public:
  OmniboxPopupContentsViewTest() {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    switch (GetParam()) {
      case ROUNDED:
        command_line->AppendSwitchASCII(
            switches::kTopChromeMD,
            switches::kTopChromeMDMaterialTouchOptimized);
        break;
      case NARROW:
        feature_list_.InitAndEnableFeature(
            omnibox::kUIExperimentNarrowDropdown);
        break;
      default:
        break;
    }
  }

  views::Widget* CreatePopupForTestQuery();
  views::Widget* GetPopupWidget() { return popup_view()->GetWidget(); }
  OmniboxResultView* GetResultViewAt(int index) {
    return popup_view()->result_view_at(index);
  }

  ToolbarView* toolbar() {
    return BrowserView::GetBrowserViewForBrowser(browser())->toolbar();
  }
  LocationBarView* location_bar() { return toolbar()->location_bar(); }
  OmniboxViewViews* omnibox_view() { return location_bar()->omnibox_view(); }
  OmniboxEditModel* edit_model() { return omnibox_view()->model(); }
  OmniboxPopupModel* popup_model() { return edit_model()->popup_model(); }
  OmniboxPopupContentsView* popup_view() {
    return static_cast<OmniboxPopupContentsView*>(popup_model()->view());
  }

 private:
  base::test::ScopedFeatureList feature_list_;

  DISALLOW_COPY_AND_ASSIGN(OmniboxPopupContentsViewTest);
};

// Create an alias to instantiate tests that only care about the rounded style.
using RoundedOmniboxPopupContentsViewTest = OmniboxPopupContentsViewTest;

views::Widget* OmniboxPopupContentsViewTest::CreatePopupForTestQuery() {
  EXPECT_TRUE(popup_model()->result().empty());
  EXPECT_FALSE(popup_view()->IsOpen());
  EXPECT_FALSE(GetPopupWidget());

  edit_model()->SetUserText(base::ASCIIToUTF16("foo"));
  edit_model()->StartAutocomplete(false, false);

  EXPECT_FALSE(popup_model()->result().empty());
  EXPECT_TRUE(popup_view()->IsOpen());
  views::Widget* popup = GetPopupWidget();
  EXPECT_TRUE(popup);
  return popup;
}

// Tests widget alignment of the different popup types.
IN_PROC_BROWSER_TEST_P(OmniboxPopupContentsViewTest, PopupAlignment) {
  views::Widget* popup = CreatePopupForTestQuery();

  if (GetParam() == WIDE) {
    EXPECT_EQ(toolbar()->width(), popup->GetRestoredBounds().width());
  } else if (GetParam() == NARROW) {
    // Inset for the views::BubbleBorder::SMALL_SHADOW.
    gfx::Insets border_insets = popup_view()->GetInsets();
    EXPECT_EQ(location_bar()->width() + border_insets.width(),
              popup->GetRestoredBounds().width());
  } else {
    gfx::Rect alignment_rect = location_bar()->GetBoundsInScreen();
    alignment_rect.Inset(
        -RoundedOmniboxResultsFrame::kLocationBarAlignmentInsets);
    // Top, left and right should align. Bottom depends on the results.
    gfx::Rect popup_rect = popup->GetRestoredBounds();
    EXPECT_EQ(popup_rect.y(), alignment_rect.y());
    EXPECT_EQ(popup_rect.x(), alignment_rect.x());
    EXPECT_EQ(popup_rect.right(), alignment_rect.right());
  }
}

// This is only enabled on ChromeOS for now, since it's hard to align an
// EventGenerator when multiple native windows are involved (the sanity check
// will fail). TODO(tapted): Enable everywhere.
#if defined(OS_CHROMEOS)
#define MAYBE_ClickOmnibox ClickOmnibox
#else
#define MAYBE_ClickOmnibox DISABLED_ClickOmnibox
#endif
// Test that clicks over the omnibox do not hit the popup.
IN_PROC_BROWSER_TEST_P(RoundedOmniboxPopupContentsViewTest,
                       MAYBE_ClickOmnibox) {
  CreatePopupForTestQuery();
  ui::test::EventGenerator generator(browser()->window()->GetNativeWindow());

  OmniboxResultView* result = GetResultViewAt(0);
  ASSERT_TRUE(result);

  // Sanity check: ensure the EventGenerator clicks where we think it should
  // when clicking on a result (but don't dismiss the popup yet). This will fail
  // if the WindowTreeHost and EventGenerator coordinate systems do not align.
  {
    const gfx::Point expected_point = result->GetLocalBounds().CenterPoint();
    EXPECT_NE(gfx::Point(), expected_point);

    gfx::Point click;
    ClickTrackingOverlayView overlay(result, &click);
    generator.MoveMouseTo(result->GetBoundsInScreen().CenterPoint());
    generator.ClickLeftButton();
    EXPECT_EQ(expected_point, click);
  }

  // Select the text, so that we can test whether a click is received (which
  // should deselect the text);
  omnibox_view()->SelectAll(true);
  views::Textfield* textfield = omnibox_view();
  EXPECT_EQ(base::ASCIIToUTF16("foo"), textfield->GetSelectedText());

  generator.MoveMouseTo(location_bar()->GetBoundsInScreen().CenterPoint());
  generator.ClickLeftButton();
  EXPECT_EQ(base::string16(), textfield->GetSelectedText());

  // Clicking the result should dismiss the popup (asynchronously).
  generator.MoveMouseTo(result->GetBoundsInScreen().CenterPoint());

  ASSERT_TRUE(GetPopupWidget());
  EXPECT_FALSE(GetPopupWidget()->IsClosed());

  generator.ClickLeftButton();
  ASSERT_TRUE(GetPopupWidget());
  EXPECT_TRUE(GetPopupWidget()->IsClosed());
}

INSTANTIATE_TEST_CASE_P(,
                        OmniboxPopupContentsViewTest,
                        ::testing::Values(WIDE, NARROW, ROUNDED),
                        &PrintPopupType);

INSTANTIATE_TEST_CASE_P(,
                        RoundedOmniboxPopupContentsViewTest,
                        ::testing::Values(ROUNDED),
                        &PrintPopupType);
