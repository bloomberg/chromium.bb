// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/accessibility/renderer_accessibility_complete.h"

#include "base/bind.h"
#include "base/message_loop.h"
#include "content/renderer/accessibility/accessibility_node_serializer.h"
#include "content/renderer/render_view_impl.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebAccessibilityObject.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDocument.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebInputElement.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebNode.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebView.h"

using WebKit::WebAccessibilityNotification;
using WebKit::WebAccessibilityObject;
using WebKit::WebDocument;
using WebKit::WebFrame;
using WebKit::WebNode;
using WebKit::WebPoint;
using WebKit::WebRect;
using WebKit::WebSize;
using WebKit::WebView;

namespace content {

bool WebAccessibilityNotificationToAccessibilityNotification(
    WebAccessibilityNotification notification,
    AccessibilityNotification* type) {
  switch (notification) {
    case WebKit::WebAccessibilityNotificationActiveDescendantChanged:
      *type = AccessibilityNotificationActiveDescendantChanged;
      break;
    case WebKit::WebAccessibilityNotificationAriaAttributeChanged:
      *type = AccessibilityNotificationAriaAttributeChanged;
      break;
    case WebKit::WebAccessibilityNotificationAutocorrectionOccured:
      *type = AccessibilityNotificationAutocorrectionOccurred;
      break;
    case WebKit::WebAccessibilityNotificationCheckedStateChanged:
      *type = AccessibilityNotificationCheckStateChanged;
      break;
    case WebKit::WebAccessibilityNotificationChildrenChanged:
      *type = AccessibilityNotificationChildrenChanged;
      break;
    case WebKit::WebAccessibilityNotificationFocusedUIElementChanged:
      *type = AccessibilityNotificationFocusChanged;
      break;
    case WebKit::WebAccessibilityNotificationInvalidStatusChanged:
      *type = AccessibilityNotificationInvalidStatusChanged;
      break;
    case WebKit::WebAccessibilityNotificationLayoutComplete:
      *type = AccessibilityNotificationLayoutComplete;
      break;
    case WebKit::WebAccessibilityNotificationLiveRegionChanged:
      *type = AccessibilityNotificationLiveRegionChanged;
      break;
    case WebKit::WebAccessibilityNotificationLoadComplete:
      *type = AccessibilityNotificationLoadComplete;
      break;
    case WebKit::WebAccessibilityNotificationMenuListItemSelected:
      *type = AccessibilityNotificationMenuListItemSelected;
      break;
    case WebKit::WebAccessibilityNotificationMenuListValueChanged:
      *type = AccessibilityNotificationMenuListValueChanged;
      break;
    case WebKit::WebAccessibilityNotificationRowCollapsed:
      *type = AccessibilityNotificationRowCollapsed;
      break;
    case WebKit::WebAccessibilityNotificationRowCountChanged:
      *type = AccessibilityNotificationRowCountChanged;
      break;
    case WebKit::WebAccessibilityNotificationRowExpanded:
      *type = AccessibilityNotificationRowExpanded;
      break;
    case WebKit::WebAccessibilityNotificationScrolledToAnchor:
      *type = AccessibilityNotificationScrolledToAnchor;
      break;
    case WebKit::WebAccessibilityNotificationSelectedChildrenChanged:
      *type = AccessibilityNotificationSelectedChildrenChanged;
      break;
    case WebKit::WebAccessibilityNotificationSelectedTextChanged:
      *type = AccessibilityNotificationSelectedTextChanged;
      break;
    case WebKit::WebAccessibilityNotificationTextChanged:
      *type = AccessibilityNotificationTextChanged;
      break;
    case WebKit::WebAccessibilityNotificationValueChanged:
      *type = AccessibilityNotificationValueChanged;
      break;
    default:
      DLOG(WARNING)
          << "WebKit accessibility notification not handled in switch!";
      return false;
  }
  return true;
}

RendererAccessibilityComplete::RendererAccessibilityComplete(
    RenderViewImpl* render_view)
    : RendererAccessibility(render_view),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_factory_(this)),
      browser_root_(NULL),
      last_scroll_offset_(gfx::Size()),
      ack_pending_(false) {
  WebAccessibilityObject::enableAccessibility();

  const WebDocument& document = GetMainDocument();
  if (!document.isNull()) {
    // It's possible that the webview has already loaded a webpage without
    // accessibility being enabled. Initialize the browser's cached
    // accessibility tree by sending it a notification.
    HandleAccessibilityNotification(
        document.accessibilityObject(),
        AccessibilityNotificationLayoutComplete);
  }
}

