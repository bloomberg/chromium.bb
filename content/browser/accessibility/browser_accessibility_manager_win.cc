// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/accessibility/browser_accessibility_manager_win.h"

#include "content/browser/accessibility/browser_accessibility_win.h"
#include "content/common/accessibility_messages.h"

namespace content {

// static
BrowserAccessibilityManager* BrowserAccessibilityManager::Create(
    const AccessibilityNodeData& src,
    BrowserAccessibilityDelegate* delegate,
    BrowserAccessibilityFactory* factory) {
  return new BrowserAccessibilityManagerWin(
      GetDesktopWindow(), NULL, src, delegate, factory);
}

BrowserAccessibilityManagerWin*
BrowserAccessibilityManager::ToBrowserAccessibilityManagerWin() {
  return static_cast<BrowserAccessibilityManagerWin*>(this);
}

BrowserAccessibilityManagerWin::BrowserAccessibilityManagerWin(
    HWND parent_hwnd,
    IAccessible* parent_iaccessible,
    const AccessibilityNodeData& src,
    BrowserAccessibilityDelegate* delegate,
    BrowserAccessibilityFactory* factory)
    : BrowserAccessibilityManager(src, delegate, factory),
      parent_hwnd_(parent_hwnd),
      parent_iaccessible_(parent_iaccessible),
      tracked_scroll_object_(NULL) {
}

BrowserAccessibilityManagerWin::~BrowserAccessibilityManagerWin() {
  if (tracked_scroll_object_) {
    tracked_scroll_object_->Release();
    tracked_scroll_object_ = NULL;
  }
}

// static
AccessibilityNodeData BrowserAccessibilityManagerWin::GetEmptyDocument() {
  AccessibilityNodeData empty_document;
  empty_document.id = 0;
  empty_document.role = WebKit::WebAXRoleRootWebArea;
  empty_document.state =
      (1 << WebKit::WebAXStateEnabled) |
      (1 << WebKit::WebAXStateReadonly) |
      (1 << WebKit::WebAXStateBusy);
  return empty_document;
}

void BrowserAccessibilityManagerWin::MaybeCallNotifyWinEvent(DWORD event,
                                                             LONG child_id) {
  if (parent_iaccessible())
    ::NotifyWinEvent(event, parent_hwnd(), OBJID_CLIENT, child_id);
}

void BrowserAccessibilityManagerWin::AddNodeToMap(BrowserAccessibility* node) {
  BrowserAccessibilityManager::AddNodeToMap(node);
  LONG unique_id_win = node->ToBrowserAccessibilityWin()->unique_id_win();
  unique_id_to_renderer_id_map_[unique_id_win] = node->renderer_id();
}

void BrowserAccessibilityManagerWin::RemoveNode(BrowserAccessibility* node) {
  unique_id_to_renderer_id_map_.erase(
      node->ToBrowserAccessibilityWin()->unique_id_win());
  BrowserAccessibilityManager::RemoveNode(node);
  if (node == tracked_scroll_object_) {
    tracked_scroll_object_->Release();
    tracked_scroll_object_ = NULL;
  }
}

void BrowserAccessibilityManagerWin::NotifyAccessibilityEvent(
    WebKit::WebAXEvent event_type,
    BrowserAccessibility* node) {
  LONG event_id = EVENT_MIN;
  switch (event_type) {
    case WebKit::WebAXEventActiveDescendantChanged:
      event_id = IA2_EVENT_ACTIVE_DESCENDANT_CHANGED;
      break;
    case WebKit::WebAXEventAlert:
      event_id = EVENT_SYSTEM_ALERT;
      break;
    case WebKit::WebAXEventAriaAttributeChanged:
      event_id = IA2_EVENT_OBJECT_ATTRIBUTE_CHANGED;
      break;
    case WebKit::WebAXEventAutocorrectionOccured:
      event_id = IA2_EVENT_OBJECT_ATTRIBUTE_CHANGED;
      break;
    case WebKit::WebAXEventBlur:
      // Equivalent to focus on the root.
      event_id = EVENT_OBJECT_FOCUS;
      node = GetRoot();
      break;
    case WebKit::WebAXEventCheckedStateChanged:
      event_id = EVENT_OBJECT_STATECHANGE;
      break;
    case WebKit::WebAXEventChildrenChanged:
      event_id = EVENT_OBJECT_REORDER;
      break;
    case WebKit::WebAXEventFocus:
      event_id = EVENT_OBJECT_FOCUS;
      break;
    case WebKit::WebAXEventInvalidStatusChanged:
      event_id = EVENT_OBJECT_STATECHANGE;
      break;
    case WebKit::WebAXEventLiveRegionChanged:
      // TODO: try not firing a native notification at all, since
      // on Windows, each individual item in a live region that changes
      // already gets its own notification.
      event_id = EVENT_OBJECT_REORDER;
      break;
    case WebKit::WebAXEventLoadComplete:
      event_id = IA2_EVENT_DOCUMENT_LOAD_COMPLETE;
      break;
    case WebKit::WebAXEventMenuListItemSelected:
      event_id = EVENT_OBJECT_FOCUS;
      break;
    case WebKit::WebAXEventMenuListValueChanged:
      event_id = EVENT_OBJECT_VALUECHANGE;
      break;
    case WebKit::WebAXEventHide:
      event_id = EVENT_OBJECT_HIDE;
      break;
    case WebKit::WebAXEventShow:
      event_id = EVENT_OBJECT_SHOW;
      break;
    case WebKit::WebAXEventScrolledToAnchor:
      event_id = EVENT_SYSTEM_SCROLLINGSTART;
      break;
    case WebKit::WebAXEventSelectedChildrenChanged:
      event_id = EVENT_OBJECT_SELECTIONWITHIN;
      break;
    case WebKit::WebAXEventSelectedTextChanged:
      event_id = IA2_EVENT_TEXT_CARET_MOVED;
      break;
    case WebKit::WebAXEventTextChanged:
      event_id = EVENT_OBJECT_NAMECHANGE;
      break;
    case WebKit::WebAXEventTextInserted:
      event_id = IA2_EVENT_TEXT_INSERTED;
      break;
    case WebKit::WebAXEventTextRemoved:
      event_id = IA2_EVENT_TEXT_REMOVED;
      break;
    case WebKit::WebAXEventValueChanged:
      event_id = EVENT_OBJECT_VALUECHANGE;
      break;
    default:
      // Not all WebKit accessibility events result in a Windows
      // accessibility notification.
      break;
  }

  if (event_id != EVENT_MIN) {
    // Pass the node's unique id in the |child_id| argument to NotifyWinEvent;
    // the AT client will then call get_accChild on the HWND's accessibility
    // object and pass it that same id, which we can use to retrieve the
    // IAccessible for this node.
    LONG child_id = node->ToBrowserAccessibilityWin()->unique_id_win();
    MaybeCallNotifyWinEvent(event_id, child_id);
  }

  // If this is a layout complete notification (sent when a container scrolls)
  // and there is a descendant tracked object, send a notification on it.
  // TODO(dmazzoni): remove once http://crbug.com/113483 is fixed.
  if (event_type == WebKit::WebAXEventLayoutComplete &&
      tracked_scroll_object_ &&
      tracked_scroll_object_->IsDescendantOf(node)) {
    MaybeCallNotifyWinEvent(
        IA2_EVENT_VISIBLE_DATA_CHANGED,
        tracked_scroll_object_->ToBrowserAccessibilityWin()->unique_id_win());
    tracked_scroll_object_->Release();
    tracked_scroll_object_ = NULL;
  }
}

void BrowserAccessibilityManagerWin::TrackScrollingObject(
    BrowserAccessibilityWin* node) {
  if (tracked_scroll_object_)
    tracked_scroll_object_->Release();
  tracked_scroll_object_ = node;
  tracked_scroll_object_->AddRef();
}

BrowserAccessibilityWin* BrowserAccessibilityManagerWin::GetFromUniqueIdWin(
    LONG unique_id_win) {
  base::hash_map<LONG, int32>::iterator iter =
      unique_id_to_renderer_id_map_.find(unique_id_win);
  if (iter != unique_id_to_renderer_id_map_.end()) {
    BrowserAccessibility* result = GetFromRendererID(iter->second);
    if (result)
      return result->ToBrowserAccessibilityWin();
  }
  return NULL;
}

}  // namespace content
