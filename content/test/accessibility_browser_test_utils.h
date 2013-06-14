// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_TEST_ACCESSIBILITY_BROWSER_TEST_UTILS_H_
#define CONTENT_TEST_ACCESSIBILITY_BROWSER_TEST_UTILS_H_

#include "base/memory/weak_ptr.h"
#include "content/common/accessibility_node_data.h"
#include "content/common/accessibility_notification.h"
#include "content/common/view_message_enums.h"

namespace content {

class MessageLoopRunner;
class RenderViewHostImpl;
class Shell;

// Create an instance of this class *before* doing any operation that
// might generate an accessibility event (like a page navigation or
// clicking on a button). Then call WaitForNotification
// afterwards to block until the specified accessibility notification has been
// received.
class AccessibilityNotificationWaiter {
 public:
  AccessibilityNotificationWaiter(
      Shell* shell,
      AccessibilityMode accessibility_mode,
      AccessibilityNotification notification);
  ~AccessibilityNotificationWaiter();

  // Blocks until the specific accessibility notification registered in
  // AccessibilityNotificationWaiter is received. Ignores notifications for
  // "about:blank".
  void WaitForNotification();

  // After WaitForNotification has returned, this will retrieve
  // the tree of accessibility nodes received from the renderer process.
  const AccessibilityNodeDataTreeNode& GetAccessibilityNodeDataTree() const;

 private:
  // Callback from RenderViewHostImpl.
  void OnAccessibilityNotification(AccessibilityNotification notification);

  // Helper function to determine if the accessibility tree in
  // GetAccessibilityNodeDataTree() is about the page with the url
  // "about:blank".
  bool IsAboutBlank();

  Shell* shell_;
  RenderViewHostImpl* view_host_;
  AccessibilityNotification notification_to_wait_for_;
  scoped_refptr<MessageLoopRunner> loop_runner_;
  base::WeakPtrFactory<AccessibilityNotificationWaiter> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(AccessibilityNotificationWaiter);
};

}  // namespace content

#endif  // CONTENT_TEST_ACCESSIBILITY_BROWSER_TEST_UTILS_H_
