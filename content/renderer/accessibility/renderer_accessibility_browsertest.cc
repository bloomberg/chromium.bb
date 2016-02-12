// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "content/common/accessibility_messages.h"
#include "content/common/frame_messages.h"
#include "content/common/site_isolation_policy.h"
#include "content/common/view_message_enums.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/render_view_test.h"
#include "content/renderer/accessibility/renderer_accessibility.h"
#include "content/renderer/render_frame_impl.h"
#include "content/renderer/render_view_impl.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/platform/WebSize.h"
#include "third_party/WebKit/public/web/WebAXObject.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebView.h"
#include "ui/accessibility/ax_node_data.h"

using blink::WebAXObject;
using blink::WebDocument;

namespace content {

class TestRendererAccessibility : public RendererAccessibility {
 public:
  explicit TestRendererAccessibility(RenderFrameImpl* render_frame)
    : RendererAccessibility(render_frame) {
  }

  void SendPendingAccessibilityEvents() {
    RendererAccessibility::SendPendingAccessibilityEvents();
  }
};

class RendererAccessibilityTest : public RenderViewTest {
 public:
  RendererAccessibilityTest() {}

  RenderViewImpl* view() {
    return static_cast<RenderViewImpl*>(view_);
  }

  RenderFrameImpl* frame() {
    return static_cast<RenderFrameImpl*>(view()->GetMainRenderFrame());
  }

  void SetUp() override {
    RenderViewTest::SetUp();
    sink_ = &render_thread_->sink();
  }

  void TearDown() override {
#if defined(LEAK_SANITIZER)
     // Do this before shutting down V8 in RenderViewTest::TearDown().
     // http://crbug.com/328552
     __lsan_do_leak_check();
#endif
     RenderViewTest::TearDown();
  }

  void SetMode(AccessibilityMode mode) {
    frame()->OnSetAccessibilityMode(mode);
  }

  void GetAllAccEvents(
      std::vector<AccessibilityHostMsg_EventParams>* param_list) {
    const IPC::Message* message =
        sink_->GetUniqueMessageMatching(AccessibilityHostMsg_Events::ID);
    ASSERT_TRUE(message);
    base::Tuple<std::vector<AccessibilityHostMsg_EventParams>, int> param;
    AccessibilityHostMsg_Events::Read(message, &param);
    *param_list = base::get<0>(param);
  }

  void GetLastAccEvent(
      AccessibilityHostMsg_EventParams* params) {
    std::vector<AccessibilityHostMsg_EventParams> param_list;
    GetAllAccEvents(&param_list);
    ASSERT_GE(param_list.size(), 1U);
    *params = param_list[0];
  }

  int CountAccessibilityNodesSentToBrowser() {
    AccessibilityHostMsg_EventParams event;
    GetLastAccEvent(&event);
    return event.update.nodes.size();
  }

 protected:
  IPC::TestSink* sink_;

