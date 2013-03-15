// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/accessibility/browser_accessibility_manager_mac.h"

#import "base/logging.h"
#import "content/browser/accessibility/browser_accessibility_cocoa.h"
#include "content/common/accessibility_messages.h"

namespace content {

// static
BrowserAccessibilityManager* BrowserAccessibilityManager::Create(
    const AccessibilityNodeData& src,
    BrowserAccessibilityDelegate* delegate,
    BrowserAccessibilityFactory* factory) {
  return new BrowserAccessibilityManagerMac(NULL, src, delegate, factory);
}

BrowserAccessibilityManagerMac::BrowserAccessibilityManagerMac(
    NSView* parent_view,
    const AccessibilityNodeData& src,
    BrowserAccessibilityDelegate* delegate,
    BrowserAccessibilityFactory* factory)
    : BrowserAccessibilityManager(src, delegate, factory),
      parent_view_(parent_view) {
}

// static
AccessibilityNodeData BrowserAccessibilityManagerMac::GetEmptyDocument() {
  AccessibilityNodeData empty_document;
  empty_document.id = 0;
  empty_document.role = AccessibilityNodeData::ROLE_ROOT_WEB_AREA;
  empty_document.state =
      1 << AccessibilityNodeData::STATE_READONLY;
  return empty_document;
}

void BrowserAccessibilityManagerMac::NotifyAccessibilityEvent(
    int type,
    BrowserAccessibility* node) {
  if (!node->IsNative())
    return;

  // Refer to AXObjectCache.mm (webkit).
  NSString* event_id = @"";
  switch (type) {
    case AccessibilityNotificationActiveDescendantChanged:
      if (node->role() == AccessibilityNodeData::ROLE_TREE)
        event_id = NSAccessibilitySelectedRowsChangedNotification;
      else
        event_id = NSAccessibilityFocusedUIElementChangedNotification;
      break;
    case AccessibilityNotificationAlert:
      // Not used on Mac.
      return;
    case AccessibilityNotificationBlur:
      // A no-op on Mac.
      return;
    case AccessibilityNotificationCheckStateChanged:
      // Not used on Mac.
      return;
    case AccessibilityNotificationChildrenChanged:
      // TODO(dtseng): no clear equivalent on Mac.
      return;
    case AccessibilityNotificationFocusChanged:
      event_id = NSAccessibilityFocusedUIElementChangedNotification;
      break;
    case AccessibilityNotificationLayoutComplete:
      event_id = @"AXLayoutComplete";
      break;
    case AccessibilityNotificationLiveRegionChanged:
      event_id = @"AXLiveRegionChanged";
      break;
    case AccessibilityNotificationLoadComplete:
      event_id = @"AXLoadComplete";
      break;
    case AccessibilityNotificationMenuListValueChanged:
      // Not used on Mac.
      return;
    case AccessibilityNotificationObjectShow:
      // Not used on Mac.
      return;
    case AccessibilityNotificationObjectHide:
      // Not used on Mac.
      return;
    case AccessibilityNotificationRowCountChanged:
      event_id = NSAccessibilityRowCountChangedNotification;
      break;
    case AccessibilityNotificationRowCollapsed:
      event_id = @"AXRowCollapsed";
      break;
    case AccessibilityNotificationRowExpanded:
      event_id = @"AXRowExpanded";
      break;
    case AccessibilityNotificationScrolledToAnchor:
      // Not used on Mac.
      return;
    case AccessibilityNotificationSelectedChildrenChanged:
      event_id = NSAccessibilitySelectedChildrenChangedNotification;
      break;
    case AccessibilityNotificationSelectedTextChanged:
      event_id = NSAccessibilitySelectedTextChangedNotification;
      break;
    case AccessibilityNotificationTextInserted:
      // Not used on Mac.
      return;
    case AccessibilityNotificationTextRemoved:
      // Not used on Mac.
      return;
    case AccessibilityNotificationValueChanged:
      event_id = NSAccessibilityValueChangedNotification;
      break;
  }
  BrowserAccessibilityCocoa* native_node = node->ToBrowserAccessibilityCocoa();
  DCHECK(native_node);
  NSAccessibilityPostNotification(native_node, event_id);
}

}  // namespace content
