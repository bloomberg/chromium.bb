// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/accessibility/accessibility_test_utils.h"

#include "content/browser/accessibility/browser_accessibility.h"
#include "content/browser/accessibility/browser_accessibility_manager.h"
#include "content/browser/frame_host/frame_tree.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/test/accessibility_browser_test_utils.h"

namespace content {

bool AccessibilityTreeContainsNodeWithName(BrowserAccessibility* node,
                                           const std::string& name) {
  if (node->GetStringAttribute(ui::AX_ATTR_NAME) == name)
    return true;
  for (unsigned i = 0; i < node->PlatformChildCount(); i++) {
    if (AccessibilityTreeContainsNodeWithName(node->PlatformGetChild(i), name))
      return true;
  }
  return false;
}

void WaitForAccessibilityTreeToContainNodeWithName(WebContents* web_contents,
                                                   const std::string& name) {
  WebContentsImpl* web_contents_impl = static_cast<WebContentsImpl*>(
      web_contents);
  RenderFrameHostImpl* main_frame = static_cast<RenderFrameHostImpl*>(
      web_contents_impl->GetMainFrame());
  BrowserAccessibilityManager* main_frame_manager =
      main_frame->browser_accessibility_manager();
  FrameTree* frame_tree = web_contents_impl->GetFrameTree();
  while (!AccessibilityTreeContainsNodeWithName(
             main_frame_manager->GetRoot(), name)) {
    AccessibilityNotificationWaiter accessibility_waiter(main_frame,
                                                         ui::AX_EVENT_NONE);
    for (FrameTreeNode* node : frame_tree->Nodes())
      accessibility_waiter.ListenToAdditionalFrame(
          node->current_frame_host());

    accessibility_waiter.WaitForNotification();
  }
}

}  // namespace