  DISALLOW_COPY_AND_ASSIGN(RendererAccessibilityTest);

};

TEST_F(RendererAccessibilityTest, SendFullAccessibilityTreeOnReload) {
  // The job of RendererAccessibility is to serialize the
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

  // Creating a RendererAccessibility should sent the tree to the browser.
  scoped_ptr<TestRendererAccessibility> accessibility(
      new TestRendererAccessibility(frame()));
  accessibility->SendPendingAccessibilityEvents();
  EXPECT_EQ(4, CountAccessibilityNodesSentToBrowser());

  // If we post another event but the tree doesn't change,
  // we should only send 1 node to the browser.
  sink_->ClearMessages();
  WebDocument document = view()->GetWebView()->mainFrame()->document();
  WebAXObject root_obj = document.accessibilityObject();
  accessibility->HandleAXEvent(
      root_obj,
      ui::AX_EVENT_LAYOUT_COMPLETE);
  accessibility->SendPendingAccessibilityEvents();
  EXPECT_EQ(1, CountAccessibilityNodesSentToBrowser());
  {
    // Make sure it's the root object that was updated.
    AccessibilityHostMsg_EventParams event;
    GetLastAccEvent(&event);
    EXPECT_EQ(root_obj.axID(), event.update.nodes[0].id);
  }

  // If we reload the page and send a event, we should send
  // all 4 nodes to the browser. Also double-check that we didn't
  // leak any of the old BrowserTreeNodes.
  LoadHTML(html.c_str());
  document = view()->GetWebView()->mainFrame()->document();
  root_obj = document.accessibilityObject();
  sink_->ClearMessages();
  accessibility->HandleAXEvent(
      root_obj,
      ui::AX_EVENT_LAYOUT_COMPLETE);
  accessibility->SendPendingAccessibilityEvents();
  EXPECT_EQ(4, CountAccessibilityNodesSentToBrowser());

  // Even if the first event is sent on an element other than
  // the root, the whole tree should be updated because we know
  // the browser doesn't have the root element.
  LoadHTML(html.c_str());
  document = view()->GetWebView()->mainFrame()->document();
  root_obj = document.accessibilityObject();
  sink_->ClearMessages();
  const WebAXObject& first_child = root_obj.childAt(0);
  accessibility->HandleAXEvent(
      first_child,
      ui::AX_EVENT_LIVE_REGION_CHANGED);
  accessibility->SendPendingAccessibilityEvents();
  EXPECT_EQ(4, CountAccessibilityNodesSentToBrowser());
}

// http://crbug.com/253537
#if defined(OS_ANDROID)
#define MAYBE_AccessibilityMessagesQueueWhileSwappedOut \
        DISABLED_AccessibilityMessagesQueueWhileSwappedOut
#else
#define MAYBE_AccessibilityMessagesQueueWhileSwappedOut \
        AccessibilityMessagesQueueWhileSwappedOut
#endif

TEST_F(RendererAccessibilityTest,
       MAYBE_AccessibilityMessagesQueueWhileSwappedOut) {
  // This test breaks down in --site-per-process, as swapping out destroys
  // the main frame and it cannot be further navigated.
  // TODO(nasko): Figure out what this behavior looks like when swapped out
  // no longer exists.
  if (SiteIsolationPolicy::IsSwappedOutStateForbidden()) {
    return;
  }
  std::string html =
      "<body>"
      "  <p>Hello, world.</p>"
      "</body>";
  LoadHTML(html.c_str());
  static const int kProxyRoutingId = 13;

  // Creating a RendererAccessibility should send the tree to the browser.
  scoped_ptr<TestRendererAccessibility> accessibility(
      new TestRendererAccessibility(frame()));
  accessibility->SendPendingAccessibilityEvents();
  EXPECT_EQ(5, CountAccessibilityNodesSentToBrowser());

  // Post a "value changed" event, but then swap out
  // before sending it. It shouldn't send the event while
  // swapped out.
  sink_->ClearMessages();
  WebDocument document = view()->GetWebView()->mainFrame()->document();
  WebAXObject root_obj = document.accessibilityObject();
  accessibility->HandleAXEvent(
      root_obj,
      ui::AX_EVENT_VALUE_CHANGED);
  view()->GetMainRenderFrame()->OnSwapOut(kProxyRoutingId, true,
                                          content::FrameReplicationState());
  accessibility->SendPendingAccessibilityEvents();
  EXPECT_FALSE(sink_->GetUniqueMessageMatching(
      AccessibilityHostMsg_Events::ID));

  // Navigate, so we're not swapped out anymore. Now we should
  // send accessibility events again. Note that the
  // message that was queued up before will be quickly discarded
  // because the element it was referring to no longer exists,
  // so the event here is from loading this new page.
  CommonNavigationParams common_params;
  RequestNavigationParams request_params;
  common_params.url = GURL("data:text/html,<p>Hello, again.</p>");
  common_params.navigation_type = FrameMsg_Navigate_Type::NORMAL;
  common_params.transition = ui::PAGE_TRANSITION_TYPED;
  request_params.current_history_list_length = 1;
  request_params.current_history_list_offset = 0;
  request_params.pending_history_list_offset = 1;
  request_params.page_id = -1;
  frame()->OnNavigate(common_params, StartNavigationParams(), request_params);
  accessibility->SendPendingAccessibilityEvents();
  EXPECT_TRUE(sink_->GetUniqueMessageMatching(
      AccessibilityHostMsg_Events::ID));
}

TEST_F(RendererAccessibilityTest, HideAccessibilityObject) {
  // Test RendererAccessibility and make sure it sends the
  // proper event to the browser when an object in the tree
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

  scoped_ptr<TestRendererAccessibility> accessibility(
      new TestRendererAccessibility(frame()));
  accessibility->SendPendingAccessibilityEvents();
  EXPECT_EQ(4, CountAccessibilityNodesSentToBrowser());

  WebDocument document = view()->GetWebView()->mainFrame()->document();
  WebAXObject root_obj = document.accessibilityObject();
  WebAXObject node_a = root_obj.childAt(0);
  WebAXObject node_b = node_a.childAt(0);
  WebAXObject node_c = node_b.childAt(0);

  // Hide node 'B' ('C' stays visible).
  ExecuteJavaScriptForTests(
      "document.getElementById('B').style.visibility = 'hidden';");
  // Force layout now.
  ExecuteJavaScriptForTests("document.getElementById('B').offsetLeft;");

  // Send a childrenChanged on 'A'.
  sink_->ClearMessages();
  accessibility->HandleAXEvent(
      node_a,
      ui::AX_EVENT_CHILDREN_CHANGED);

  accessibility->SendPendingAccessibilityEvents();
  AccessibilityHostMsg_EventParams event;
  GetLastAccEvent(&event);
  ASSERT_EQ(2U, event.update.nodes.size());

  // RendererAccessibility notices that 'C' is being reparented,
  // so it clears the subtree rooted at 'A', then updates 'A' and then 'C'.
  EXPECT_EQ(node_a.axID(), event.update.node_id_to_clear);
  EXPECT_EQ(node_a.axID(), event.update.nodes[0].id);
  EXPECT_EQ(node_c.axID(), event.update.nodes[1].id);
  EXPECT_EQ(2, CountAccessibilityNodesSentToBrowser());
}

TEST_F(RendererAccessibilityTest, ShowAccessibilityObject) {
  // Test RendererAccessibility and make sure it sends the
  // proper event to the browser when an object in the tree
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

  scoped_ptr<TestRendererAccessibility> accessibility(
      new TestRendererAccessibility(frame()));
  accessibility->SendPendingAccessibilityEvents();
  EXPECT_EQ(3, CountAccessibilityNodesSentToBrowser());

  // Show node 'B', then send a childrenChanged on 'A'.
  ExecuteJavaScriptForTests(
      "document.getElementById('B').style.visibility = 'visible';");
  ExecuteJavaScriptForTests("document.getElementById('B').offsetLeft;");

  sink_->ClearMessages();
  WebDocument document = view()->GetWebView()->mainFrame()->document();
  WebAXObject root_obj = document.accessibilityObject();
  WebAXObject node_a = root_obj.childAt(0);
  WebAXObject node_b = node_a.childAt(0);
  WebAXObject node_c = node_b.childAt(0);

  accessibility->HandleAXEvent(
      node_a,
      ui::AX_EVENT_CHILDREN_CHANGED);

  accessibility->SendPendingAccessibilityEvents();
  AccessibilityHostMsg_EventParams event;
  GetLastAccEvent(&event);

  ASSERT_EQ(3U, event.update.nodes.size());
  EXPECT_EQ(node_a.axID(), event.update.node_id_to_clear);
  EXPECT_EQ(node_a.axID(), event.update.nodes[0].id);
  EXPECT_EQ(node_b.axID(), event.update.nodes[1].id);
  EXPECT_EQ(node_c.axID(), event.update.nodes[2].id);
  EXPECT_EQ(3, CountAccessibilityNodesSentToBrowser());
}

TEST_F(RendererAccessibilityTest, DetachAccessibilityObject) {
  // Test RendererAccessibility and make sure it sends the
  // proper event to the browser when an object in the tree
  // is detached, but its children are not. This can happen when
  // a layout occurs and an anonymous render block is no longer needed.
  std::string html =
      "<body aria-label='Body'>"
      "<span>1</span><span style='display:block'>2</span>"
      "</body>";
  LoadHTML(html.c_str());

  scoped_ptr<TestRendererAccessibility> accessibility(
      new TestRendererAccessibility(frame()));
  accessibility->SendPendingAccessibilityEvents();
  EXPECT_EQ(7, CountAccessibilityNodesSentToBrowser());

  // Initially, the accessibility tree looks like this:
  //
  //   Document
  //   +--Body
  //      +--Anonymous Block
  //         +--Static Text "1"
  //            +--Inline Text Box "1"
  //      +--Static Text "2"
  //         +--Inline Text Box "2"
  WebDocument document = view()->GetWebView()->mainFrame()->document();
  WebAXObject root_obj = document.accessibilityObject();
  WebAXObject body = root_obj.childAt(0);
  WebAXObject anonymous_block = body.childAt(0);
  WebAXObject text_1 = anonymous_block.childAt(0);
  WebAXObject text_2 = body.childAt(1);

  // Change the display of the second 'span' back to inline, which causes the
  // anonymous block to be destroyed.
  ExecuteJavaScriptForTests(
      "document.querySelectorAll('span')[1].style.display = 'inline';");
  // Force layout now.
  ExecuteJavaScriptForTests("document.body.offsetLeft;");

  // Send a childrenChanged on the body.
  sink_->ClearMessages();
  accessibility->HandleAXEvent(
      body,
      ui::AX_EVENT_CHILDREN_CHANGED);

  accessibility->SendPendingAccessibilityEvents();

  // Afterwards, the accessibility tree looks like this:
  //
  //   Document
  //   +--Body
  //      +--Static Text "1"
  //         +--Inline Text Box "1"
  //      +--Static Text "2"
  //         +--Inline Text Box "2"
  //
  // We just assert that there are now four nodes in the
  // accessibility tree and that only three nodes needed
  // to be updated (the body, the static text 1, and
  // the static text 2).

  AccessibilityHostMsg_EventParams event;
  GetLastAccEvent(&event);
  ASSERT_EQ(5U, event.update.nodes.size());

  EXPECT_EQ(body.axID(), event.update.nodes[0].id);
  EXPECT_EQ(text_1.axID(), event.update.nodes[1].id);
  // The third event is to update text_2, but its id changes
  // so we don't have a test expectation for it.
}

TEST_F(RendererAccessibilityTest, EventOnDetachedNodeTriggersMainFrameLayout) {
  std::string html =
      "<body>"
      "  <iframe srcdoc='<input>'></iframe>"
      "  <button>Button</button>"
      "</body>";
  LoadHTML(html.c_str());

  scoped_ptr<TestRendererAccessibility> accessibility(
      new TestRendererAccessibility(frame()));
  accessibility->SendPendingAccessibilityEvents();

  WebDocument document = view()->GetWebView()->mainFrame()->document();
  WebAXObject root_obj = document.accessibilityObject();
  ASSERT_EQ(blink::WebAXRoleWebArea, root_obj.role());
  WebAXObject group = root_obj.childAt(0);
  ASSERT_EQ(blink::WebAXRoleGroup, group.role());
  WebAXObject iframe = group.childAt(0);
  ASSERT_EQ(blink::WebAXRoleIframe, iframe.role());
  WebAXObject child_doc = iframe.childAt(0);
  ASSERT_EQ(blink::WebAXRoleWebArea, child_doc.role());
  WebAXObject child_group = child_doc.childAt(0);
  ASSERT_EQ(blink::WebAXRoleGroup, child_group.role());
  WebAXObject child_textfield = child_group.childAt(0);
  ASSERT_EQ(blink::WebAXRoleTextField, child_textfield.role());

  // Hide the input element inside the iframe.
  ExecuteJavaScriptForTests(
      "document.querySelector('iframe').contentDocument"
      ".querySelector('input').style.display = 'none';");
  accessibility->HandleAXEvent(
      child_textfield,
      ui::AX_EVENT_LAYOUT_COMPLETE);
  accessibility->SendPendingAccessibilityEvents();

  // Make sure |child_textfield| is invalid now.
  ASSERT_TRUE(child_textfield.isDetached());

  // Now do a random style change in the iframe to make its layout not clean.
  ExecuteJavaScriptForTests(
      "var doc = document.querySelector('iframe').contentDocument; "
      "doc.body.style.backgroundColor = '#f00';");

  // Now try to handle a "layout complete" event on the detached textfield
  // object. The event handling will be a no-op, but the layout complete
  // will trigger calling SendLocationChanges. Make sure that
  // SendLocationChanges doesn't depend on layout in the main frame being
  // clean.
  //
  // If this doesn't crash, the test passes.
  accessibility->HandleAXEvent(child_textfield,
                               ui::AX_EVENT_LAYOUT_COMPLETE);
  accessibility->SendPendingAccessibilityEvents();
}

}  // namespace content
