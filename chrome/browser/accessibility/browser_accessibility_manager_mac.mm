// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/accessibility/browser_accessibility_manager_mac.h"

#import "base/logging.h"
#import "chrome/browser/accessibility/browser_accessibility_cocoa.h"
#include "content/common/view_messages.h"

// static
BrowserAccessibilityManager* BrowserAccessibilityManager::Create(
    gfx::NativeView parent_view,
    const WebAccessibility& src,
    BrowserAccessibilityDelegate* delegate,
    BrowserAccessibilityFactory* factory) {
  return new BrowserAccessibilityManagerMac(
      parent_view, src, delegate, factory);
}

BrowserAccessibilityManagerMac::BrowserAccessibilityManagerMac(
    gfx::NativeView parent_window,
    const webkit_glue::WebAccessibility& src,
    BrowserAccessibilityDelegate* delegate,
    BrowserAccessibilityFactory* factory)
        : BrowserAccessibilityManager(parent_window, src, delegate, factory) {
}

void BrowserAccessibilityManagerMac::NotifyAccessibilityEvent(
    int type,
    BrowserAccessibility* node) {
  // Refer to AXObjectCache.mm (webkit).
  NSString* event_id = @"";
  switch (type) {
    case ViewHostMsg_AccessibilityNotification_Type::
        NOTIFICATION_TYPE_CHECK_STATE_CHANGED:
      // Does not exist on Mac.
      return;
    case ViewHostMsg_AccessibilityNotification_Type::
        NOTIFICATION_TYPE_CHILDREN_CHANGED:
      // TODO(dtseng): no clear equivalent on Mac.
      return;
    case ViewHostMsg_AccessibilityNotification_Type::
        NOTIFICATION_TYPE_FOCUS_CHANGED:
      event_id = NSAccessibilityFocusedUIElementChangedNotification;
      break;
    case ViewHostMsg_AccessibilityNotification_Type::
        NOTIFICATION_TYPE_LOAD_COMPLETE:
      event_id = @"AXLoadComplete";
      break;
    case ViewHostMsg_AccessibilityNotification_Type::
        NOTIFICATION_TYPE_VALUE_CHANGED:
      event_id = NSAccessibilityValueChangedNotification;
      break;
    case ViewHostMsg_AccessibilityNotification_Type::
        NOTIFICATION_TYPE_SELECTED_TEXT_CHANGED:
      event_id = NSAccessibilitySelectedTextChangedNotification;
      break;
  }
  BrowserAccessibilityCocoa* native_node = node->toBrowserAccessibilityCocoa();
  DCHECK(native_node);
  NSAccessibilityPostNotification(native_node, event_id);
}
