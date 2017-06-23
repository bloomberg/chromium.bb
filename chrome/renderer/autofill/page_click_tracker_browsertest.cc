// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "chrome/test/base/chrome_render_view_test.h"
#include "components/autofill/content/renderer/autofill_agent.h"
#include "components/autofill/content/renderer/page_click_listener.h"
#include "components/autofill/content/renderer/page_click_tracker.h"
#include "content/public/renderer/render_view.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/platform/WebFloatPoint.h"
#include "third_party/WebKit/public/platform/WebSize.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebInputElement.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "third_party/WebKit/public/web/WebSettings.h"
#include "third_party/WebKit/public/web/WebView.h"
#include "ui/events/keycodes/keyboard_codes.h"

namespace autofill {

class TestPageClickListener : public PageClickListener {
 public:
  TestPageClickListener()
      : form_control_element_clicked_called_(false),
        was_focused_(false) {}

  void FormControlElementClicked(const blink::WebFormControlElement& element,
                                 bool was_focused) override {
    form_control_element_clicked_called_ = true;
    form_control_element_clicked_ = element;
    was_focused_ = was_focused;
  }

  void ClearResults() {
    form_control_element_clicked_called_ = false;
    form_control_element_clicked_.Reset();
    was_focused_ = false;
  }

  bool form_control_element_clicked_called_;
  blink::WebFormControlElement form_control_element_clicked_;
  bool was_focused_;
};

class PageClickTrackerTest : public ChromeRenderViewTest {
 protected:
  void SetUp() override {
    ChromeRenderViewTest::SetUp();

    // PageClickTracker is created and owned by AutofillAgent. To setup our test
    // listener we need to use our copy of PageClickTracker and set it up for
    // testing. The ownership will be transfered to AutofillAgent.
    auto page_click_tracker = base::MakeUnique<PageClickTracker>(
        view_->GetMainRenderFrame(), &test_listener_);
    autofill_agent_->set_page_click_tracker_for_testing(
        std::move(page_click_tracker));

    // Must be set before loading HTML.
    view_->GetWebView()->SetDefaultPageScaleLimits(1, 4);

    LoadHTML("<form>"
             "  <input type='text' id='text_1'></input><br>"
             "  <input type='text' id='text_2'></input><br>"
             "  <textarea  id='textarea_1'></textarea><br>"
             "  <textarea  id='textarea_2'></textarea><br>"
             "  <input type='button' id='button'></input><br>"
             "  <input type='button' id='button_2' disabled></input><br>"
             "</form>");
    GetWebWidget()->Resize(blink::WebSize(500, 500));
    GetWebWidget()->SetFocus(true);
    blink::WebDocument document = GetMainFrame()->GetDocument();
    text_ = document.GetElementById("text_1");
    textarea_ = document.GetElementById("textarea_1");
    ASSERT_FALSE(text_.IsNull());
    ASSERT_FALSE(textarea_.IsNull());

    // Enable show-ime event when element is focused by indicating that a user
    // gesture has been processed since load.
    EXPECT_TRUE(SimulateElementClick("button"));
  }

  void TearDown() override {
    text_.Reset();
    textarea_.Reset();
    test_listener_.ClearResults();
    ChromeRenderViewTest::TearDown();
  }

