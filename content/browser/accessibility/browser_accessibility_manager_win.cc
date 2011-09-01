// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/accessibility/browser_accessibility_manager_win.h"

#include "content/browser/accessibility/browser_accessibility_win.h"
#include "content/common/view_messages.h"

using webkit_glue::WebAccessibility;

// static
BrowserAccessibilityManager* BrowserAccessibilityManager::Create(
    gfx::NativeView parent_view,
    const WebAccessibility& src,
    BrowserAccessibilityDelegate* delegate,
    BrowserAccessibilityFactory* factory) {
  return new BrowserAccessibilityManagerWin(
      parent_view,
      src,
      delegate,
      factory);
}

BrowserAccessibilityManagerWin*
BrowserAccessibilityManager::toBrowserAccessibilityManagerWin() {
  return static_cast<BrowserAccessibilityManagerWin*>(this);
}

BrowserAccessibilityManagerWin::BrowserAccessibilityManagerWin(
    HWND parent_view,
    const WebAccessibility& src,
    BrowserAccessibilityDelegate* delegate,
    BrowserAccessibilityFactory* factory)
    : BrowserAccessibilityManager(parent_view, src, delegate, factory) {
  // Allow NULL parent_view for unit testing.
  if (parent_view == NULL) {
    window_iaccessible_ = NULL;
    return;
  }

  HRESULT hr = ::CreateStdAccessibleObject(
      parent_view, OBJID_WINDOW, IID_IAccessible,
      reinterpret_cast<void **>(&window_iaccessible_));
  DCHECK(SUCCEEDED(hr));
}

BrowserAccessibilityManagerWin::~BrowserAccessibilityManagerWin() {
}

IAccessible* BrowserAccessibilityManagerWin::GetParentWindowIAccessible() {
  return window_iaccessible_;
}

void BrowserAccessibilityManagerWin::NotifyAccessibilityEvent(
    int type,
    BrowserAccessibility* node) {
  LONG event_id = EVENT_MIN;
  switch (type) {
    case ViewHostMsg_AccEvent::ACTIVE_DESCENDANT_CHANGED:
      event_id = IA2_EVENT_ACTIVE_DESCENDANT_CHANGED;
      break;
    case ViewHostMsg_AccEvent::CHECK_STATE_CHANGED:
      event_id = EVENT_OBJECT_STATECHANGE;
      break;
    case ViewHostMsg_AccEvent::CHILDREN_CHANGED:
      event_id = EVENT_OBJECT_REORDER;
      break;
    case ViewHostMsg_AccEvent::FOCUS_CHANGED:
      event_id = EVENT_OBJECT_FOCUS;
      break;
    case ViewHostMsg_AccEvent::LOAD_COMPLETE:
      event_id = IA2_EVENT_DOCUMENT_LOAD_COMPLETE;
      break;
    case ViewHostMsg_AccEvent::VALUE_CHANGED:
      event_id = EVENT_OBJECT_VALUECHANGE;
      break;
    case ViewHostMsg_AccEvent::SELECTED_TEXT_CHANGED:
      event_id = IA2_EVENT_TEXT_CARET_MOVED;
      break;
    case ViewHostMsg_AccEvent::LIVE_REGION_CHANGED:
      event_id = EVENT_OBJECT_REORDER;
      break;
    case ViewHostMsg_AccEvent::TEXT_INSERTED:
      event_id = IA2_EVENT_TEXT_INSERTED;
      break;
    case ViewHostMsg_AccEvent::TEXT_REMOVED:
      event_id = IA2_EVENT_TEXT_REMOVED;
      break;
    case ViewHostMsg_AccEvent::OBJECT_SHOW:
      event_id = EVENT_OBJECT_SHOW;
      break;
    case ViewHostMsg_AccEvent::OBJECT_HIDE:
      event_id = EVENT_OBJECT_HIDE;
      break;
    case ViewHostMsg_AccEvent::ALERT:
      event_id = EVENT_SYSTEM_ALERT;
      break;
    default:
      // Not all WebKit accessibility events result in a Windows
      // accessibility notification.
      return;
  }

  NotifyWinEvent(event_id, GetParentView(), OBJID_CLIENT, node->child_id());
}