RendererAccessibilityComplete::~RendererAccessibilityComplete() {
}

bool RendererAccessibilityComplete::OnMessageReceived(
    const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(RendererAccessibilityComplete, message)
    IPC_MESSAGE_HANDLER(AccessibilityMsg_SetFocus, OnSetFocus)
    IPC_MESSAGE_HANDLER(AccessibilityMsg_DoDefaultAction,
                        OnDoDefaultAction)
    IPC_MESSAGE_HANDLER(AccessibilityMsg_Notifications_ACK,
                        OnNotificationsAck)
    IPC_MESSAGE_HANDLER(AccessibilityMsg_ScrollToMakeVisible,
                        OnScrollToMakeVisible)
    IPC_MESSAGE_HANDLER(AccessibilityMsg_ScrollToPoint,
                        OnScrollToPoint)
    IPC_MESSAGE_HANDLER(AccessibilityMsg_SetTextSelection,
                        OnSetTextSelection)
    IPC_MESSAGE_HANDLER(AccessibilityMsg_FatalError, OnFatalError)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void RendererAccessibilityComplete::FocusedNodeChanged(const WebNode& node) {
  const WebDocument& document = GetMainDocument();
  if (document.isNull())
    return;

  if (node.isNull()) {
    // When focus is cleared, implicitly focus the document.
    // TODO(dmazzoni): Make WebKit send this notification instead.
    HandleAccessibilityNotification(
        document.accessibilityObject(),
        AccessibilityNotificationBlur);
  }
}

void RendererAccessibilityComplete::DidFinishLoad(WebKit::WebFrame* frame) {
  const WebDocument& document = GetMainDocument();
  if (document.isNull())
    return;

  // Check to see if the root accessibility object has changed, to work
  // around WebKit bugs that cause AXObjectCache to be cleared
  // unnecessarily.
  // TODO(dmazzoni): remove this once rdar://5794454 is fixed.
  WebAccessibilityObject new_root = document.accessibilityObject();
  if (!browser_root_ || new_root.axID() != browser_root_->id) {
    HandleAccessibilityNotification(
        new_root,
        AccessibilityNotificationLayoutComplete);
  }
}

void RendererAccessibilityComplete::HandleWebAccessibilityNotification(
    const WebAccessibilityObject& obj,
    WebAccessibilityNotification notification) {
  AccessibilityNotification temp;
  if (!WebAccessibilityNotificationToAccessibilityNotification(
      notification, &temp)) {
    return;
  }

  HandleAccessibilityNotification(obj, temp);
}

void RendererAccessibilityComplete::HandleAccessibilityNotification(
    const WebKit::WebAccessibilityObject& obj,
    AccessibilityNotification notification) {
  const WebDocument& document = GetMainDocument();
  if (document.isNull())
    return;

  gfx::Size scroll_offset = document.frame()->scrollOffset();
  if (scroll_offset != last_scroll_offset_) {
    // Make sure the browser is always aware of the scroll position of
    // the root document element by posting a generic notification that
    // will update it.
    // TODO(dmazzoni): remove this as soon as
    // https://bugs.webkit.org/show_bug.cgi?id=73460 is fixed.
    last_scroll_offset_ = scroll_offset;
    if (!obj.equals(document.accessibilityObject())) {
      HandleAccessibilityNotification(
          document.accessibilityObject(),
          AccessibilityNotificationLayoutComplete);
    }
  }

  // Add the accessibility object to our cache and ensure it's valid.
  AccessibilityHostMsg_NotificationParams acc_notification;
  acc_notification.id = obj.axID();
  acc_notification.notification_type = notification;

  // Discard duplicate accessibility notifications.
  for (uint32 i = 0; i < pending_notifications_.size(); ++i) {
    if (pending_notifications_[i].id == acc_notification.id &&
        pending_notifications_[i].notification_type ==
            acc_notification.notification_type) {
      return;
    }
  }
  pending_notifications_.push_back(acc_notification);

  if (!ack_pending_ && !weak_factory_.HasWeakPtrs()) {
    // When no accessibility notifications are in-flight post a task to send
    // the notifications to the browser. We use PostTask so that we can queue
    // up additional notifications.
    MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(
            &RendererAccessibilityComplete::
                SendPendingAccessibilityNotifications,
            weak_factory_.GetWeakPtr()));
  }
}

