// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "chrome/common/render_messages.h"
#include "chrome/renderer/page_click_listener.h"
#include "chrome/renderer/page_click_tracker.h"
#include "chrome/test/base/chrome_render_view_test.h"
#include "content/public/renderer/render_view.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDocument.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebInputElement.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebSize.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebView.h"
#include "ui/base/keycodes/keyboard_codes.h"

class TestPageClickListener : public PageClickListener {
 public:
  TestPageClickListener()
      : input_element_clicked_called_(false),
        input_element_lost_focus_called_(false),
        was_focused_(false),
        is_focused_(false),
        notification_response_(false) {
  }

  virtual bool InputElementClicked(const WebKit::WebInputElement& element,
                                   bool was_focused,
                                   bool is_focused) {
    input_element_clicked_called_ = true;
    element_clicked_ = element;
    was_focused_ = was_focused;
    is_focused_ = is_focused;
    return notification_response_;
  }

  virtual bool InputElementLostFocus() {
    input_element_lost_focus_called_ = true;
    return notification_response_;
  }

  void ClearResults() {
    input_element_clicked_called_ = false;
    input_element_lost_focus_called_ = false;
    element_clicked_.reset();
    was_focused_ = false;
    is_focused_ = false;
  }

  bool input_element_clicked_called_;
  bool input_element_lost_focus_called_;
  WebKit::WebInputElement element_clicked_;
  bool was_focused_;
  bool is_focused_;
  bool notification_response_;
};

class PageClickTrackerTest : public ChromeRenderViewTest {
 protected:
  virtual void SetUp() {
    ChromeRenderViewTest::SetUp();

    // RenderView creates PageClickTracker but it doesn't keep it around.
    // Rather than make it do so for the test, we create a new object.
    page_click_tracker_.reset(new PageClickTracker(view_));
    page_click_tracker_->AddListener(&test_listener1_);
    page_click_tracker_->AddListener(&test_listener2_);

    LoadHTML("<form>"
             "  <input type='text' id='text_1'></input><br>"
             "  <input type='text' id='text_2'></input><br>"
             "  <input type='button' id='button'></input><br>"
             "</form>");
    GetWebWidget()->resize(WebKit::WebSize(500, 500));
    GetWebWidget()->setFocus(true);
    WebKit::WebDocument document = view_->GetWebView()->mainFrame()->document();
    text_ = document.getElementById("text_1");
    ASSERT_FALSE(text_.isNull());
  }

  // Send all the messages required for a complete key press.
  void SendKeyPress(int key_code) {
    WebKit::WebKeyboardEvent keyboard_event;
    keyboard_event.windowsKeyCode = key_code;
    keyboard_event.setKeyIdentifierFromWindowsKeyCode();

    keyboard_event.type = WebKit::WebInputEvent::RawKeyDown;
    SendWebKeyboardEvent(keyboard_event);

    keyboard_event.type = WebKit::WebInputEvent::Char;
    SendWebKeyboardEvent(keyboard_event);

    keyboard_event.type = WebKit::WebInputEvent::KeyUp;
    SendWebKeyboardEvent(keyboard_event);
  }

  scoped_ptr<PageClickTracker>  page_click_tracker_;
  TestPageClickListener test_listener1_;
  TestPageClickListener test_listener2_;
  WebKit::WebElement text_;
};

// Tests that PageClickTracker does notify correctly when a node is clicked.
TEST_F(PageClickTrackerTest, PageClickTrackerInputClicked) {
  // Click the text field once.
  EXPECT_TRUE(SimulateElementClick("text_1"));
  EXPECT_TRUE(test_listener1_.input_element_clicked_called_);
  EXPECT_TRUE(test_listener2_.input_element_clicked_called_);
  EXPECT_FALSE(test_listener1_.was_focused_);
  EXPECT_FALSE(test_listener2_.was_focused_);
  EXPECT_TRUE(test_listener1_.is_focused_);
  EXPECT_TRUE(test_listener2_.is_focused_);
  EXPECT_TRUE(text_ == test_listener1_.element_clicked_);
  EXPECT_TRUE(text_ == test_listener2_.element_clicked_);
  test_listener1_.ClearResults();
  test_listener2_.ClearResults();

  // Click the text field again to test that was_focused_ is set correctly.
  EXPECT_TRUE(SimulateElementClick("text_1"));
  EXPECT_TRUE(test_listener1_.input_element_clicked_called_);
  EXPECT_TRUE(test_listener2_.input_element_clicked_called_);
  EXPECT_TRUE(test_listener1_.was_focused_);
  EXPECT_TRUE(test_listener2_.was_focused_);
  EXPECT_TRUE(test_listener1_.is_focused_);
  EXPECT_TRUE(test_listener2_.is_focused_);
  EXPECT_TRUE(text_ == test_listener1_.element_clicked_);
  EXPECT_TRUE(text_ == test_listener2_.element_clicked_);
  test_listener1_.ClearResults();
  test_listener2_.ClearResults();

  // Click the button, no notification should happen (this is not a text-input).
  EXPECT_TRUE(SimulateElementClick("button"));
  EXPECT_FALSE(test_listener1_.input_element_clicked_called_);
  EXPECT_FALSE(test_listener2_.input_element_clicked_called_);

  // Make the first listener stop the event propagation, click the text field
  // and make sure only the first listener is notified.
  test_listener1_.notification_response_ = true;
  EXPECT_TRUE(SimulateElementClick("text_1"));
  EXPECT_TRUE(test_listener1_.input_element_clicked_called_);
  EXPECT_FALSE(test_listener2_.input_element_clicked_called_);
  test_listener1_.ClearResults();

  // Make sure removing a listener work.
  page_click_tracker_->RemoveListener(&test_listener1_);
  EXPECT_TRUE(SimulateElementClick("text_1"));
  EXPECT_FALSE(test_listener1_.input_element_clicked_called_);
  EXPECT_TRUE(test_listener2_.input_element_clicked_called_);
  test_listener2_.ClearResults();

  // Make sure we don't choke when no listeners are registered.
  page_click_tracker_->RemoveListener(&test_listener2_);
  EXPECT_TRUE(SimulateElementClick("text_1"));
  EXPECT_FALSE(test_listener1_.input_element_clicked_called_);
  EXPECT_FALSE(test_listener2_.input_element_clicked_called_);
}

