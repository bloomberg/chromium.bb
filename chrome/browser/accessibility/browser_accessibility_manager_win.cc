// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/accessibility/browser_accessibility_manager_win.h"

#include "chrome/browser/accessibility/browser_accessibility_win.h"
#include "chrome/common/render_messages_params.h"

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
  LONG event_id;
  switch (type) {
    case ViewHostMsg_AccessibilityNotification_Params::
          NOTIFICATION_TYPE_CHECK_STATE_CHANGED:
      event_id = EVENT_OBJECT_STATECHANGE;
      break;
    case ViewHostMsg_AccessibilityNotification_Params::
          NOTIFICATION_TYPE_CHILDREN_CHANGED:
      event_id = EVENT_OBJECT_REORDER;
      break;
    case ViewHostMsg_AccessibilityNotification_Params::
          NOTIFICATION_TYPE_FOCUS_CHANGED:
      event_id = EVENT_OBJECT_FOCUS;
      break;
    case ViewHostMsg_AccessibilityNotification_Params::
          NOTIFICATION_TYPE_LOAD_COMPLETE:
      event_id = IA2_EVENT_DOCUMENT_LOAD_COMPLETE;
      break;
    case ViewHostMsg_AccessibilityNotification_Params::
          NOTIFICATION_TYPE_VALUE_CHANGED:
      event_id = EVENT_OBJECT_VALUECHANGE;
      break;
    case ViewHostMsg_AccessibilityNotification_Params::
          NOTIFICATION_TYPE_SELECTED_TEXT_CHANGED:
      event_id = IA2_EVENT_TEXT_CARET_MOVED;
  }

  NotifyWinEvent(event_id, GetParentView(), OBJID_CLIENT, node->child_id());
}