RendererAccessibilityComplete::BrowserTreeNode::BrowserTreeNode() : id(0) {}

RendererAccessibilityComplete::BrowserTreeNode::~BrowserTreeNode() {}

void RendererAccessibilityComplete::SendPendingAccessibilityNotifications() {
  const WebDocument& document = GetMainDocument();
  if (document.isNull())
    return;

  if (pending_notifications_.empty())
    return;

  ack_pending_ = true;

  // Make a copy of the notifications, because it's possible that
  // actions inside this loop will cause more notifications to be
  // queued up.
  std::vector<AccessibilityHostMsg_NotificationParams> src_notifications =
      pending_notifications_;
  pending_notifications_.clear();

  // Generate a notification message from each WebKit notification.
  std::vector<AccessibilityHostMsg_NotificationParams> notification_msgs;

  // Loop over each notification and generate an updated notification message.
  for (size_t i = 0; i < src_notifications.size(); ++i) {
    AccessibilityHostMsg_NotificationParams& notification =
        src_notifications[i];

    WebAccessibilityObject obj = document.accessibilityObjectFromID(
        notification.id);
    if (!obj.updateBackingStoreAndCheckValidity())
      continue;

    // When we get a "selected children changed" notification, WebKit
    // doesn't also send us notifications for each child that changed
    // selection state, so make sure we re-send that whole subtree.
    if (notification.notification_type ==
        AccessibilityNotificationSelectedChildrenChanged) {
      base::hash_map<int32, BrowserTreeNode*>::iterator iter =
          browser_id_map_.find(obj.axID());
      if (iter != browser_id_map_.end())
        ClearBrowserTreeNode(iter->second);
    }

    // The browser may not have this object yet, for example if we get a
    // notification on an object that was recently added, or if we get a
    // notification on a node before the page has loaded. Work our way
    // up the parent chain until we find a node the browser has, or until
    // we reach the root.
    WebAccessibilityObject root_object = document.accessibilityObject();
    int root_id = root_object.axID();
    while (browser_id_map_.find(obj.axID()) == browser_id_map_.end() &&
           !obj.isDetached() &&
           obj.axID() != root_id) {
      obj = obj.parentObject();
      if (notification.notification_type ==
          AccessibilityNotificationChildrenChanged) {
        notification.id = obj.axID();
      }
    }

    if (obj.isDetached()) {
#ifndef NDEBUG
      if (logging_)
        LOG(WARNING) << "Got notification on object that is invalid or has"
                     << " invalid ancestor. Id: " << obj.axID();
#endif
      continue;
    }

    // Another potential problem is that this notification may be on an
    // object that is detached from the tree. Determine if this node is not a
    // child of its parent, and if so move the notification to the parent.
    // TODO(dmazzoni): see if this can be removed after
    // https://bugs.webkit.org/show_bug.cgi?id=68466 is fixed.
    if (obj.axID() != root_id) {
      WebAccessibilityObject parent = obj.parentObject();
      while (!parent.isDetached() &&
             parent.accessibilityIsIgnored()) {
        parent = parent.parentObject();
      }

      if (parent.isDetached()) {
        NOTREACHED();
        continue;
      }
      bool is_child_of_parent = false;
      for (unsigned int i = 0; i < parent.childCount(); ++i) {
        if (parent.childAt(i).equals(obj)) {
          is_child_of_parent = true;
          break;
        }
      }

      if (!is_child_of_parent) {
        obj = parent;
        notification.id = obj.axID();
      }
    }

    // Allow WebKit to cache intermediate results since we're doing a bunch
    // of read-only queries at once.
    root_object.startCachingComputedObjectAttributesUntilTreeMutates();

    AccessibilityHostMsg_NotificationParams notification_msg;
    notification_msg.notification_type = notification.notification_type;
    notification_msg.id = notification.id;
    SerializeChangedNodes(obj, &notification_msg.nodes);
    notification_msgs.push_back(notification_msg);

#ifndef NDEBUG
    if (logging_) {
      AccessibilityNodeDataTreeNode tree;
      MakeAccessibilityNodeDataTree(notification_msg.nodes, &tree);
      LOG(INFO) << "Accessibility update: \n"
          << "routing id=" << routing_id()
          << " notification="
          << AccessibilityNotificationToString(notification.notification_type)
          << "\n" << tree.DebugString(true);
    }
#endif
  }

  Send(new AccessibilityHostMsg_Notifications(routing_id(), notification_msgs));
}

