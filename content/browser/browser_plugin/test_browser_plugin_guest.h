// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BROWSER_PLUGIN_TEST_BROWSER_PLUGIN_GUEST_H_
#define CONTENT_BROWSER_BROWSER_PLUGIN_TEST_BROWSER_PLUGIN_GUEST_H_

#include "base/compiler_specific.h"
#include "base/process_util.h"
#include "content/browser/browser_plugin/browser_plugin_guest.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/test/test_utils.h"

class WebContentsImpl;

namespace content {

class RenderProcessHost;
class RenderViewHost;

// Test class for BrowserPluginGuest.
//
// Provides utilities to wait for certain state/messages in BrowserPluginGuest
// to be used in tests.
class TestBrowserPluginGuest : public BrowserPluginGuest,
                               public NotificationObserver {
 public:
  TestBrowserPluginGuest(int instance_id,
                         WebContentsImpl* web_contents,
                         RenderViewHost* render_view_host);
  virtual ~TestBrowserPluginGuest();

  // NotificationObserver method override.
  virtual void Observe(int type,
                       const NotificationSource& source,
                       const NotificationDetails& details) OVERRIDE;

  // Overridden methods from BrowserPluginGuest to intercept in test objects.
  virtual void RenderViewGone(base::TerminationStatus status) OVERRIDE;
  virtual void SetFocus(bool focused) OVERRIDE;
  virtual bool ViewTakeFocus(bool reverse) OVERRIDE;

  // Test utilities to wait for a event we are interested in.
  // Waits until UpdateRect message is sent from the guest, meaning it is
  // ready/rendered.
  void WaitForUpdateRectMsg();
  // Waits until UpdateRect message with a specific size is sent from the guest.
  void WaitForUpdateRectMsgWithSize(int width, int height);
  // Waits for focus to reach this guest.
  void WaitForFocus();
  // Wait for focus to move out of this guest.
  void WaitForAdvanceFocus();
  // Wait until the guest is hidden.
  void WaitUntilHidden();
  // Waits until guest crashes.
  void WaitForCrashed();

 private:
  // Overridden methods from BrowserPluginGuest to intercept in test objects.
  virtual void SendMessageToEmbedder(IPC::Message* msg) OVERRIDE;

  int update_rect_count_;
  bool crash_observed_;
  bool focus_observed_;
  bool advance_focus_observed_;
  bool was_hidden_observed_;

  // For WaitForUpdateRectMsgWithSize().
  bool waiting_for_update_rect_msg_with_size_;
  int expected_width_;
  int expected_height_;

  int last_update_rect_width_;
  int last_update_rect_height_;

  scoped_refptr<MessageLoopRunner> send_message_loop_runner_;
  scoped_refptr<MessageLoopRunner> crash_message_loop_runner_;
  scoped_refptr<MessageLoopRunner> focus_message_loop_runner_;
  scoped_refptr<MessageLoopRunner> advance_focus_message_loop_runner_;
  scoped_refptr<MessageLoopRunner> was_hidden_message_loop_runner_;

  // A scoped container for notification registries.
  NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(TestBrowserPluginGuest);
};

}  // namespace content

#endif  // CONTENT_BROWSER_BROWSER_PLUGIN_TEST_BROWSER_PLUGIN_GUEST_H_
