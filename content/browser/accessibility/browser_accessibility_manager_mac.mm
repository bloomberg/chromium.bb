// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/accessibility/browser_accessibility_manager_mac.h"

#import "base/logging.h"
#import "content/browser/accessibility/browser_accessibility_cocoa.h"
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
    case ViewHostMsg_AccEvent::ACTIVE_DESCENDANT_CHANGED:
      if (node->role() == WebAccessibility::ROLE_TREE)
        event_id = NSAccessibilitySelectedRowsChangedNotification;
      else
        event_id = NSAccessibilityFocusedUIElementChangedNotification;
    case ViewHostMsg_AccEvent::ALERT:
      // Not used on Mac.
      return;
    case ViewHostMsg_AccEvent::CHECK_STATE_CHANGED:
      // Not used on Mac.
      return;
    case ViewHostMsg_AccEvent::CHILDREN_CHANGED:
      // TODO(dtseng): no clear equivalent on Mac.
      return;
    case ViewHostMsg_AccEvent::FOCUS_CHANGED:
      event_id = NSAccessibilityFocusedUIElementChangedNotification;
      break;
    case ViewHostMsg_AccEvent::LAYOUT_COMPLETE:
      event_id = @"AXLayoutComplete";
      break;
    case ViewHostMsg_AccEvent::LIVE_REGION_CHANGED:
      event_id = @"AXLiveRegionChanged";
      break;
    case ViewHostMsg_AccEvent::LOAD_COMPLETE:
      event_id = @"AXLoadComplete";
      break;
    case ViewHostMsg_AccEvent::MENU_LIST_VALUE_CHANGED:
      // Not used on Mac.
      return;
    case ViewHostMsg_AccEvent::OBJECT_SHOW:
      // Not used on Mac.
      return;
    case ViewHostMsg_AccEvent::OBJECT_HIDE:
      // Not used on Mac.
      return;
    case ViewHostMsg_AccEvent::ROW_COUNT_CHANGED:
      event_id = NSAccessibilityRowCountChangedNotification;
      break;
    case ViewHostMsg_AccEvent::ROW_COLLAPSED:
      event_id = @"AXRowCollapsed";
      break;
    case ViewHostMsg_AccEvent::ROW_EXPANDED:
      event_id = @"AXRowExpanded";
      break;
    case ViewHostMsg_AccEvent::SCROLLED_TO_ANCHOR:
      // Not used on Mac.
      return;
    case ViewHostMsg_AccEvent::SELECTED_CHILDREN_CHANGED:
      event_id = NSAccessibilitySelectedChildrenChangedNotification;
      break;
    case ViewHostMsg_AccEvent::SELECTED_TEXT_CHANGED:
      event_id = NSAccessibilitySelectedTextChangedNotification;
      break;
    case ViewHostMsg_AccEvent::TEXT_INSERTED:
      // Not used on Mac.
      return;
    case ViewHostMsg_AccEvent::TEXT_REMOVED:
      // Not used on Mac.
      return;
    case ViewHostMsg_AccEvent::VALUE_CHANGED:
      event_id = NSAccessibilityValueChangedNotification;
      break;
  }
  BrowserAccessibilityCocoa* native_node = node->toBrowserAccessibilityCocoa();
  DCHECK(native_node);
  NSAccessibilityPostNotification(native_node, event_id);
}
