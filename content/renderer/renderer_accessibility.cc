// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "content/common/view_messages.h"
#include "content/public/common/content_switches.h"
#include "content/renderer/render_view_impl.h"
#include "content/renderer/renderer_accessibility.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebAccessibilityObject.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDocument.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebView.h"
#include "webkit/glue/webaccessibility.h"

using WebKit::WebAccessibilityNotification;
using WebKit::WebAccessibilityObject;
using WebKit::WebDocument;
using WebKit::WebFrame;
using WebKit::WebNode;
using WebKit::WebView;
using webkit_glue::WebAccessibility;

bool WebAccessibilityNotificationToViewHostMsg(
    WebAccessibilityNotification notification,
    ViewHostMsg_AccEvent::Value* type) {
  switch (notification) {
    case WebKit::WebAccessibilityNotificationActiveDescendantChanged:
      *type = ViewHostMsg_AccEvent::ACTIVE_DESCENDANT_CHANGED;
      break;
    case WebKit::WebAccessibilityNotificationCheckedStateChanged:
      *type = ViewHostMsg_AccEvent::CHECK_STATE_CHANGED;
      break;
    case WebKit::WebAccessibilityNotificationChildrenChanged:
      *type = ViewHostMsg_AccEvent::CHILDREN_CHANGED;
      break;
    case WebKit::WebAccessibilityNotificationFocusedUIElementChanged:
      *type = ViewHostMsg_AccEvent::FOCUS_CHANGED;
      break;
    case WebKit::WebAccessibilityNotificationLayoutComplete:
      *type = ViewHostMsg_AccEvent::LAYOUT_COMPLETE;
      break;
    case WebKit::WebAccessibilityNotificationLiveRegionChanged:
      *type = ViewHostMsg_AccEvent::LIVE_REGION_CHANGED;
      break;
    case WebKit::WebAccessibilityNotificationLoadComplete:
      *type = ViewHostMsg_AccEvent::LOAD_COMPLETE;
      break;
    case WebKit::WebAccessibilityNotificationMenuListValueChanged:
      *type = ViewHostMsg_AccEvent::MENU_LIST_VALUE_CHANGED;
      break;
    case WebKit::WebAccessibilityNotificationRowCollapsed:
      *type = ViewHostMsg_AccEvent::ROW_COLLAPSED;
      break;
    case WebKit::WebAccessibilityNotificationRowCountChanged:
      *type = ViewHostMsg_AccEvent::ROW_COUNT_CHANGED;
      break;
    case WebKit::WebAccessibilityNotificationRowExpanded:
      *type = ViewHostMsg_AccEvent::ROW_EXPANDED;
      break;
    case WebKit::WebAccessibilityNotificationScrolledToAnchor:
      *type = ViewHostMsg_AccEvent::SCROLLED_TO_ANCHOR;
      break;
    case WebKit::WebAccessibilityNotificationSelectedChildrenChanged:
      *type = ViewHostMsg_AccEvent::SELECTED_CHILDREN_CHANGED;
      break;
    case WebKit::WebAccessibilityNotificationSelectedTextChanged:
      *type = ViewHostMsg_AccEvent::SELECTED_TEXT_CHANGED;
      break;
    case WebKit::WebAccessibilityNotificationValueChanged:
      *type = ViewHostMsg_AccEvent::VALUE_CHANGED;
      break;
    default:
      NOTREACHED();
      return false;
  }
  return true;
}

RendererAccessibility::RendererAccessibility(RenderViewImpl* render_view)
    : content::RenderViewObserver(render_view),
      ALLOW_THIS_IN_INITIALIZER_LIST(method_factory_(this)),
      browser_root_(NULL),
      last_scroll_offset_(gfx::Size()),
      ack_pending_(false),
      logging_(false),
      sent_load_complete_(false) {
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  if (command_line.HasSwitch(switches::kEnableAccessibility))
    WebAccessibilityObject::enableAccessibility();
  if (command_line.HasSwitch(switches::kEnableAccessibilityLogging))
    logging_ = true;
}

RendererAccessibility::~RendererAccessibility() {
}

bool RendererAccessibility::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(RendererAccessibility, message)
    IPC_MESSAGE_HANDLER(ViewMsg_EnableAccessibility, OnEnableAccessibility)
    IPC_MESSAGE_HANDLER(ViewMsg_SetAccessibilityFocus, OnSetAccessibilityFocus)
    IPC_MESSAGE_HANDLER(ViewMsg_AccessibilityDoDefaultAction,
                        OnAccessibilityDoDefaultAction)
    IPC_MESSAGE_HANDLER(ViewMsg_AccessibilityNotifications_ACK,
                        OnAccessibilityNotificationsAck)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void RendererAccessibility::FocusedNodeChanged(const WebNode& node) {
  if (!WebAccessibilityObject::accessibilityEnabled())
    return;

  const WebDocument& document = GetMainDocument();
  if (document.isNull())
    return;

  if (node.isNull()) {
    // When focus is cleared, implicitly focus the document.
    // TODO(dmazzoni): Make WebKit send this notification instead.
    PostAccessibilityNotification(
        document.accessibilityObject(),
        WebKit::WebAccessibilityNotificationFocusedUIElementChanged);
  }
}

