// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/test_render_view_host.h"
#include "content/browser/tab_contents/navigation_controller.h"
#include "content/browser/tab_contents/navigation_entry.h"
#include "content/browser/tab_contents/test_tab_contents.h"
#include "content/common/view_messages.h"
#include "content/public/common/page_transition_types.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDragOperation.h"
#include "webkit/glue/webdropdata.h"

class RenderViewHostTest : public RenderViewHostTestHarness {
};

// All about URLs reported by the renderer should get rewritten to about:blank.
// See RenderViewHost::OnMsgNavigate for a discussion.
TEST_F(RenderViewHostTest, FilterAbout) {
  rvh()->SendNavigate(1, GURL("about:cache"));
  ASSERT_TRUE(controller().GetActiveEntry());
  EXPECT_EQ(GURL("about:blank"), controller().GetActiveEntry()->url());
}

// Create a full screen popup RenderWidgetHost and View.
TEST_F(RenderViewHostTest, CreateFullscreenWidget) {
  int routing_id = process()->GetNextRoutingID();
  rvh()->CreateNewFullscreenWidget(routing_id);
}

// Makes sure that RenderViewHost::is_waiting_for_unload_ack_ is false when
// reloading a page. If is_waiting_for_unload_ack_ is not false when reloading
// the tab may get closed out even though the user pressed the reload button.
TEST_F(RenderViewHostTest, ResetUnloadOnReload) {
  const GURL url1("http://foo1");
  const GURL url2("http://foo2");

  // This test is for a subtle timing bug. Here's the sequence that triggered
  // the bug:
  // . go to a page.
  // . go to a new page, preferably one that takes a while to resolve, such
  //   as one on a site that doesn't exist.
  //   . After this step is_waiting_for_unload_ack_ has been set to true on
  //     the first RVH.
  // . click stop before the page has been commited.
  // . click reload.
  //   . is_waiting_for_unload_ack_ is still true, and the if the hang monitor
  //     fires the tab gets closed.

  NavigateAndCommit(url1);
  controller().LoadURL(
      url2, content::Referrer(), content::PAGE_TRANSITION_LINK, std::string());
  // Simulate the ClosePage call which is normally sent by the net::URLRequest.
  rvh()->ClosePage();
  // Needed so that navigations are not suspended on the RVH.
  rvh()->SendShouldCloseACK(true);
  contents()->Stop();
  controller().Reload(false);
  EXPECT_FALSE(rvh()->is_waiting_for_unload_ack_for_testing());
}

class MockDraggingRenderViewHostDelegateView
    : public RenderViewHostDelegate::View {
 public:
  virtual ~MockDraggingRenderViewHostDelegateView() {}
  virtual void CreateNewWindow(
      int route_id,
      const ViewHostMsg_CreateWindow_Params& params) {}
  virtual void CreateNewWidget(int route_id,
                               WebKit::WebPopupType popup_type) {}
  virtual void CreateNewFullscreenWidget(int route_id) {}
  virtual void ShowCreatedWindow(int route_id,
                                 WindowOpenDisposition disposition,
                                 const gfx::Rect& initial_pos,
                                 bool user_gesture) {}
  virtual void ShowCreatedWidget(int route_id,
                                 const gfx::Rect& initial_pos) {}
  virtual void ShowCreatedFullscreenWidget(int route_id) {}
  virtual void ShowContextMenu(const ContextMenuParams& params) {}
  virtual void ShowPopupMenu(const gfx::Rect& bounds,
                             int item_height,
                             double item_font_size,
                             int selected_item,
                             const std::vector<WebMenuItem>& items,
                             bool right_aligned) {}
  virtual void StartDragging(const WebDropData& drop_data,
                             WebKit::WebDragOperationsMask allowed_ops,
                             const SkBitmap& image,
                             const gfx::Point& image_offset) {
    drag_url_ = drop_data.url;
    html_base_url_ = drop_data.html_base_url;
  }
  virtual void UpdateDragCursor(WebKit::WebDragOperation operation) {}
  virtual void GotFocus() {}
  virtual void TakeFocus(bool reverse) {}
  virtual void UpdatePreferredSize(const gfx::Size& pref_size) {}

  GURL drag_url() {
    return drag_url_;
  }

  GURL html_base_url() {
    return html_base_url_;
  }

 private:
  GURL drag_url_;
  GURL html_base_url_;
};

