// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/utf_string_conversions.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/renderer_host/test_render_view_host.h"
#include "content/browser/web_contents/navigation_controller_impl.h"
#include "content/common/view_messages.h"
#include "content/port/browser/render_view_host_delegate_view.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/common/bindings_policy.h"
#include "content/public/common/page_transition_types.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/test/test_web_contents.h"
#include "net/base/net_util.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDragOperation.h"
#include "webkit/glue/webdropdata.h"

namespace content {

class RenderViewHostTest : public RenderViewHostImplTestHarness {
};

// All about URLs reported by the renderer should get rewritten to about:blank.
// See RenderViewHost::OnNavigate for a discussion.
TEST_F(RenderViewHostTest, FilterAbout) {
  test_rvh()->SendNavigate(1, GURL("about:cache"));
  ASSERT_TRUE(controller().GetActiveEntry());
  EXPECT_EQ(GURL("about:blank"), controller().GetActiveEntry()->GetURL());
}

// Create a full screen popup RenderWidgetHost and View.
TEST_F(RenderViewHostTest, CreateFullscreenWidget) {
  int routing_id = process()->GetNextRoutingID();
  test_rvh()->CreateNewFullscreenWidget(routing_id);
}

// Makes sure that RenderViewHost::is_waiting_for_unload_ack_ is false when
// reloading a page. If is_waiting_for_unload_ack_ is not false when reloading
// the contents may get closed out even though the user pressed the reload
// button.
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
  //     fires the contents gets closed.

  NavigateAndCommit(url1);
  controller().LoadURL(
      url2, Referrer(), PAGE_TRANSITION_LINK, std::string());
  // Simulate the ClosePage call which is normally sent by the net::URLRequest.
  rvh()->ClosePage();
  // Needed so that navigations are not suspended on the RVH.
  test_rvh()->SendShouldCloseACK(true);
  contents()->Stop();
  controller().Reload(false);
  EXPECT_FALSE(test_rvh()->is_waiting_for_unload_ack_for_testing());
}

// Ensure we do not grant bindings to a process shared with unprivileged views.
TEST_F(RenderViewHostTest, DontGrantBindingsToSharedProcess) {
  // Create another view in the same process.
  scoped_ptr<TestWebContents> new_web_contents(
      TestWebContents::Create(browser_context(), rvh()->GetSiteInstance()));

  rvh()->AllowBindings(BINDINGS_POLICY_WEB_UI);
  EXPECT_FALSE(rvh()->GetEnabledBindings() & BINDINGS_POLICY_WEB_UI);
}

class MockDraggingRenderViewHostDelegateView
    : public RenderViewHostDelegateView {
 public:
  virtual ~MockDraggingRenderViewHostDelegateView() {}
  virtual void ShowContextMenu(
      const ContextMenuParams& params,
      ContextMenuSourceType type) OVERRIDE {}
  virtual void ShowPopupMenu(const gfx::Rect& bounds,
                             int item_height,
                             double item_font_size,
                             int selected_item,
                             const std::vector<WebMenuItem>& items,
                             bool right_aligned,
                             bool allow_multiple_selection) OVERRIDE {}
  virtual void StartDragging(const WebDropData& drop_data,
                             WebKit::WebDragOperationsMask allowed_ops,
                             const gfx::ImageSkia& image,
                             const gfx::Vector2d& image_offset,
                             const DragEventSourceInfo& event_info) OVERRIDE {
    drag_url_ = drop_data.url;
    html_base_url_ = drop_data.html_base_url;
  }
  virtual void UpdateDragCursor(WebKit::WebDragOperation operation) OVERRIDE {}
  virtual void GotFocus() OVERRIDE {}
  virtual void TakeFocus(bool reverse) OVERRIDE {}
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
  TestWebContents* web_contents = contents();
  MockDraggingRenderViewHostDelegateView delegate_view;
  web_contents->set_delegate_view(&delegate_view);

  WebDropData drop_data;
  GURL file_url = GURL("file:///home/user/secrets.txt");
  drop_data.url = file_url;
  drop_data.html_base_url = file_url;
  test_rvh()->TestOnStartDragging(drop_data);
  EXPECT_EQ(GURL("about:blank"), delegate_view.drag_url());
  EXPECT_EQ(GURL("about:blank"), delegate_view.html_base_url());

  GURL http_url = GURL("http://www.domain.com/index.html");
  drop_data.url = http_url;
  drop_data.html_base_url = http_url;
  test_rvh()->TestOnStartDragging(drop_data);
  EXPECT_EQ(http_url, delegate_view.drag_url());
  EXPECT_EQ(http_url, delegate_view.html_base_url());

  GURL https_url = GURL("https://www.domain.com/index.html");
  drop_data.url = https_url;
  drop_data.html_base_url = https_url;
  test_rvh()->TestOnStartDragging(drop_data);
  EXPECT_EQ(https_url, delegate_view.drag_url());
  EXPECT_EQ(https_url, delegate_view.html_base_url());

  GURL javascript_url = GURL("javascript:alert('I am a bookmarklet')");
  drop_data.url = javascript_url;
  drop_data.html_base_url = http_url;
  test_rvh()->TestOnStartDragging(drop_data);
  EXPECT_EQ(javascript_url, delegate_view.drag_url());
  EXPECT_EQ(http_url, delegate_view.html_base_url());
}