void RendererAccessibility::DidFinishLoad(WebKit::WebFrame* frame) {
  if (!WebAccessibilityObject::accessibilityEnabled())
    return;

  const WebDocument& document = GetMainDocument();
  if (document.isNull())
    return;

  // Check to see if the root accessibility object has changed, to work
  // around WebKit bugs that cause AXObjectCache to be cleared
  // unnecessarily.
  // TODO(dmazzoni): remove this once rdar://5794454 is fixed.
  WebAccessibilityObject new_root = document.accessibilityObject();
  if (!browser_root_ || new_root.axID() != browser_root_->id) {
    PostAccessibilityNotification(
        new_root,
        WebKit::WebAccessibilityNotificationLoadComplete);
  }
}

void RendererAccessibility::PostAccessibilityNotification(
    const WebAccessibilityObject& obj,
    WebAccessibilityNotification notification) {
  if (!WebAccessibilityObject::accessibilityEnabled())
    return;

  const WebDocument& document = GetMainDocument();
  if (document.isNull())
    return;

  if (notification != WebKit::WebAccessibilityNotificationLoadComplete &&
      !sent_load_complete_) {
    // Load complete should be our first notification sent. Send it manually
    // in cases where we don't get it first to avoid focus problems.
    PostAccessibilityNotification(
        document.accessibilityObject(),
        WebKit::WebAccessibilityNotificationLoadComplete);
  }

  gfx::Size scroll_offset = document.frame()->scrollOffset();
  if (scroll_offset != last_scroll_offset_) {
    // Make sure the browser is always aware of the scroll position of
    // the root document element by posting a generic notification that
    // will update it.
    // TODO(dmazzoni): remove this as soon as
    // https://bugs.webkit.org/show_bug.cgi?id=73460 is fixed.
    last_scroll_offset_ = scroll_offset;
    PostAccessibilityNotification(
        document.accessibilityObject(),
        WebKit::WebAccessibilityNotificationLayoutComplete);
  }

  if (notification == WebKit::WebAccessibilityNotificationLoadComplete)
    sent_load_complete_ = true;

  // Add the accessibility object to our cache and ensure it's valid.
  Notification acc_notification;
  acc_notification.id = obj.axID();
  acc_notification.type = notification;

  ViewHostMsg_AccEvent::Value temp;
  if (!WebAccessibilityNotificationToViewHostMsg(notification, &temp))
    return;

  // Discard duplicate accessibility notifications.
  for (uint32 i = 0; i < pending_notifications_.size(); ++i) {
    if (pending_notifications_[i].id == acc_notification.id &&
        pending_notifications_[i].type == acc_notification.type) {
      return;
    }
  }
  pending_notifications_.push_back(acc_notification);

  if (!ack_pending_ && method_factory_.empty()) {
    // When no accessibility notifications are in-flight post a task to send
    // the notifications to the browser. We use PostTask so that we can queue
    // up additional notifications.
    MessageLoop::current()->PostTask(
        FROM_HERE,
        method_factory_.NewRunnableMethod(
            &RendererAccessibility::SendPendingAccessibilityNotifications));
  }
}

void RendererAccessibility::SendPendingAccessibilityNotifications() {
  const WebDocument& document = GetMainDocument();
  if (document.isNull())
    return;

  if (pending_notifications_.empty())
    return;

  // Send all pending accessibility notifications.
  std::vector<ViewHostMsg_AccessibilityNotification_Params> notifications;
  for (size_t i = 0; i < pending_notifications_.size(); ++i) {
    Notification& notification = pending_notifications_[i];

    bool includes_children = ShouldIncludeChildren(notification);
    WebAccessibilityObject obj = document.accessibilityObjectFromID(
        notification.id);

    // The browser may not have this object yet, for example if we get a
    // notification on an object that was recently added, or if we get a
    // notification on a node before the page has loaded. Work our way
    // up the parent chain until we find a node the browser has, or until
    // we reach the root.
    int root_id = document.accessibilityObject().axID();
    while (browser_id_map_.find(obj.axID()) == browser_id_map_.end() &&
           obj.isValid() &&
           obj.axID() != root_id) {
      obj = obj.parentObject();
      includes_children = true;
      if (notification.type ==
          WebKit::WebAccessibilityNotificationChildrenChanged) {
        notification.id = obj.axID();
      }
    }

    if (!obj.isValid()) {
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
      while (!parent.isNull() &&
             parent.isValid() &&
             parent.accessibilityIsIgnored()) {
        parent = parent.parentObject();
      }

      if (parent.isNull() || !parent.isValid()) {
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
        includes_children = true;
      }
    }

    ViewHostMsg_AccessibilityNotification_Params param;
    WebAccessibilityNotificationToViewHostMsg(
        notification.type, &param.notification_type);
    param.id = notification.id;
    param.includes_children = includes_children;
    param.acc_tree = WebAccessibility(obj, includes_children);
    if (obj.axID() == root_id) {
      DCHECK_EQ(param.acc_tree.role, WebAccessibility::ROLE_WEB_AREA);
      param.acc_tree.role = WebAccessibility::ROLE_ROOT_WEB_AREA;
    }
    notifications.push_back(param);

    if (includes_children)
      UpdateBrowserTree(param.acc_tree);

#ifndef NDEBUG
    if (logging_) {
      LOG(INFO) << "Accessibility update: "
                << param.acc_tree.DebugString(true,
                                              routing_id(),
                                              notification.type);
    }
#endif
  }
  pending_notifications_.clear();
  Send(new ViewHostMsg_AccessibilityNotifications(routing_id(), notifications));
  ack_pending_ = true;
}