  TestPageClickListener test_listener_;
  blink::WebElement text_;
  blink::WebElement textarea_;
};

// Tests that PageClickTracker does notify correctly when an input
// node is clicked.
TEST_F(PageClickTrackerTest, PageClickTrackerInputClicked) {
  EXPECT_NE(text_, text_.GetDocument().FocusedElement());
  // Click the text field once.
  EXPECT_TRUE(SimulateElementClick("text_1"));
  EXPECT_TRUE(test_listener_.form_control_element_clicked_called_);
  EXPECT_FALSE(test_listener_.was_focused_);
  EXPECT_TRUE(text_ == test_listener_.form_control_element_clicked_);
  test_listener_.ClearResults();

  // Click the text field again to test that was_focused_ is set correctly.
  EXPECT_TRUE(SimulateElementClick("text_1"));
  EXPECT_TRUE(test_listener_.form_control_element_clicked_called_);
  EXPECT_TRUE(test_listener_.was_focused_);
  EXPECT_TRUE(text_ == test_listener_.form_control_element_clicked_);
  test_listener_.ClearResults();

  // Click the button, no notification should happen (this is not a text-input).
  EXPECT_TRUE(SimulateElementClick("button"));
  EXPECT_FALSE(test_listener_.form_control_element_clicked_called_);
}

// Tests that PageClickTracker does not notify when there is right click.
TEST_F(PageClickTrackerTest, PageClickTrackerInputRightClicked) {
  EXPECT_NE(text_, text_.GetDocument().FocusedElement());
  // Right click the text field once.
  EXPECT_TRUE(SimulateElementRightClick("text_1"));
  EXPECT_FALSE(test_listener_.form_control_element_clicked_called_);
  EXPECT_FALSE(test_listener_.was_focused_);
  EXPECT_NE(text_, test_listener_.form_control_element_clicked_);
}

TEST_F(PageClickTrackerTest, PageClickTrackerInputFocusedAndClicked) {
  // Focus the text field without a click.
  ExecuteJavaScriptForTests("document.getElementById('text_1').focus();");
  EXPECT_FALSE(test_listener_.form_control_element_clicked_called_);
  test_listener_.ClearResults();

  // Click the focused text field to test that was_focused_ is set correctly.
  EXPECT_TRUE(SimulateElementClick("text_1"));
  EXPECT_TRUE(test_listener_.form_control_element_clicked_called_);
  EXPECT_TRUE(test_listener_.was_focused_);
  EXPECT_TRUE(text_ == test_listener_.form_control_element_clicked_);
}

// Tests that PageClickTracker does notify correctly when a textarea
// node is clicked.
TEST_F(PageClickTrackerTest, PageClickTrackerTextAreaClicked) {
  // Click the textarea field once.
  EXPECT_TRUE(SimulateElementClick("textarea_1"));
  EXPECT_TRUE(test_listener_.form_control_element_clicked_called_);
  EXPECT_FALSE(test_listener_.was_focused_);
  EXPECT_TRUE(textarea_ == test_listener_.form_control_element_clicked_);
  test_listener_.ClearResults();

  // Click the textarea field again to test that was_focused_ is set correctly.
  EXPECT_TRUE(SimulateElementClick("textarea_1"));
  EXPECT_TRUE(test_listener_.form_control_element_clicked_called_);
  EXPECT_TRUE(test_listener_.was_focused_);
  EXPECT_TRUE(textarea_ == test_listener_.form_control_element_clicked_);
  test_listener_.ClearResults();

  // Click the button, no notification should happen (this is not a text-input).
  EXPECT_TRUE(SimulateElementClick("button"));
  EXPECT_FALSE(test_listener_.form_control_element_clicked_called_);
}

TEST_F(PageClickTrackerTest, PageClickTrackerTextAreaFocusedAndClicked) {
  // Focus the textarea without a click.
  ExecuteJavaScriptForTests("document.getElementById('textarea_1').focus();");
  EXPECT_FALSE(test_listener_.form_control_element_clicked_called_);
  test_listener_.ClearResults();

  // Click the focused text field to test that was_focused_ is set correctly.
  EXPECT_TRUE(SimulateElementClick("textarea_1"));
  EXPECT_TRUE(test_listener_.form_control_element_clicked_called_);
  EXPECT_TRUE(test_listener_.was_focused_);
  EXPECT_TRUE(textarea_ == test_listener_.form_control_element_clicked_);
  test_listener_.ClearResults();
}

TEST_F(PageClickTrackerTest, PageClickTrackerScaledTextareaClicked) {
  EXPECT_NE(textarea_, textarea_.GetDocument().FocusedElement());
  view_->GetWebView()->SetPageScaleFactor(3);
  view_->GetWebView()->SetVisualViewportOffset(blink::WebFloatPoint(50, 50));

  // Click textarea_1.
  SimulatePointClick(gfx::Point(30, 30));
  EXPECT_TRUE(test_listener_.form_control_element_clicked_called_);
  EXPECT_FALSE(test_listener_.was_focused_);
  EXPECT_TRUE(textarea_ == test_listener_.form_control_element_clicked_);
}

TEST_F(PageClickTrackerTest, PageClickTrackerScaledTextareaTapped) {
  EXPECT_NE(textarea_, textarea_.GetDocument().FocusedElement());
  view_->GetWebView()->SetPageScaleFactor(3);
  view_->GetWebView()->SetVisualViewportOffset(blink::WebFloatPoint(50, 50));

  // Tap textarea_1.
  SimulateRectTap(gfx::Rect(30, 30, 30, 30));
  EXPECT_TRUE(test_listener_.form_control_element_clicked_called_);
  EXPECT_FALSE(test_listener_.was_focused_);
  EXPECT_TRUE(textarea_ == test_listener_.form_control_element_clicked_);
}

TEST_F(PageClickTrackerTest, PageClickTrackerDisabledInputClickedNoEvent) {
  EXPECT_NE(text_, text_.GetDocument().FocusedElement());
  // Click the text field once.
  EXPECT_TRUE(SimulateElementClick("text_1"));
  EXPECT_TRUE(test_listener_.form_control_element_clicked_called_);
  EXPECT_FALSE(test_listener_.was_focused_);
  EXPECT_TRUE(text_ == test_listener_.form_control_element_clicked_);
  test_listener_.ClearResults();

  // Click the disabled element.
  EXPECT_TRUE(SimulateElementClick("button_2"));
  EXPECT_FALSE(test_listener_.form_control_element_clicked_called_);
}

TEST_F(PageClickTrackerTest,
       PageClickTrackerClickDisabledInputDoesNotResetClickCounter) {
  EXPECT_NE(text_, text_.GetDocument().FocusedElement());
  // Click the text field once.
  EXPECT_TRUE(SimulateElementClick("text_1"));
  EXPECT_TRUE(test_listener_.form_control_element_clicked_called_);
  EXPECT_FALSE(test_listener_.was_focused_);
  EXPECT_TRUE(text_ == test_listener_.form_control_element_clicked_);
  test_listener_.ClearResults();

  // Click the disabled element.
  EXPECT_TRUE(SimulateElementClick("button_2"));
  EXPECT_FALSE(test_listener_.form_control_element_clicked_called_);
  test_listener_.ClearResults();

  // Click the text field second time. Page click tracker should know that this
  // is the second click.
  EXPECT_TRUE(SimulateElementClick("text_1"));
  EXPECT_TRUE(test_listener_.form_control_element_clicked_called_);
  EXPECT_TRUE(test_listener_.was_focused_);
  EXPECT_TRUE(text_ == test_listener_.form_control_element_clicked_);
}

TEST_F(PageClickTrackerTest, PageClickTrackerTapNearEdgeIsPageClick) {
  EXPECT_NE(text_, text_.GetDocument().FocusedElement());
  // Tap outside of element bounds, but tap width is overlapping the field.
  gfx::Rect element_bounds = GetElementBounds("text_1");
  SimulateRectTap(element_bounds -
                  gfx::Vector2d(element_bounds.width() / 2 + 1, 0));
  EXPECT_TRUE(test_listener_.form_control_element_clicked_called_);
  EXPECT_FALSE(test_listener_.was_focused_);
  EXPECT_TRUE(text_ == test_listener_.form_control_element_clicked_);
}

}  // namespace autofill
