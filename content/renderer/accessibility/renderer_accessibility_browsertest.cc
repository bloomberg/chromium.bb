// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/utf_string_conversions.h"
#include "content/common/accessibility_node_data.h"
#include "content/common/view_messages.h"
#include "content/public/test/render_view_test.h"
#include "content/renderer/accessibility/renderer_accessibility_complete.h"
#include "content/renderer/render_view_impl.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebSize.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebAccessibilityObject.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDocument.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebView.h"

using WebKit::WebAccessibilityObject;
using WebKit::WebDocument;

namespace content {

class TestRendererAccessibilityComplete : public RendererAccessibilityComplete {
 public:
  explicit TestRendererAccessibilityComplete(RenderViewImpl* render_view)
    : RendererAccessibilityComplete(render_view),
      browser_tree_node_count_(0) {
  }

  int browser_tree_node_count() { return browser_tree_node_count_; }

  struct TestBrowserTreeNode : public BrowserTreeNode {
    TestBrowserTreeNode(TestRendererAccessibilityComplete* owner)
        : owner_(owner) {
      owner_->browser_tree_node_count_++;
    }

    virtual ~TestBrowserTreeNode() {
      owner_->browser_tree_node_count_--;
    }

   private:
    TestRendererAccessibilityComplete* owner_;
  };

  virtual BrowserTreeNode* CreateBrowserTreeNode() OVERRIDE {
    return new TestBrowserTreeNode(this);
  }

  void SendPendingAccessibilityNotifications() {
    RendererAccessibilityComplete::SendPendingAccessibilityNotifications();
  }

private:
  int browser_tree_node_count_;
};

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
    ASSERT_GE(param.a.size(), 1U);
    *params = param.a[0];
  }

  int CountAccessibilityNodesSentToBrowser() {
    AccessibilityHostMsg_NotificationParams notification;
    GetLastAccNotification(&notification);
    return notification.nodes.size();
  }

 protected:
  IPC::TestSink* sink_;

  DISALLOW_COPY_AND_ASSIGN(RendererAccessibilityTest);
};