void RendererAccessibility::UpdateBrowserTree(
    const webkit_glue::WebAccessibility& renderer_node) {
  BrowserTreeNode* browser_node = NULL;
  base::hash_map<int32, BrowserTreeNode*>::iterator iter =
      browser_id_map_.find(renderer_node.id);
  if (iter != browser_id_map_.end()) {
    browser_node = iter->second;
    ClearBrowserTreeNode(browser_node);
  } else {
    DCHECK_EQ(renderer_node.role, WebAccessibility::ROLE_ROOT_WEB_AREA);
    if (browser_root_) {
      ClearBrowserTreeNode(browser_root_);
      browser_id_map_.erase(browser_root_->id);
      delete browser_root_;
    }
    browser_root_ = new BrowserTreeNode;
    browser_node = browser_root_;
    browser_node->id = renderer_node.id;
    browser_id_map_[browser_node->id] = browser_node;
  }
  browser_node->children.reserve(renderer_node.children.size());
  for (size_t i = 0; i < renderer_node.children.size(); ++i) {
    BrowserTreeNode* browser_child_node = new BrowserTreeNode;
    browser_child_node->id = renderer_node.children[i].id;
    browser_id_map_[browser_child_node->id] = browser_child_node;
    browser_node->children.push_back(browser_child_node);
    UpdateBrowserTree(renderer_node.children[i]);
  }
}

void RendererAccessibility::ClearBrowserTreeNode(
    BrowserTreeNode* browser_node) {
  for (size_t i = 0; i < browser_node->children.size(); ++i) {
    browser_id_map_.erase(browser_node->children[i]->id);
    ClearBrowserTreeNode(browser_node->children[i]);
    delete browser_node->children[i];
  }
  browser_node->children.clear();
}

void RendererAccessibility::OnAccessibilityDoDefaultAction(int acc_obj_id) {
  if (!WebAccessibilityObject::accessibilityEnabled())
    return;

  const WebDocument& document = GetMainDocument();
  if (document.isNull())
    return;

  WebAccessibilityObject obj = document.accessibilityObjectFromID(acc_obj_id);
  if (!obj.isValid()) {
#ifndef NDEBUG
    if (logging_)
      LOG(WARNING) << "DoDefaultAction on invalid object id " << acc_obj_id;
#endif
    return;
  }

  obj.performDefaultAction();
}

void RendererAccessibility::OnAccessibilityNotificationsAck() {
  DCHECK(ack_pending_);
  ack_pending_ = false;
  SendPendingAccessibilityNotifications();
}

void RendererAccessibility::OnEnableAccessibility() {
  if (WebAccessibilityObject::accessibilityEnabled())
    return;

  WebAccessibilityObject::enableAccessibility();

  const WebDocument& document = GetMainDocument();
  if (!document.isNull()) {
    // It's possible that the webview has already loaded a webpage without
    // accessibility being enabled. Initialize the browser's cached
    // accessibility tree by sending it a 'load complete' notification.
    PostAccessibilityNotification(
        document.accessibilityObject(),
        WebKit::WebAccessibilityNotificationLoadComplete);
  }
}

void RendererAccessibility::OnSetAccessibilityFocus(int acc_obj_id) {
  if (!WebAccessibilityObject::accessibilityEnabled())
    return;

  const WebDocument& document = GetMainDocument();
  if (document.isNull())
    return;

  WebAccessibilityObject obj = document.accessibilityObjectFromID(acc_obj_id);
  if (!obj.isValid()) {
#ifndef NDEBUG
    if (logging_) {
      LOG(WARNING) << "OnSetAccessibilityFocus on invalid object id "
                   << acc_obj_id;
    }
#endif
    return;
  }

  WebAccessibilityObject root = document.accessibilityObject();
  if (!root.isValid()) {
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

bool RendererAccessibility::ShouldIncludeChildren(
    const RendererAccessibility::Notification& notification) {
  WebKit::WebAccessibilityNotification type = notification.type;
  if (type == WebKit::WebAccessibilityNotificationChildrenChanged ||
      type == WebKit::WebAccessibilityNotificationLoadComplete ||
      type == WebKit::WebAccessibilityNotificationLiveRegionChanged) {
    return true;
  }
  return false;
}

WebDocument RendererAccessibility::GetMainDocument() {
  WebView* view = render_view()->GetWebView();
  WebFrame* main_frame = view ? view->mainFrame() : NULL;

  if (main_frame)
    return main_frame->document();
  else
    return WebDocument();
}