TEST_F(RenderViewHostTest, DragEnteredFileURLsStillBlocked) {
  WebDropData dropped_data;
  gfx::Point client_point;
  gfx::Point screen_point;
  // We use "//foo/bar" path (rather than "/foo/bar") since dragged paths are
  // expected to be absolute on any platforms.
  base::FilePath highlighted_file_path(FILE_PATH_LITERAL("//tmp/foo.html"));
  base::FilePath dragged_file_path(FILE_PATH_LITERAL("//tmp/image.jpg"));
  base::FilePath sensitive_file_path(FILE_PATH_LITERAL("//etc/passwd"));
  GURL highlighted_file_url = net::FilePathToFileURL(highlighted_file_path);
  GURL dragged_file_url = net::FilePathToFileURL(dragged_file_path);
  GURL sensitive_file_url = net::FilePathToFileURL(sensitive_file_path);
  dropped_data.url = highlighted_file_url;
  dropped_data.filenames.push_back(WebDropData::FileInfo(
      UTF8ToUTF16(dragged_file_path.AsUTF8Unsafe()), string16()));

  rvh()->DragTargetDragEnter(dropped_data, client_point, screen_point,
                              WebKit::WebDragOperationNone, 0);

  int id = process()->GetID();
  ChildProcessSecurityPolicyImpl* policy =
      ChildProcessSecurityPolicyImpl::GetInstance();

  EXPECT_FALSE(policy->CanRequestURL(id, highlighted_file_url));
  EXPECT_FALSE(policy->CanReadFile(id, highlighted_file_path));
  EXPECT_TRUE(policy->CanRequestURL(id, dragged_file_url));
  EXPECT_TRUE(policy->CanReadFile(id, dragged_file_path));
  EXPECT_FALSE(policy->CanRequestURL(id, sensitive_file_url));
  EXPECT_FALSE(policy->CanReadFile(id, sensitive_file_path));
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
  test_rvh()->OnMessageReceived(message);
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
  test_rvh()->OnMessageReceived(message);
  EXPECT_EQ(1, process()->bad_msg_count());
}

// Test that OnInputEventAck() detects bad messages.
TEST_F(RenderViewHostTest, BadMessageHandlerInputEventAck) {
  EXPECT_EQ(0, process()->bad_msg_count());
  // ViewHostMsg_HandleInputEvent_ACK is defined taking 0 params but
  // the code actually expects it to have at least one int para, this this
  // bogus message will not fail at de-serialization but should fail in
  // OnInputEventAck() processing.
  IPC::Message message(0, ViewHostMsg_HandleInputEvent_ACK::ID,
                       IPC::Message::PRIORITY_NORMAL);
  test_rvh()->OnMessageReceived(message);
  EXPECT_EQ(1, process()->bad_msg_count());
}

#endif

}  // namespace content
