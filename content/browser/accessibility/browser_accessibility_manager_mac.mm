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
  empty_document.role = WebKit::WebAXRoleRootWebArea;
  empty_document.state =
      1 << WebKit::WebAXStateReadonly;
  return empty_document;
}

void BrowserAccessibilityManagerMac::NotifyAccessibilityEvent(
    WebKit::WebAXEvent event_type,
    BrowserAccessibility* node) {
  if (!node->IsNative())
    return;

  // Refer to AXObjectCache.mm (webkit).
  NSString* event_id = @"";
  switch (event_type) {
    case WebKit::WebAXEventActiveDescendantChanged:
      if (node->role() == WebKit::WebAXRoleTree)
        event_id = NSAccessibilitySelectedRowsChangedNotification;
      else
        event_id = NSAccessibilityFocusedUIElementChangedNotification;
      break;
    case WebKit::WebAXEventAlert:
      // Not used on Mac.
      return;
    case WebKit::WebAXEventBlur:
      // A no-op on Mac.
      return;
    case WebKit::WebAXEventCheckedStateChanged:
      // Not used on Mac.
      return;
    case WebKit::WebAXEventChildrenChanged:
      // TODO(dtseng): no clear equivalent on Mac.
      return;
    case WebKit::WebAXEventFocus:
      event_id = NSAccessibilityFocusedUIElementChangedNotification;
      break;
    case WebKit::WebAXEventLayoutComplete:
      event_id = @"AXLayoutComplete";
      break;
    case WebKit::WebAXEventLiveRegionChanged:
      event_id = @"AXLiveRegionChanged";
      break;
    case WebKit::WebAXEventLoadComplete:
      event_id = @"AXLoadComplete";
      break;
    case WebKit::WebAXEventMenuListValueChanged:
      // Not used on Mac.
      return;
    case WebKit::WebAXEventShow:
      // Not used on Mac.
      return;
    case WebKit::WebAXEventHide:
      // Not used on Mac.
      return;
    case WebKit::WebAXEventRowCountChanged:
      event_id = NSAccessibilityRowCountChangedNotification;
      break;
    case WebKit::WebAXEventRowCollapsed:
      event_id = @"AXRowCollapsed";
      break;
    case WebKit::WebAXEventRowExpanded:
      event_id = @"AXRowExpanded";
      break;
    case WebKit::WebAXEventScrolledToAnchor:
      // Not used on Mac.
      return;
    case WebKit::WebAXEventSelectedChildrenChanged:
      event_id = NSAccessibilitySelectedChildrenChangedNotification;
      break;
    case WebKit::WebAXEventSelectedTextChanged:
      event_id = NSAccessibilitySelectedTextChangedNotification;
      break;
    case WebKit::WebAXEventTextInserted:
      // Not used on Mac.
      return;
    case WebKit::WebAXEventTextRemoved:
      // Not used on Mac.
      return;
    case WebKit::WebAXEventValueChanged:
      event_id = NSAccessibilityValueChangedNotification;
      break;
    case WebKit::WebAXEventAriaAttributeChanged:
      // Not used on Mac.
      return;
    case WebKit::WebAXEventAutocorrectionOccured:
      // Not used on Mac.
      return;
    case WebKit::WebAXEventInvalidStatusChanged:
      // Not used on Mac.
      return;
    case WebKit::WebAXEventLocationChanged:
      // Not used on Mac.
      return;
    case WebKit::WebAXEventMenuListItemSelected:
      // Not used on Mac.
      return;
    case WebKit::WebAXEventTextChanged:
      // Not used on Mac.
      return;
    default:
      LOG(WARNING) << "Unknown accessibility event: " << event_type;
      return;
  }
  BrowserAccessibilityCocoa* native_node = node->ToBrowserAccessibilityCocoa();
  DCHECK(native_node);
  NSAccessibilityPostNotification(native_node, event_id);
}

}  // namespace content
