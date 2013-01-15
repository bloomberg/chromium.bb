// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/utf_string_conversions.h"
#include "content/common/accessibility_messages.h"
#include "content/common/accessibility_node_data.h"
#include "content/common/view_messages.h"
#include "content/renderer/render_view_impl.h"
#include "content/public/test/render_view_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebSize.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebView.h"

namespace content {

class RendererAccessibilityTest : public RenderViewTest {
 public:
  RendererAccessibilityTest() {}

  RenderViewImpl* view() {
    return static_cast<RenderViewImpl*>(view_);
  }

  virtual void SetUp() {
    RenderViewTest::SetUp();
    sink_ = &render_thread_->sink();
  }

  void SetMode(AccessibilityMode mode) {
    view()->OnSetAccessibilityMode(mode);
  }

  void GetLastAccNotification(
      AccessibilityHostMsg_NotificationParams* params) {
    const IPC::Message* message =
        sink_->GetUniqueMessageMatching(AccessibilityHostMsg_Notifications::ID);
    ASSERT_TRUE(message);
    Tuple1<std::vector<AccessibilityHostMsg_NotificationParams> > param;
    AccessibilityHostMsg_Notifications::Read(message, &param);
    ASSERT_EQ(param.a.size(), 1U);
    *params = param.a[0];
  }

 protected:
  IPC::TestSink* sink_;

  DISALLOW_COPY_AND_ASSIGN(RendererAccessibilityTest);
};

TEST_F(RendererAccessibilityTest, EditableTextModeFocusNotifications) {
  SetMode(AccessibilityModeEditableTextOnly);

  // Set a minimum size and give focus so simulated events work.
  view()->webwidget()->resize(WebKit::WebSize(500, 500));
  view()->webwidget()->setFocus(true);

  std::string html =
      "<body>"
      "  <input>"
      "  <textarea></textarea>"
      "  <p contentEditable>Editable</p>"
      "  <div tabindex=0 role=textbox>Textbox</div>"
      "  <button>Button</button>"
      "  <a href=#>Link</a>"
      "</body>";

  // Load the test page.
  LoadHTML(html.c_str());

  // We should have sent a message to the browser with the initial focus
  // on the document.
  {
    SCOPED_TRACE("Initial focus on document");
    AccessibilityHostMsg_NotificationParams notification;
    GetLastAccNotification(&notification);
    EXPECT_EQ(notification.notification_type,
              AccessibilityNotificationLayoutComplete);
    EXPECT_EQ(notification.includes_children, true);
    EXPECT_EQ(notification.id, 1);
    EXPECT_EQ(notification.acc_tree.id, 1);
    EXPECT_EQ(notification.acc_tree.role,
              AccessibilityNodeData::ROLE_ROOT_WEB_AREA);
    EXPECT_EQ(notification.acc_tree.state,
              (1U << AccessibilityNodeData::STATE_READONLY) |
              (1U << AccessibilityNodeData::STATE_FOCUSABLE) |
              (1U << AccessibilityNodeData::STATE_FOCUSED));
    EXPECT_EQ(notification.acc_tree.children.size(), 1U);
  }

  // Now focus the input element, and check everything again.
  {
    SCOPED_TRACE("input");
    sink_->ClearMessages();
    ExecuteJavaScript("document.querySelector('input').focus();");
    AccessibilityHostMsg_NotificationParams notification;
    GetLastAccNotification(&notification);
    EXPECT_EQ(notification.notification_type,
              AccessibilityNotificationFocusChanged);
    EXPECT_EQ(notification.includes_children, true);
    EXPECT_EQ(notification.id, 3);
    EXPECT_EQ(notification.acc_tree.id, 1);
    EXPECT_EQ(notification.acc_tree.role,
              AccessibilityNodeData::ROLE_ROOT_WEB_AREA);
    EXPECT_EQ(notification.acc_tree.state,
              (1U << AccessibilityNodeData::STATE_READONLY) |
              (1U << AccessibilityNodeData::STATE_FOCUSABLE));
    EXPECT_EQ(notification.acc_tree.children.size(), 1U);
    EXPECT_EQ(notification.acc_tree.children[0].id, 3);
    EXPECT_EQ(notification.acc_tree.children[0].role,
              AccessibilityNodeData::ROLE_GROUP);
    EXPECT_EQ(notification.acc_tree.children[0].state,
              (1U << AccessibilityNodeData::STATE_FOCUSABLE) |
              (1U << AccessibilityNodeData::STATE_FOCUSED));
  }

  // Check other editable text nodes.
  {
    SCOPED_TRACE("textarea");
    sink_->ClearMessages();
    ExecuteJavaScript("document.querySelector('textarea').focus();");
    AccessibilityHostMsg_NotificationParams notification;
    GetLastAccNotification(&notification);
    EXPECT_EQ(notification.id, 4);
    EXPECT_EQ(notification.acc_tree.children[0].state,
              (1U << AccessibilityNodeData::STATE_FOCUSABLE) |
              (1U << AccessibilityNodeData::STATE_FOCUSED));
  }

  {
    SCOPED_TRACE("contentEditable");
    sink_->ClearMessages();
    ExecuteJavaScript("document.querySelector('p').focus();");
    AccessibilityHostMsg_NotificationParams notification;
    GetLastAccNotification(&notification);
    EXPECT_EQ(notification.id, 5);
    EXPECT_EQ(notification.acc_tree.children[0].state,
              (1U << AccessibilityNodeData::STATE_FOCUSABLE) |
              (1U << AccessibilityNodeData::STATE_FOCUSED));
  }

  {
    SCOPED_TRACE("role=textarea");
    sink_->ClearMessages();
    ExecuteJavaScript("document.querySelector('div').focus();");
    AccessibilityHostMsg_NotificationParams notification;
    GetLastAccNotification(&notification);
    EXPECT_EQ(notification.id, 6);
    EXPECT_EQ(notification.acc_tree.children[0].state,
              (1U << AccessibilityNodeData::STATE_FOCUSABLE) |
              (1U << AccessibilityNodeData::STATE_FOCUSED));
  }

  // Try focusing things that aren't editable text.
  {
    SCOPED_TRACE("button");
    sink_->ClearMessages();
    ExecuteJavaScript("document.querySelector('button').focus();");
    AccessibilityHostMsg_NotificationParams notification;
    GetLastAccNotification(&notification);
    EXPECT_EQ(notification.id, 7);
    EXPECT_EQ(notification.acc_tree.children[0].state,
              (1U << AccessibilityNodeData::STATE_FOCUSABLE) |
              (1U << AccessibilityNodeData::STATE_FOCUSED) |
              (1U << AccessibilityNodeData::STATE_READONLY));
  }

  {
    SCOPED_TRACE("link");
    sink_->ClearMessages();
    ExecuteJavaScript("document.querySelector('a').focus();");
    AccessibilityHostMsg_NotificationParams notification;
    GetLastAccNotification(&notification);
    EXPECT_EQ(notification.id, 8);
    EXPECT_EQ(notification.acc_tree.children[0].state,
              (1U << AccessibilityNodeData::STATE_FOCUSABLE) |
              (1U << AccessibilityNodeData::STATE_FOCUSED) |
              (1U << AccessibilityNodeData::STATE_READONLY));
  }

  // Clear focus.
  {
    SCOPED_TRACE("Back to document.");
    sink_->ClearMessages();
    ExecuteJavaScript("document.activeElement.blur()");
    AccessibilityHostMsg_NotificationParams notification;
    GetLastAccNotification(&notification);
    EXPECT_EQ(notification.id, 1);
  }
}

}  // namespace content
