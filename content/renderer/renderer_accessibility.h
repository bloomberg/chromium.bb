// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_RENDERER_ACCESSIBILITY_H_
#define CONTENT_RENDERER_RENDERER_ACCESSIBILITY_H_
#pragma once

#include <vector>

#include "base/hash_tables.h"
#include "base/task.h"
#include "content/public/renderer/render_view_observer.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebAccessibilityNotification.h"

class RenderViewImpl;

namespace WebKit {
class WebAccessibilityObject;
class WebDocument;
class WebNode;
};

namespace webkit_glue {
struct WebAccessibility;
};

// RendererAccessibility belongs to the RenderView. It's responsible for
// sending a serialized representation of WebKit's accessibility tree from
// the renderer to the browser and sending updates whenever it changes, and
// handling requests from the browser to perform accessibility actions on
// nodes in the tree (e.g., change focus, or click on a button).
class RendererAccessibility : public content::RenderViewObserver {
 public:
  RendererAccessibility(RenderViewImpl* render_view);
  virtual ~RendererAccessibility();

  // RenderView::Observer implementation.
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;
  virtual void FocusedNodeChanged(const WebKit::WebNode& node) OVERRIDE;
  virtual void DidFinishLoad(WebKit::WebFrame* frame) OVERRIDE;

  // Called when an accessibility notification occurs in WebKit.
  virtual void PostAccessibilityNotification(
      const WebKit::WebAccessibilityObject& obj,
      WebKit::WebAccessibilityNotification notification);

 private:
  // One accessibility notification from WebKit. These are queued up and
  // used to send tree updates and notification messages from the
  // renderer to the browser.
  struct Notification {
   public:
    // The id of the accessibility object.
    int32 id;

    // The accessibility notification type.
    WebKit::WebAccessibilityNotification type;
  };

  // In order to keep track of what nodes the browser knows about, we keep a
  // representation of the browser tree - just IDs and parent/child
  // relationships.
  struct BrowserTreeNode {
    BrowserTreeNode() {}
    ~BrowserTreeNode() {}
    int32 id;
    std::vector<BrowserTreeNode*> children;
  };

  // Send queued notifications from the renderer to the browser.
  void SendPendingAccessibilityNotifications();

  // Update our representation of what nodes the browser has, given a
  // tree of nodes.
  void UpdateBrowserTree(const webkit_glue::WebAccessibility& renderer_node);

  // Clear the given node and recursively delete all of its descendants
  // from the browser tree. (Does not delete |browser_node|).
  void ClearBrowserTreeNode(BrowserTreeNode* browser_node);

  // Handlers for messages from the browser to the renderer.
  void OnAccessibilityDoDefaultAction(int acc_obj_id);
  void OnAccessibilityNotificationsAck();
  void OnEnableAccessibility();
  void OnSetAccessibilityFocus(int acc_obj_id);

  // Whether or not this notification typically needs to send
  // updates to its children, too.
  bool ShouldIncludeChildren(const Notification& notification);

  // Returns the main top-level document for this page, or NULL if there's
  // no view or frame.
  WebKit::WebDocument GetMainDocument();

  // So we can queue up tasks to be executed later.
  ScopedRunnableMethodFactory<RendererAccessibility> method_factory_;

  // Notifications from WebKit are collected until they are ready to be
  // sent to the browser.
  std::vector<Notification> pending_notifications_;

  // Our representation of the browser tree.
  BrowserTreeNode* browser_root_;

  // A map from IDs to nodes in the browser tree.
  base::hash_map<int32, BrowserTreeNode*> browser_id_map_;

  // Set if we are waiting for an accessibility notification ack.
  bool ack_pending_;

  // True if verbose logging of accessibility events is on.
  bool logging_;

  // True if we've sent a load complete notification for this page already.
  bool sent_load_complete_;

  DISALLOW_COPY_AND_ASSIGN(RendererAccessibility);
};

#endif  // CONTENT_RENDERER_RENDERER_ACCESSIBILITY_H_
