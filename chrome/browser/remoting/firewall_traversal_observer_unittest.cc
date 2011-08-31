// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/remoting/firewall_traversal_observer.h"

#include "chrome/browser/prefs/pref_service.h"
#include "content/browser/renderer_host/mock_render_process_host.h"
#include "chrome/browser/ui/tab_contents/test_tab_contents_wrapper.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "content/browser/browser_thread.h"
#include "content/browser/tab_contents/test_tab_contents.h"
#include "content/common/notification_service.h"
#include "content/common/view_messages.h"
#include "ipc/ipc_message.h"

class FirewallTraversalTabHelperTest : public TabContentsWrapperTestHarness {
 public:
  FirewallTraversalTabHelperTest()
      : browser_thread_(BrowserThread::UI, &message_loop_) {}
  virtual ~FirewallTraversalTabHelperTest() {}

 private:
  BrowserThread browser_thread_;
};

// Test receiving message.
TEST_F(FirewallTraversalTabHelperTest, TestBasicMessage) {
  IPC::TestSink& sink = process()->sink();
  sink.ClearMessages();
  IPC::Message msg(MSG_ROUTING_NONE,
                   ViewHostMsg_RequestRemoteAccessClientFirewallTraversal::ID,
                   IPC::Message::PRIORITY_NORMAL);
  ASSERT_TRUE(active_rvh()->TestOnMessageReceived(msg));
  const IPC::Message* expect_msg = sink.GetUniqueMessageMatching(
      ViewMsg_UpdateRemoteAccessClientFirewallTraversal::ID);
  ASSERT_TRUE(expect_msg);
}

// Test changing preference.
TEST_F(FirewallTraversalTabHelperTest, TestPrefChange) {
  IPC::TestSink& sink = process()->sink();
  sink.ClearMessages();
  contents_wrapper()->profile()->GetPrefs()->SetBoolean(
      prefs::kRemoteAccessClientFirewallTraversal,
      false);
  const IPC::Message* expect_msg = sink.GetUniqueMessageMatching(
      ViewMsg_UpdateRemoteAccessClientFirewallTraversal::ID);
  ASSERT_TRUE(expect_msg);
}
