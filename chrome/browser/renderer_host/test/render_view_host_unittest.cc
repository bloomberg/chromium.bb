// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_host/test/test_render_view_host.h"
#include "chrome/browser/tab_contents/navigation_controller.h"
#include "chrome/browser/tab_contents/navigation_entry.h"
#include "chrome/browser/tab_contents/test_tab_contents.h"
#include "chrome/common/render_messages.h"

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
  controller().LoadURL(url2, GURL(), 0);
  // Simulate the ClosePage call which is normally sent by the net::URLRequest.
  rvh()->ClosePage(true, 0, 0);
  // Needed so that navigations are not suspended on the RVH. Normally handled
  // by way of ViewHostMsg_ShouldClose_ACK.
  contents()->render_manager()->ShouldClosePage(true, true);
  contents()->Stop();
  controller().Reload(false);
  EXPECT_FALSE(rvh()->is_waiting_for_unload_ack());
}

// The test that follow trigger DCHECKS in debug build.
#if defined(NDEBUG)

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

#endif  // NDEBUG