RendererAccessibilityComplete::BrowserTreeNode*
RendererAccessibilityComplete::CreateBrowserTreeNode() {
  return new RendererAccessibilityComplete::BrowserTreeNode();
}

void RendererAccessibilityComplete::SerializeChangedNodes(
    const WebKit::WebAccessibilityObject& obj,
    std::vector<AccessibilityNodeData>* dst) {
  // This method has three responsibilities:
  // 1. Serialize |obj| into an AccessibilityNodeData, and append it to
  //    the end of the |dst| vector to be send to the browser process.
  // 2. Determine if |obj| has any new children that the browser doesn't
  //    know about yet, and call SerializeChangedNodes recursively on those.
  // 3. Update our internal data structure that keeps track of what nodes
  //    the browser knows about.

  // First, find the BrowserTreeNode for this id in our data structure where
  // we keep track of what accessibility objects the browser already knows
  // about. If we don't find it, then this must be the new root of the
  // accessibility tree.
  BrowserTreeNode* browser_node = NULL;
  base::hash_map<int32, BrowserTreeNode*>::iterator iter =
    browser_id_map_.find(obj.axID());
  if (iter != browser_id_map_.end()) {
    browser_node = iter->second;
  } else {
    if (browser_root_) {
      ClearBrowserTreeNode(browser_root_);
      browser_id_map_.erase(browser_root_->id);
      delete browser_root_;
    }
    browser_root_ = CreateBrowserTreeNode();
    browser_node = browser_root_;
    browser_node->id = obj.axID();
    browser_id_map_[browser_node->id] = browser_node;
  }

  // Serialize this node. This fills in all of the fields in
  // AccessibilityNodeData except child_ids, which we handle below.
  dst->push_back(AccessibilityNodeData());
  AccessibilityNodeData* serialized_node = &dst->back();
  SerializeAccessibilityNode(obj, serialized_node);
  if (serialized_node->id == browser_root_->id)
    serialized_node->role = AccessibilityNodeData::ROLE_ROOT_WEB_AREA;

  // Create set of the ids of the children of |obj| so we can quickly look
  // up which children are new and which ones were there before.
  base::hash_set<int32> new_child_ids;
  for (unsigned i = 0; i < obj.childCount(); i++) {
    WebAccessibilityObject child = obj.childAt(i);
    if (ShouldIncludeChildNode(obj, child)) {
      new_child_ids.insert(child.axID());
    }
  }

  // Go through the old children and delete subtrees for child
  // ids that are no longer present, and create a map from
  // id to BrowserTreeNode for the rest. It's important to delete
  // first in a separate pass so that nodes that are reparented
  // don't end up children of two different parents in the middle
  // of an update, which can lead to a double-free.
  base::hash_map<int32, BrowserTreeNode*> browser_child_id_map;
  std::vector<BrowserTreeNode*> old_children;
  old_children.swap(browser_node->children);
  for (size_t i = 0; i < old_children.size(); i++) {
    BrowserTreeNode* old_child = old_children[i];
    int old_child_id = old_child->id;
    if (new_child_ids.find(old_child_id) == new_child_ids.end()) {
      browser_id_map_.erase(old_child_id);
      ClearBrowserTreeNode(old_child);
      delete old_child;
    } else {
      browser_child_id_map[old_child_id] = old_child;
    }
  }

  // Iterate over the children, make note of the ones that are new
  // and need to be serialized, and update the BrowserTreeNode
  // data structure to reflect the new tree.
  std::vector<WebAccessibilityObject> children_to_serialize;
  int child_count = obj.childCount();
  browser_node->children.reserve(child_count);
  for (int i = 0; i < child_count; i++) {
    WebAccessibilityObject child = obj.childAt(i);
    int child_id = child.axID();

    // Checks to make sure the child is valid, attached to this node,
    // and one we want to include in the tree.
    if (!ShouldIncludeChildNode(obj, child))
      continue;

    // No need to do anything more with children that aren't new;
    // the browser will reuse its existing object.
    if (new_child_ids.find(child_id) == new_child_ids.end())
      continue;

    new_child_ids.erase(child_id);
    serialized_node->child_ids.push_back(child_id);
    if (browser_child_id_map.find(child_id) != browser_child_id_map.end()) {
      browser_node->children.push_back(browser_child_id_map[child_id]);
    } else {
      BrowserTreeNode* new_child = CreateBrowserTreeNode();
      new_child->id = child_id;
      browser_node->children.push_back(new_child);
      browser_id_map_[child_id] = new_child;
      children_to_serialize.push_back(child);
    }
  }

  // Serialize all of the new children, recursively.
  for (size_t i = 0; i < children_to_serialize.size(); ++i)
    SerializeChangedNodes(children_to_serialize[i], dst);
}

