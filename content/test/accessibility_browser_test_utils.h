// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_TEST_ACCESSIBILITY_BROWSER_TEST_UTILS_H_
#define CONTENT_TEST_ACCESSIBILITY_BROWSER_TEST_UTILS_H_

#include "base/memory/weak_ptr.h"
#include "content/common/accessibility_mode_enums.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/accessibility/ax_tree.h"

namespace content {

class MessageLoopRunner;
class RenderFrameHostImpl;
class Shell;

// Create an instance of this class *before* doing any operation that
// might generate an accessibility event (like a page navigation or
// clicking on a button). Then call WaitForNotification
// afterwards to block until the specified accessibility notification has been
// received.
class AccessibilityNotificationWaiter {
 public:
  explicit AccessibilityNotificationWaiter(Shell* shell);
  AccessibilityNotificationWaiter(
      Shell* shell,
      AccessibilityMode accessibility_mode,
      ui::AXEvent event);
  AccessibilityNotificationWaiter(
      RenderFrameHostImpl* frame_host,
       ui::AXEvent event);
  ~AccessibilityNotificationWaiter();

  // Blocks until the specific accessibility notification registered in
  // AccessibilityNotificationWaiter is received. Ignores notifications for
  // "about:blank".
  void WaitForNotification();

  // After WaitForNotification has returned, this will retrieve
  // the tree of accessibility nodes received from the renderer process.
  const ui::AXTree& GetAXTree() const;

  // After WaitForNotification returns, use this to retrieve the id of the
  // node that was the target of the event.
  int event_target_id() { return event_target_id_; }

 private:
  // Callback from RenderViewHostImpl.
  void OnAccessibilityEvent(ui::AXEvent event, int event_target_id);

  // Helper function to determine if the accessibility tree in
  // GetAXTree() is about the page with the url "about:blank".
  bool IsAboutBlank();

  RenderFrameHostImpl* frame_host_;
  ui::AXEvent event_to_wait_for_;
  scoped_refptr<MessageLoopRunner> loop_runner_;
  int event_target_id_;

  base::WeakPtrFactory<AccessibilityNotificationWaiter> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(AccessibilityNotificationWaiter);
};

}  // namespace content

#endif  // CONTENT_TEST_ACCESSIBILITY_BROWSER_TEST_UTILS_H_