TEST_F(RenderViewHostTest, StartDragging) {
  TestTabContents* tab_contents = contents();
  MockDraggingRenderViewHostDelegateView view_delegate;
  tab_contents->set_view_delegate(&view_delegate);

  WebDropData drop_data;
  GURL file_url = GURL("file:///home/user/secrets.txt");
  drop_data.url = file_url;
  drop_data.html_base_url = file_url;
  rvh()->TestOnMsgStartDragging(drop_data);
  EXPECT_TRUE(view_delegate.drag_url().is_empty());
  EXPECT_TRUE(view_delegate.html_base_url().is_empty());

  GURL http_url = GURL("http://www.domain.com/index.html");
  drop_data.url = http_url;
  drop_data.html_base_url = http_url;
  rvh()->TestOnMsgStartDragging(drop_data);
  EXPECT_EQ(http_url, view_delegate.drag_url());
  EXPECT_EQ(http_url, view_delegate.html_base_url());

  GURL https_url = GURL("https://www.domain.com/index.html");
  drop_data.url = https_url;
  drop_data.html_base_url = https_url;
  rvh()->TestOnMsgStartDragging(drop_data);
  EXPECT_EQ(https_url, view_delegate.drag_url());
  EXPECT_EQ(https_url, view_delegate.html_base_url());

  GURL javascript_url = GURL("javascript:alert('I am a bookmarklet')");
  drop_data.url = javascript_url;
  drop_data.html_base_url = http_url;
  rvh()->TestOnMsgStartDragging(drop_data);
  EXPECT_EQ(javascript_url, view_delegate.drag_url());
  EXPECT_EQ(http_url, view_delegate.html_base_url());
}

// The test that follow trigger DCHECKS in debug build.
#if defined(NDEBUG) && !defined(DCHECK_ALWAYS_ON)

// Test that when we fail to de-serialize a message, RenderViewHost calls the
// ReceivedBadMessage() handler.
TEST_F(RenderViewHostTest, BadMessageHandlerRenderViewHost) {
  EXPECT_EQ(0, process()->bad_msg_count());
  // craft an incorrect ViewHostMsg_UpdateTargetURL message. The real one has
  // two payload items but the one we construct has none.
  IPC::Message message(0, ViewHostMsg_UpdateTargetURL::ID,
                       IPC::Message::PRIORITY_NORMAL);
  rvh()->TestOnMessageReceived(message);
  EXPECT_EQ(1, process()->bad_msg_count());
}

// Test that when we fail to de-serialize a message, RenderWidgetHost calls the
// ReceivedBadMessage() handler.
TEST_F(RenderViewHostTest, BadMessageHandlerRenderWidgetHost) {
  EXPECT_EQ(0, process()->bad_msg_count());
  // craft an incorrect ViewHostMsg_UpdateRect message. The real one has
  // one payload item but the one we construct has none.
  IPC::Message message(0, ViewHostMsg_UpdateRect::ID,
                       IPC::Message::PRIORITY_NORMAL);
  rvh()->TestOnMessageReceived(message);
  EXPECT_EQ(1, process()->bad_msg_count());
}

// Test that OnMsgInputEventAck() detects bad messages.
TEST_F(RenderViewHostTest, BadMessageHandlerInputEventAck) {
  EXPECT_EQ(0, process()->bad_msg_count());
  // ViewHostMsg_HandleInputEvent_ACK is defined taking 0 params but
  // the code actually expects it to have at least one int para, this this
  // bogus message will not fail at de-serialization but should fail in
  // OnMsgInputEventAck() processing.
  IPC::Message message(0, ViewHostMsg_HandleInputEvent_ACK::ID,
                       IPC::Message::PRIORITY_NORMAL);
  rvh()->TestOnMessageReceived(message);
  EXPECT_EQ(1, process()->bad_msg_count());
}

#endif