void RendererAccessibilityComplete::ClearBrowserTreeNode(
    BrowserTreeNode* browser_node) {
  for (size_t i = 0; i < browser_node->children.size(); ++i) {
    browser_id_map_.erase(browser_node->children[i]->id);
    ClearBrowserTreeNode(browser_node->children[i]);
    delete browser_node->children[i];
  }
  browser_node->children.clear();
}

void RendererAccessibilityComplete::OnDoDefaultAction(int acc_obj_id) {
  const WebDocument& document = GetMainDocument();
  if (document.isNull())
    return;

  WebAccessibilityObject obj = document.accessibilityObjectFromID(acc_obj_id);
  if (obj.isDetached()) {
#ifndef NDEBUG
    if (logging_)
      LOG(WARNING) << "DoDefaultAction on invalid object id " << acc_obj_id;
#endif
    return;
  }

  obj.performDefaultAction();
}

void RendererAccessibilityComplete::OnScrollToMakeVisible(
    int acc_obj_id, gfx::Rect subfocus) {
  const WebDocument& document = GetMainDocument();
  if (document.isNull())
    return;

  WebAccessibilityObject obj = document.accessibilityObjectFromID(acc_obj_id);
  if (obj.isDetached()) {
#ifndef NDEBUG
    if (logging_)
      LOG(WARNING) << "ScrollToMakeVisible on invalid object id " << acc_obj_id;
#endif
    return;
  }

  obj.scrollToMakeVisibleWithSubFocus(
      WebRect(subfocus.x(), subfocus.y(),
              subfocus.width(), subfocus.height()));

  // Make sure the browser gets a notification when the scroll
  // position actually changes.
  // TODO(dmazzoni): remove this once this bug is fixed:
  // https://bugs.webkit.org/show_bug.cgi?id=73460
  HandleAccessibilityNotification(
      document.accessibilityObject(),
      AccessibilityNotificationLayoutComplete);
}