TEST_F(RendererAccessibilityTest, EditableTextModeFocusNotifications) {
  // This is not a test of true web accessibility, it's a test of
  // a mode used on Windows 8 in Metro mode where an extremely simplified
  // accessibility tree containing only the current focused node is
  // generated.
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
    EXPECT_EQ(notification.id, 1);
    EXPECT_EQ(notification.nodes.size(), 2U);
    EXPECT_EQ(notification.nodes[0].id, 1);
    EXPECT_EQ(notification.nodes[0].role,
              AccessibilityNodeData::ROLE_ROOT_WEB_AREA);
    EXPECT_EQ(notification.nodes[0].state,
              (1U << AccessibilityNodeData::STATE_READONLY) |
              (1U << AccessibilityNodeData::STATE_FOCUSABLE) |
              (1U << AccessibilityNodeData::STATE_FOCUSED));
    EXPECT_EQ(notification.nodes[0].child_ids.size(), 1U);
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
    EXPECT_EQ(notification.id, 3);
    EXPECT_EQ(notification.nodes[0].id, 1);
    EXPECT_EQ(notification.nodes[0].role,
              AccessibilityNodeData::ROLE_ROOT_WEB_AREA);
    EXPECT_EQ(notification.nodes[0].state,
              (1U << AccessibilityNodeData::STATE_READONLY) |
              (1U << AccessibilityNodeData::STATE_FOCUSABLE));
    EXPECT_EQ(notification.nodes[0].child_ids.size(), 1U);
    EXPECT_EQ(notification.nodes[1].id, 3);
    EXPECT_EQ(notification.nodes[1].role,
              AccessibilityNodeData::ROLE_GROUP);
    EXPECT_EQ(notification.nodes[1].state,
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
    EXPECT_EQ(notification.nodes[1].state,
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
    EXPECT_EQ(notification.nodes[1].state,
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
    EXPECT_EQ(notification.nodes[1].state,
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
    EXPECT_EQ(notification.nodes[1].state,
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
    EXPECT_EQ(notification.nodes[1].state,
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

TEST_F(RendererAccessibilityTest, SendFullAccessibilityTreeOnReload) {
  // The job of RendererAccessibilityComplete is to serialize the
  // accessibility tree built by WebKit and send it to the browser.
  // When the accessibility tree changes, it tries to send only
  // the nodes that actually changed or were reparented. This test
  // ensures that the messages sent are correct in cases when a page
  // reloads, and that internal state is properly garbage-collected.
  std::string html =
      "<body>"
      "  <div role='group' id='A'>"
      "    <div role='group' id='A1'></div>"
      "    <div role='group' id='A2'></div>"
      "  </div>"
      "</body>";
  LoadHTML(html.c_str());

  // Creating a RendererAccessibilityComplete should sent the tree
  // to the browser.
  scoped_ptr<TestRendererAccessibilityComplete> accessibility(
      new TestRendererAccessibilityComplete(view()));
  accessibility->SendPendingAccessibilityNotifications();
  EXPECT_EQ(4, accessibility->browser_tree_node_count());
  EXPECT_EQ(4, CountAccessibilityNodesSentToBrowser());

  // If we post another notification but the tree doesn't change,
  // we should only send 1 node to the browser.
  sink_->ClearMessages();
  WebDocument document = view()->GetWebView()->mainFrame()->document();
  WebAccessibilityObject root_obj = document.accessibilityObject();
  accessibility->HandleWebAccessibilityNotification(
      root_obj,
      WebKit::WebAccessibilityNotificationLayoutComplete);
  accessibility->SendPendingAccessibilityNotifications();
  EXPECT_EQ(4, accessibility->browser_tree_node_count());
  EXPECT_EQ(1, CountAccessibilityNodesSentToBrowser());
  {
    // Make sure it's the root object that was updated.
    AccessibilityHostMsg_NotificationParams notification;
    GetLastAccNotification(&notification);
    EXPECT_EQ(root_obj.axID(), notification.nodes[0].id);
  }

  // If we reload the page and send a notification, we should send
  // all 4 nodes to the browser. Also double-check that we didn't
  // leak any of the old BrowserTreeNodes.
  LoadHTML(html.c_str());
  document = view()->GetWebView()->mainFrame()->document();
  root_obj = document.accessibilityObject();
  sink_->ClearMessages();
  accessibility->HandleWebAccessibilityNotification(
      root_obj,
      WebKit::WebAccessibilityNotificationLayoutComplete);
  accessibility->SendPendingAccessibilityNotifications();
  EXPECT_EQ(4, accessibility->browser_tree_node_count());
  EXPECT_EQ(4, CountAccessibilityNodesSentToBrowser());

  // Even if the first notification is sent on an element other than
  // the root, the whole tree should be updated because we know
  // the browser doesn't have the root element.
  LoadHTML(html.c_str());
  document = view()->GetWebView()->mainFrame()->document();
  root_obj = document.accessibilityObject();
  sink_->ClearMessages();
  const WebAccessibilityObject& first_child = root_obj.firstChild();
  accessibility->HandleWebAccessibilityNotification(
      first_child,
      WebKit::WebAccessibilityNotificationLiveRegionChanged);
  accessibility->SendPendingAccessibilityNotifications();
  EXPECT_EQ(4, accessibility->browser_tree_node_count());
  EXPECT_EQ(4, CountAccessibilityNodesSentToBrowser());
}

TEST_F(RendererAccessibilityTest, HideAccessibilityObject) {
  // Test RendererAccessibilityComplete and make sure it sends the
  // proper notification to the browser when an object in the tree
  // is hidden, but its children are not.
  std::string html =
      "<body>"
      "  <div role='group' id='A'>"
      "    <div role='group' id='B'>"
      "      <div role='group' id='C' style='visibility:visible'>"
      "      </div>"
      "    </div>"
      "  </div>"
      "</body>";
  LoadHTML(html.c_str());

  scoped_ptr<TestRendererAccessibilityComplete> accessibility(
      new TestRendererAccessibilityComplete(view()));
  accessibility->SendPendingAccessibilityNotifications();
  EXPECT_EQ(4, accessibility->browser_tree_node_count());
  EXPECT_EQ(4, CountAccessibilityNodesSentToBrowser());

  WebDocument document = view()->GetWebView()->mainFrame()->document();
  WebAccessibilityObject root_obj = document.accessibilityObject();
  WebAccessibilityObject node_a = root_obj.childAt(0);
  WebAccessibilityObject node_b = node_a.childAt(0);
  WebAccessibilityObject node_c = node_b.childAt(0);

  // Hide node 'B' ('C' stays visible).
  ExecuteJavaScript(
      "document.getElementById('B').style.visibility = 'hidden';");
  // Force layout now.
  ExecuteJavaScript("document.getElementById('B').offsetLeft;");

  // Send a childrenChanged on 'A'.
  sink_->ClearMessages();
  accessibility->HandleWebAccessibilityNotification(
      node_a,
      WebKit::WebAccessibilityNotificationChildrenChanged);

  accessibility->SendPendingAccessibilityNotifications();
  EXPECT_EQ(3, accessibility->browser_tree_node_count());
  AccessibilityHostMsg_NotificationParams notification;
  GetLastAccNotification(&notification);
  ASSERT_EQ(3U, notification.nodes.size());

  // RendererAccessibilityComplete notices that 'C' is being reparented,
  // so it updates 'B' first to remove 'C' as a child, then 'A' to add it,
  // and finally it updates 'C'.
  EXPECT_EQ(node_b.axID(), notification.nodes[0].id);
  EXPECT_EQ(node_a.axID(), notification.nodes[1].id);
  EXPECT_EQ(node_c.axID(), notification.nodes[2].id);
  EXPECT_EQ(3, CountAccessibilityNodesSentToBrowser());
}

TEST_F(RendererAccessibilityTest, ShowAccessibilityObject) {
  // Test RendererAccessibilityComplete and make sure it sends the
  // proper notification to the browser when an object in the tree
  // is shown, causing its own already-visible children to be
  // reparented to it.
  std::string html =
      "<body>"
      "  <div role='group' id='A'>"
      "    <div role='group' id='B' style='visibility:hidden'>"
      "      <div role='group' id='C' style='visibility:visible'>"
      "      </div>"
      "    </div>"
      "  </div>"
      "</body>";
  LoadHTML(html.c_str());

  scoped_ptr<TestRendererAccessibilityComplete> accessibility(
      new TestRendererAccessibilityComplete(view()));
  accessibility->SendPendingAccessibilityNotifications();
  EXPECT_EQ(3, accessibility->browser_tree_node_count());
  EXPECT_EQ(3, CountAccessibilityNodesSentToBrowser());

  // Show node 'B', then send a childrenChanged on 'A'.
  ExecuteJavaScript(
      "document.getElementById('B').style.visibility = 'visible';");
  ExecuteJavaScript("document.getElementById('B').offsetLeft;");

  sink_->ClearMessages();
  WebDocument document = view()->GetWebView()->mainFrame()->document();
  WebAccessibilityObject root_obj = document.accessibilityObject();
  WebAccessibilityObject node_a = root_obj.childAt(0);
  accessibility->HandleWebAccessibilityNotification(
      node_a,
      WebKit::WebAccessibilityNotificationChildrenChanged);

  accessibility->SendPendingAccessibilityNotifications();
  EXPECT_EQ(4, accessibility->browser_tree_node_count());
  AccessibilityHostMsg_NotificationParams notification;
  GetLastAccNotification(&notification);
  ASSERT_EQ(3U, notification.nodes.size());
  EXPECT_EQ(3, CountAccessibilityNodesSentToBrowser());
}

}  // namespace content
