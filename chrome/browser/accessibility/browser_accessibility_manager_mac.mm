// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/accessibility/browser_accessibility_manager_mac.h"

#include "chrome/browser/accessibility/browser_accessibility_mac.h"
#import "chrome/browser/cocoa/browser_accessibility.h"
// TODO(dtseng): move to delegate?
#import "chrome/browser/renderer_host/render_widget_host_view_mac.h"

// static
BrowserAccessibilityManager* BrowserAccessibilityManager::Create(
    gfx::NativeView parent_view,
    const WebAccessibility& src,
    BrowserAccessibilityDelegate* delegate,
    BrowserAccessibilityFactory* factory) {
  return new BrowserAccessibilityManagerMac(
      parent_view, src, delegate, factory);
}

BrowserAccessibility* BrowserAccessibilityFactory::Create() {
  return BrowserAccessibility::Create();
}

BrowserAccessibilityManagerMac::BrowserAccessibilityManagerMac(
    gfx::NativeView parent_window,
    const webkit_glue::WebAccessibility& src,
    BrowserAccessibilityDelegate* delegate,
    BrowserAccessibilityFactory* factory) 
        : BrowserAccessibilityManager(parent_window, src, delegate, factory) {
}

void BrowserAccessibilityManagerMac::NotifyAccessibilityEvent(
    ViewHostMsg_AccessibilityNotification_Params::NotificationType n,
    BrowserAccessibility* node) {
  // TODO(dtseng): support all notifications.
  NSString* event_id = @"";
  switch (n) {
    case ViewHostMsg_AccessibilityNotification_Params::
        NOTIFICATION_TYPE_CHECK_STATE_CHANGED:
      return;
    case ViewHostMsg_AccessibilityNotification_Params::
        NOTIFICATION_TYPE_CHILDREN_CHANGED:
      event_id = NSAccessibilityValueChangedNotification;
      if (GetRoot() == node)
        [((RenderWidgetHostViewCocoa*)GetParentView())
            setAccessibilityTreeRoot:GetRoot()];
      else
        [node->GetParent()->toBrowserAccessibilityMac()->native_view()
            updateDescendants];
      break;
    case ViewHostMsg_AccessibilityNotification_Params::
        NOTIFICATION_TYPE_FOCUS_CHANGED:
      event_id = NSAccessibilityFocusedUIElementChangedNotification;
      if (GetRoot() == node)
        [((RenderWidgetHostViewCocoa*)GetParentView())
            setAccessibilityTreeRoot:GetRoot()];
      else
        [node->GetParent()->toBrowserAccessibilityMac()->native_view()
            updateDescendants];
      break;
    case ViewHostMsg_AccessibilityNotification_Params::
        NOTIFICATION_TYPE_LOAD_COMPLETE:
      [((RenderWidgetHostViewCocoa*)GetParentView())
          setAccessibilityTreeRoot:GetRoot()];
      return;
    case ViewHostMsg_AccessibilityNotification_Params::
        NOTIFICATION_TYPE_VALUE_CHANGED:
      event_id = NSAccessibilityValueChangedNotification;
      break;
    case ViewHostMsg_AccessibilityNotification_Params::
        NOTIFICATION_TYPE_SELECTED_TEXT_CHANGED:
      return;
  }
  BrowserAccessibilityCocoa* native_node = node->toBrowserAccessibilityMac()->
      native_view();
  DCHECK(native_node);
  NSAccessibilityPostNotification(native_node, event_id); 
}