TEST_F(PageClickTrackerTest, PageClickTrackerInputFocusLost) {
  // Gain focus on the text field by using tab.
  EXPECT_NE(text_, text_.document().focusedNode());
  SendKeyPress(ui::VKEY_TAB);
  EXPECT_EQ(text_, text_.document().focusedNode());
  EXPECT_FALSE(test_listener1_.input_element_lost_focus_called_);
  EXPECT_FALSE(test_listener2_.input_element_lost_focus_called_);

  // Click a button and ensure that the lost focus notification was sent,
  // even though focus was gained without the mouse.
  EXPECT_TRUE(SimulateElementClick("button"));
  EXPECT_TRUE(test_listener1_.input_element_lost_focus_called_);
  EXPECT_TRUE(test_listener2_.input_element_lost_focus_called_);
  test_listener1_.ClearResults();
  test_listener2_.ClearResults();

  // Click a text field and test that no lost focus notifications are sent.
  EXPECT_TRUE(SimulateElementClick("text_1"));
  EXPECT_FALSE(test_listener1_.input_element_lost_focus_called_);
  EXPECT_FALSE(test_listener2_.input_element_lost_focus_called_);
  test_listener1_.ClearResults();
  test_listener2_.ClearResults();

  // Select another text field to test that the notifcation for the
  // first text field losing focus is sent.
  EXPECT_TRUE(SimulateElementClick("text_2"));
  EXPECT_TRUE(test_listener1_.input_element_lost_focus_called_);
  EXPECT_TRUE(test_listener2_.input_element_lost_focus_called_);
  test_listener1_.ClearResults();
  test_listener2_.ClearResults();

  // Click the button, a notification should happen since a text field has
  // lost focus.
  EXPECT_TRUE(SimulateElementClick("button"));
  EXPECT_TRUE(test_listener1_.input_element_lost_focus_called_);
  EXPECT_TRUE(test_listener2_.input_element_lost_focus_called_);
  test_listener1_.ClearResults();
  test_listener2_.ClearResults();

  // Click on a text field while the button has focus and ensure no lost focus
  // notification is sent.
  EXPECT_TRUE(SimulateElementClick("text_1"));
  EXPECT_FALSE(test_listener1_.input_element_lost_focus_called_);
  EXPECT_FALSE(test_listener2_.input_element_lost_focus_called_);

  // Make the first listener stop the event propagation, then click a button
  // and make sure only the first listener is notified.
  test_listener1_.notification_response_ = true;
  EXPECT_TRUE(SimulateElementClick("button"));
  EXPECT_TRUE(test_listener1_.input_element_lost_focus_called_);
  EXPECT_FALSE(test_listener2_.input_element_lost_focus_called_);
  test_listener1_.ClearResults();

  // Make sure removing a listener work.
  page_click_tracker_->RemoveListener(&test_listener1_);
  EXPECT_TRUE(SimulateElementClick("text_1"));
  EXPECT_TRUE(SimulateElementClick("button"));
  EXPECT_FALSE(test_listener1_.input_element_lost_focus_called_);
  EXPECT_TRUE(test_listener2_.input_element_lost_focus_called_);
  test_listener2_.ClearResults();

  // Make sure we don't choke when no listeners are registered.
  page_click_tracker_->RemoveListener(&test_listener2_);
  EXPECT_TRUE(SimulateElementClick("text_1"));
  EXPECT_TRUE(SimulateElementClick("button"));
  EXPECT_FALSE(test_listener1_.input_element_lost_focus_called_);
  EXPECT_FALSE(test_listener2_.input_element_lost_focus_called_);
}
