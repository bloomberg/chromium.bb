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
  empty_document.role = blink::WebAXRoleRootWebArea;
  empty_document.state =
      1 << blink::WebAXStateReadonly;
  return empty_document;
}

void BrowserAccessibilityManagerMac::NotifyAccessibilityEvent(
    blink::WebAXEvent event_type,
    BrowserAccessibility* node) {
  if (!node->IsNative())
    return;

  // Refer to AXObjectCache.mm (webkit).
  NSString* event_id = @"";
  switch (event_type) {
    case blink::WebAXEventActiveDescendantChanged:
      if (node->role() == blink::WebAXRoleTree)
        event_id = NSAccessibilitySelectedRowsChangedNotification;
      else
        event_id = NSAccessibilityFocusedUIElementChangedNotification;
      break;
    case blink::WebAXEventAlert:
      // Not used on Mac.
      return;
    case blink::WebAXEventBlur:
      // A no-op on Mac.
      return;
    case blink::WebAXEventCheckedStateChanged:
      // Not used on Mac.
      return;
    case blink::WebAXEventChildrenChanged:
      // TODO(dtseng): no clear equivalent on Mac.
      return;
    case blink::WebAXEventFocus:
      event_id = NSAccessibilityFocusedUIElementChangedNotification;
      break;
    case blink::WebAXEventLayoutComplete:
      event_id = @"AXLayoutComplete";
      break;
    case blink::WebAXEventLiveRegionChanged:
      event_id = @"AXLiveRegionChanged";
      break;
    case blink::WebAXEventLoadComplete:
      event_id = @"AXLoadComplete";
      break;
    case blink::WebAXEventMenuListValueChanged:
      // Not used on Mac.
      return;
    case blink::WebAXEventShow:
      // Not used on Mac.
      return;
    case blink::WebAXEventHide:
      // Not used on Mac.
      return;
    case blink::WebAXEventRowCountChanged:
      event_id = NSAccessibilityRowCountChangedNotification;
      break;
    case blink::WebAXEventRowCollapsed:
      event_id = @"AXRowCollapsed";
      break;
    case blink::WebAXEventRowExpanded:
      event_id = @"AXRowExpanded";
      break;
    case blink::WebAXEventScrolledToAnchor:
      // Not used on Mac.
      return;
    case blink::WebAXEventSelectedChildrenChanged:
      event_id = NSAccessibilitySelectedChildrenChangedNotification;
      break;
    case blink::WebAXEventSelectedTextChanged:
      event_id = NSAccessibilitySelectedTextChangedNotification;
      break;
    case blink::WebAXEventTextInserted:
      // Not used on Mac.
      return;
    case blink::WebAXEventTextRemoved:
      // Not used on Mac.
      return;
    case blink::WebAXEventValueChanged:
      event_id = NSAccessibilityValueChangedNotification;
      break;
    case blink::WebAXEventAriaAttributeChanged:
      // Not used on Mac.
      return;
    case blink::WebAXEventAutocorrectionOccured:
      // Not used on Mac.
      return;
    case blink::WebAXEventInvalidStatusChanged:
      // Not used on Mac.
      return;
    case blink::WebAXEventLocationChanged:
      // Not used on Mac.
      return;
    case blink::WebAXEventMenuListItemSelected:
      // Not used on Mac.
      return;
    case blink::WebAXEventTextChanged:
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
