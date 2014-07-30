// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "components/autofill/content/renderer/page_click_listener.h"
#include "components/autofill/content/renderer/page_click_tracker.h"
#include "content/public/renderer/render_view.h"
#include "content/public/test/render_view_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebInputElement.h"
#include "third_party/WebKit/public/web/WebTextAreaElement.h"
#include "third_party/WebKit/public/web/WebView.h"
#include "third_party/WebKit/public/platform/WebSize.h"
#include "ui/events/keycodes/keyboard_codes.h"

namespace autofill {

class TestPageClickListener : public PageClickListener {
 public:
  TestPageClickListener()
      : form_control_element_clicked_called_(false),
        was_focused_(false) {}

  virtual void FormControlElementClicked(
      const blink::WebFormControlElement& element,
      bool was_focused) OVERRIDE {
    form_control_element_clicked_called_ = true;
    form_control_element_clicked_ = element;
    was_focused_ = was_focused;
  }

  void ClearResults() {
    form_control_element_clicked_called_ = false;
    form_control_element_clicked_.reset();
    was_focused_ = false;
  }

  bool form_control_element_clicked_called_;
  blink::WebFormControlElement form_control_element_clicked_;
  bool was_focused_;
};

class PageClickTrackerTest : public content::RenderViewTest {
 protected:
  virtual void SetUp() OVERRIDE {
    content::RenderViewTest::SetUp();

    // RenderView creates PageClickTracker but it doesn't keep it around.
    // Rather than make it do so for the test, we create a new object.
    page_click_tracker_.reset(new PageClickTracker(view_, &test_listener_));

    LoadHTML("<form>"
             "  <input type='text' id='text_1'></input><br>"
             "  <input type='text' id='text_2'></input><br>"
             "  <textarea  id='textarea_1'></textarea><br>"
             "  <textarea  id='textarea_2'></textarea><br>"
             "  <input type='button' id='button'></input><br>"
             "</form>");
    GetWebWidget()->resize(blink::WebSize(500, 500));
    GetWebWidget()->setFocus(true);
    blink::WebDocument document = view_->GetWebView()->mainFrame()->document();
    text_ = document.getElementById("text_1");
    textarea_ = document.getElementById("textarea_1");
    ASSERT_FALSE(text_.isNull());
    ASSERT_FALSE(textarea_.isNull());
  }

  virtual void TearDown() OVERRIDE {
    text_.reset();
    textarea_.reset();
    test_listener_.ClearResults();
    page_click_tracker_.reset();
    content::RenderViewTest::TearDown();
  }

  // Simulates a click on the given element and then waits for the stack
  // to unwind.
  void SendElementClick(const std::string& element_id) {
    EXPECT_TRUE(SimulateElementClick(element_id));
    ProcessPendingMessages();
  }

  // Send all the messages required for a complete key press.
  void SendKeyPress(int key_code) {
    blink::WebKeyboardEvent keyboard_event;
    keyboard_event.windowsKeyCode = key_code;
    keyboard_event.setKeyIdentifierFromWindowsKeyCode();

    keyboard_event.type = blink::WebInputEvent::RawKeyDown;
    SendWebKeyboardEvent(keyboard_event);

    keyboard_event.type = blink::WebInputEvent::Char;
    SendWebKeyboardEvent(keyboard_event);

    keyboard_event.type = blink::WebInputEvent::KeyUp;
    SendWebKeyboardEvent(keyboard_event);
  }

  scoped_ptr<PageClickTracker> page_click_tracker_;
  TestPageClickListener test_listener_;
  blink::WebElement text_;
  blink::WebElement textarea_;
};

// Tests that PageClickTracker does notify correctly when an input
// node is clicked.
TEST_F(PageClickTrackerTest, PageClickTrackerInputClicked) {
  EXPECT_NE(text_, text_.document().focusedElement());
  // Click the text field once.
  SendElementClick("text_1");
  EXPECT_TRUE(test_listener_.form_control_element_clicked_called_);
  EXPECT_FALSE(test_listener_.was_focused_);
  EXPECT_TRUE(text_ == test_listener_.form_control_element_clicked_);
  test_listener_.ClearResults();

  // Click the text field again to test that was_focused_ is set correctly.
  SendElementClick("text_1");
  EXPECT_TRUE(test_listener_.form_control_element_clicked_called_);
  EXPECT_TRUE(test_listener_.was_focused_);
  EXPECT_TRUE(text_ == test_listener_.form_control_element_clicked_);
  test_listener_.ClearResults();

  // Click the button, no notification should happen (this is not a text-input).
  SendElementClick("button");
  EXPECT_FALSE(test_listener_.form_control_element_clicked_called_);
}

// Tests that PageClickTracker does notify correctly when a textarea
// node is clicked.
TEST_F(PageClickTrackerTest, PageClickTrackerTextAreaClicked) {
  // Click the textarea field once.
  SendElementClick("textarea_1");
  EXPECT_TRUE(test_listener_.form_control_element_clicked_called_);
  EXPECT_FALSE(test_listener_.was_focused_);
  EXPECT_TRUE(textarea_ == test_listener_.form_control_element_clicked_);
  test_listener_.ClearResults();

  // Click the textarea field again to test that was_focused_ is set correctly.
  SendElementClick("textarea_1");
  EXPECT_TRUE(test_listener_.form_control_element_clicked_called_);
  EXPECT_TRUE(test_listener_.was_focused_);
  EXPECT_TRUE(textarea_ == test_listener_.form_control_element_clicked_);
  test_listener_.ClearResults();

  // Click the button, no notification should happen (this is not a text-input).
  SendElementClick("button");
  EXPECT_FALSE(test_listener_.form_control_element_clicked_called_);
}

}  // namespace autofill