void RendererAccessibilityComplete::OnScrollToPoint(
    int acc_obj_id, gfx::Point point) {
  const WebDocument& document = GetMainDocument();
  if (document.isNull())
    return;

  WebAccessibilityObject obj = document.accessibilityObjectFromID(acc_obj_id);
  if (obj.isDetached()) {
#ifndef NDEBUG
    if (logging_)
      LOG(WARNING) << "ScrollToPoint on invalid object id " << acc_obj_id;
#endif
    return;
  }

  obj.scrollToGlobalPoint(WebPoint(point.x(), point.y()));

  // Make sure the browser gets a notification when the scroll
  // position actually changes.
  // TODO(dmazzoni): remove this once this bug is fixed:
  // https://bugs.webkit.org/show_bug.cgi?id=73460
  HandleAccessibilityNotification(
      document.accessibilityObject(),
      AccessibilityNotificationLayoutComplete);
}

void RendererAccessibilityComplete::OnSetTextSelection(
    int acc_obj_id, int start_offset, int end_offset) {
  const WebDocument& document = GetMainDocument();
  if (document.isNull())
    return;

  WebAccessibilityObject obj = document.accessibilityObjectFromID(acc_obj_id);
  if (obj.isDetached()) {
#ifndef NDEBUG
    if (logging_)
      LOG(WARNING) << "SetTextSelection on invalid object id " << acc_obj_id;
#endif
    return;
  }

  // TODO(dmazzoni): support elements other than <input>.
  WebKit::WebNode node = obj.node();
  if (!node.isNull() && node.isElementNode()) {
    WebKit::WebElement element = node.to<WebKit::WebElement>();
    WebKit::WebInputElement* input_element =
        WebKit::toWebInputElement(&element);
    if (input_element && input_element->isTextField())
      input_element->setSelectionRange(start_offset, end_offset);
  }
}

void RendererAccessibilityComplete::OnNotificationsAck() {
  DCHECK(ack_pending_);
  ack_pending_ = false;
  SendPendingAccessibilityNotifications();
}

void RendererAccessibilityComplete::OnSetFocus(int acc_obj_id) {
  const WebDocument& document = GetMainDocument();
  if (document.isNull())
    return;

  WebAccessibilityObject obj = document.accessibilityObjectFromID(acc_obj_id);
  if (obj.isDetached()) {
#ifndef NDEBUG
    if (logging_) {
      LOG(WARNING) << "OnSetAccessibilityFocus on invalid object id "
                   << acc_obj_id;
    }
#endif
    return;
  }

  WebAccessibilityObject root = document.accessibilityObject();
  if (root.isDetached()) {
#ifndef NDEBUG
    if (logging_) {
      LOG(WARNING) << "OnSetAccessibilityFocus but root is invalid";
    }
#endif
    return;
  }

  // By convention, calling SetFocus on the root of the tree should clear the
  // current focus. Otherwise set the focus to the new node.
  if (acc_obj_id == root.axID())
    render_view()->GetWebView()->clearFocusedNode();
  else
    obj.setFocused(true);
}

void RendererAccessibilityComplete::OnFatalError() {
  CHECK(false) << "Invalid accessibility tree.";
}

}  // namespace content
