// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_host/test/test_render_view_host.h"
#include "chrome/browser/tab_contents/navigation_entry.h"
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
