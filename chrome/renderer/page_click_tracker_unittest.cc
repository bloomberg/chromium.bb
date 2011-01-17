// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"

#include "chrome/common/render_messages.h"
#include "chrome/renderer/page_click_listener.h"
#include "chrome/renderer/page_click_tracker.h"
#include "chrome/test/render_view_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDocument.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebInputElement.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebSize.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebView.h"

class TestPageClickListener : public PageClickListener {
 public:
  TestPageClickListener()
      : called_(false),
        was_focused_(false),
        is_focused_(false),
        notification_response_(false) {
  }

  virtual bool InputElementClicked(const WebKit::WebInputElement& element,
                                   bool was_focused,
                                   bool is_focused) {
    called_ = true;
    element_clicked_ = element;
    was_focused_ = was_focused;
    is_focused_ = is_focused;
    return notification_response_;
  }

  void ClearResults() {
    called_ = false;
    element_clicked_.reset();
    was_focused_ = false;
    is_focused_ = false;
  }

  bool called_;
  WebKit::WebInputElement element_clicked_;
  bool was_focused_;
  bool is_focused_;
  bool notification_response_;
};

// Tests that PageClickTracker does notify correctly when a node is clicked.
TEST_F(RenderViewTest, PageClickTracker) {
  // RenderView creates PageClickTracker but it doesn't keep it around.  Rather
  // than make it do so for the test, we create a new object.
  PageClickTracker* page_click_tracker = new PageClickTracker(view_);

  TestPageClickListener test_listener1;
  TestPageClickListener test_listener2;
  page_click_tracker->AddListener(&test_listener1);
  page_click_tracker->AddListener(&test_listener2);

  LoadHTML("<form>"
           "  <input type='text' id='text'></input><br>"
           "  <input type='button' id='button'></input><br>"
           "</form>");
  view_->webwidget()->resize(WebKit::WebSize(500, 500));
  view_->webwidget()->setFocus(true);
  WebKit::WebDocument document = view_->webview()->mainFrame()->document();
  WebKit::WebElement text = document.getElementById("text");
  ASSERT_FALSE(text.isNull());
  WebKit::WebElement button = document.getElementById("button");
  ASSERT_FALSE(button.isNull());

  // Click the text field once.
  EXPECT_TRUE(SimulateElementClick("text"));
  EXPECT_TRUE(test_listener1.called_);
  EXPECT_TRUE(test_listener2.called_);
  EXPECT_FALSE(test_listener1.was_focused_);
  EXPECT_FALSE(test_listener2.was_focused_);
  EXPECT_TRUE(test_listener1.is_focused_);
  EXPECT_TRUE(test_listener2.is_focused_);
  EXPECT_TRUE(text == test_listener1.element_clicked_);
  EXPECT_TRUE(text == test_listener2.element_clicked_);
  test_listener1.ClearResults();
  test_listener2.ClearResults();

  // Click the text field again to test that was_focused_ is set correctly.
  EXPECT_TRUE(SimulateElementClick("text"));
  EXPECT_TRUE(test_listener1.called_);
  EXPECT_TRUE(test_listener2.called_);
  EXPECT_TRUE(test_listener1.was_focused_);
  EXPECT_TRUE(test_listener2.was_focused_);
  EXPECT_TRUE(test_listener1.is_focused_);
  EXPECT_TRUE(test_listener2.is_focused_);
  EXPECT_TRUE(text == test_listener1.element_clicked_);
  EXPECT_TRUE(text == test_listener2.element_clicked_);
  test_listener1.ClearResults();
  test_listener2.ClearResults();

  // Click the button, no notification should happen (this is not a text-input).
  EXPECT_TRUE(SimulateElementClick("button"));
  EXPECT_FALSE(test_listener1.called_);
  EXPECT_FALSE(test_listener2.called_);

  // Make the first listener stop the event propagation, click the text field
  // and make sure only the first listener is notified.
  test_listener1.notification_response_ = true;
  EXPECT_TRUE(SimulateElementClick("text"));
  EXPECT_TRUE(test_listener1.called_);
  EXPECT_FALSE(test_listener2.called_);
  test_listener1.ClearResults();

  // Make sure removing a listener work.
  page_click_tracker->RemoveListener(&test_listener1);
  EXPECT_TRUE(SimulateElementClick("text"));
  EXPECT_FALSE(test_listener1.called_);
  EXPECT_TRUE(test_listener2.called_);
  test_listener2.ClearResults();

  // Make sure we don't choke when no listeners are registered.
  page_click_tracker->RemoveListener(&test_listener2);
  EXPECT_TRUE(SimulateElementClick("text"));
  EXPECT_FALSE(test_listener1.called_);
  EXPECT_FALSE(test_listener2.called_);
}
