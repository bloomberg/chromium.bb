// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/accessibility/renderer_accessibility_focus_only.h"

#include "content/common/accessibility_node_data.h"
#include "content/renderer/render_view_impl.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDocument.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebElement.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebNode.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebView.h"

using WebKit::WebDocument;
using WebKit::WebElement;
using WebKit::WebFrame;
using WebKit::WebNode;
using WebKit::WebView;

namespace {
// The root node will always have id 1. Let each child node have a new
// id starting with 2.
const int kInitialId = 2;
}

namespace content {

RendererAccessibilityFocusOnly::RendererAccessibilityFocusOnly(
    RenderViewImpl* render_view)
    : RendererAccessibility(render_view),
      next_id_(kInitialId) {
}

RendererAccessibilityFocusOnly::~RendererAccessibilityFocusOnly() {
}

void RendererAccessibilityFocusOnly::HandleWebAccessibilityNotification(
    const WebKit::WebAccessibilityObject& obj,
    WebKit::WebAccessibilityNotification notification) {
  // Do nothing.
}

void RendererAccessibilityFocusOnly::FocusedNodeChanged(const WebNode& node) {
  // Send the new accessible tree and post a native focus event.
  HandleFocusedNodeChanged(node, true);
}

void RendererAccessibilityFocusOnly::DidFinishLoad(WebKit::WebFrame* frame) {
  WebView* view = render_view()->GetWebView();
  if (view->focusedFrame() != frame)
    return;

  WebDocument document = frame->document();
  // Send an accessible tree to the browser, but do not post a native
  // focus event. This is important so that if focus is initially in an
  // editable text field, Windows will know to pop up the keyboard if the
  // user touches it and focus doesn't change.
  HandleFocusedNodeChanged(document.focusedNode(), false);
}

void RendererAccessibilityFocusOnly::HandleFocusedNodeChanged(
    const WebNode& node,
    bool send_focus_event) {
  const WebDocument& document = GetMainDocument();
  if (document.isNull())
    return;

  bool node_has_focus;
  bool node_is_editable_text;
  // Check HasIMETextFocus first, because it will correctly handle
  // focus in a text box inside a ppapi plug-in. Otherwise fall back on
  // checking the focused node in WebKit.
  if (render_view_->HasIMETextFocus()) {
    node_has_focus = true;
    node_is_editable_text = true;
  } else {
    node_has_focus = !node.isNull();
    node_is_editable_text =
        node_has_focus && render_view_->IsEditableNode(node);
  }

  std::vector<AccessibilityHostMsg_NotificationParams> notifications;
  notifications.push_back(AccessibilityHostMsg_NotificationParams());
  AccessibilityHostMsg_NotificationParams& notification = notifications[0];

  // If we want to update the browser's accessibility tree but not send a
  // native focus changed notification, we can send a LayoutComplete
  // notification, which doesn't post a native event on Windows.
  notification.notification_type =
      send_focus_event ?
      AccessibilityNotificationFocusChanged :
      AccessibilityNotificationLayoutComplete;

  // Set the id that the notification applies to: the root node if nothing
  // has focus, otherwise the focused node.
  notification.id = node_has_focus ? next_id_ : 1;

  // Always include the root of the tree, the document. It always has id 1.
  notification.nodes.push_back(AccessibilityNodeData());
  AccessibilityNodeData& root = notification.nodes[0];
  root.id = 1;
  root.role = AccessibilityNodeData::ROLE_ROOT_WEB_AREA;
  root.state =
      (1 << AccessibilityNodeData::STATE_READONLY) |
      (1 << AccessibilityNodeData::STATE_FOCUSABLE);
  if (!node_has_focus)
    root.state |= (1 << AccessibilityNodeData::STATE_FOCUSED);
  root.location = gfx::Rect(render_view_->size());
  root.child_ids.push_back(next_id_);

  notification.nodes.push_back(AccessibilityNodeData());
  AccessibilityNodeData& child = notification.nodes[1];
  child.id = next_id_;
  child.role = AccessibilityNodeData::ROLE_GROUP;

  if (!node.isNull() && node.isElementNode()) {
    child.location = gfx::Rect(
        const_cast<WebNode&>(node).to<WebElement>().boundsInViewportSpace());
  } else if (render_view_->HasIMETextFocus()) {
    child.location = root.location;
  } else {
    child.location = gfx::Rect();
  }

  if (node_has_focus) {
    child.state =
        (1 << AccessibilityNodeData::STATE_FOCUSABLE) |
        (1 << AccessibilityNodeData::STATE_FOCUSED);
    if (!node_is_editable_text)
      child.state |= (1 << AccessibilityNodeData::STATE_READONLY);
  }

#ifndef NDEBUG
  if (logging_) {
    LOG(INFO) << "Accessibility update: \n"
        << "routing id=" << routing_id()
        << " notification="
        << AccessibilityNotificationToString(notification.notification_type)
        << "\n" << notification.nodes[0].DebugString(true);
  }
#endif

  Send(new AccessibilityHostMsg_Notifications(routing_id(), notifications));

  // Increment the id, wrap back when we get past a million.
  next_id_++;
  if (next_id_ > 1000000)
    next_id_ = kInitialId;
}

}  // namespace content
