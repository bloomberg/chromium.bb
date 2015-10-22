// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/accessibility/browser_accessibility_manager_mac.h"

#import "base/logging.h"
#import "content/browser/accessibility/browser_accessibility_cocoa.h"
#import "content/browser/accessibility/browser_accessibility_mac.h"
#include "content/common/accessibility_messages.h"

namespace content {

// static
BrowserAccessibilityManager* BrowserAccessibilityManager::Create(
    const ui::AXTreeUpdate& initial_tree,
    BrowserAccessibilityDelegate* delegate,
    BrowserAccessibilityFactory* factory) {
  return new BrowserAccessibilityManagerMac(
      NULL, initial_tree, delegate, factory);
}

BrowserAccessibilityManagerMac::BrowserAccessibilityManagerMac(
    NSView* parent_view,
    const ui::AXTreeUpdate& initial_tree,
    BrowserAccessibilityDelegate* delegate,
    BrowserAccessibilityFactory* factory)
    : BrowserAccessibilityManager(delegate, factory),
      parent_view_(parent_view) {
  Initialize(initial_tree);
}

// static
ui::AXTreeUpdate
    BrowserAccessibilityManagerMac::GetEmptyDocument() {
  ui::AXNodeData empty_document;
  empty_document.id = 0;
  empty_document.role = ui::AX_ROLE_ROOT_WEB_AREA;
  empty_document.state =
      1 << ui::AX_STATE_READ_ONLY;
  ui::AXTreeUpdate update;
  update.nodes.push_back(empty_document);
  return update;
}

BrowserAccessibility* BrowserAccessibilityManagerMac::GetFocus(
    BrowserAccessibility* root) {
  // On Mac, list boxes should always get focus on the whole list, otherwise
  // information about the number of selected items will never be reported.
  BrowserAccessibility* node = BrowserAccessibilityManager::GetFocus(root);
  if (node && node->GetRole() == ui::AX_ROLE_LIST_BOX)
    return node;

  // For other roles, follow the active descendant.
  return GetActiveDescendantFocus(root);
}

void BrowserAccessibilityManagerMac::NotifyAccessibilityEvent(
    ui::AXEvent event_type,
    BrowserAccessibility* node) {
  if (!node->IsNative())
    return;

  if (event_type == ui::AX_EVENT_FOCUS &&
      node->GetRole() == ui::AX_ROLE_LIST_BOX_OPTION &&
      node->HasState(ui::AX_STATE_SELECTED) &&
      node->GetParent() &&
      node->GetParent()->GetRole() == ui::AX_ROLE_LIST_BOX) {
    node = node->GetParent();
    SetFocus(node, false);
  }

  // Refer to AXObjectCache.mm (webkit).
  NSString* event_id = @"";
  switch (event_type) {
    case ui::AX_EVENT_ACTIVEDESCENDANTCHANGED:
      if (node->GetRole() == ui::AX_ROLE_TREE) {
        event_id = NSAccessibilitySelectedRowsChangedNotification;
      } else {
        event_id = NSAccessibilityFocusedUIElementChangedNotification;
        BrowserAccessibility* active_descendant_focus =
            GetActiveDescendantFocus(GetRoot());
        if (active_descendant_focus)
          node = active_descendant_focus;
      }

      break;
    case ui::AX_EVENT_ALERT:
      // Not used on Mac.
      return;
    case ui::AX_EVENT_BLUR:
      // A no-op on Mac.
      return;
    case ui::AX_EVENT_CHECKED_STATE_CHANGED:
      // Not used on Mac.
      return;
    case ui::AX_EVENT_CHILDREN_CHANGED:
      // TODO(dtseng): no clear equivalent on Mac.
      return;
    case ui::AX_EVENT_DOCUMENT_SELECTION_CHANGED:
      // Not used on Mac.
      return;
    case ui::AX_EVENT_FOCUS:
      event_id = NSAccessibilityFocusedUIElementChangedNotification;
      break;
    case ui::AX_EVENT_LAYOUT_COMPLETE:
      event_id = @"AXLayoutComplete";
      break;
    case ui::AX_EVENT_LIVE_REGION_CHANGED:
      event_id = @"AXLiveRegionChanged";
      break;
    case ui::AX_EVENT_LOAD_COMPLETE:
      event_id = @"AXLoadComplete";
      break;
    case ui::AX_EVENT_MENU_LIST_VALUE_CHANGED:
      // Not used on Mac.
      return;
    case ui::AX_EVENT_ROW_COUNT_CHANGED:
      event_id = NSAccessibilityRowCountChangedNotification;
      break;
    case ui::AX_EVENT_ROW_COLLAPSED:
      event_id = @"AXRowCollapsed";
      break;
    case ui::AX_EVENT_ROW_EXPANDED:
      event_id = @"AXRowExpanded";
      break;
    case ui::AX_EVENT_SCROLLED_TO_ANCHOR:
      // Not used on Mac.
      return;
    case ui::AX_EVENT_SELECTED_CHILDREN_CHANGED:
      event_id = NSAccessibilitySelectedChildrenChangedNotification;
      break;
    case ui::AX_EVENT_TEXT_SELECTION_CHANGED:
      event_id = NSAccessibilitySelectedTextChangedNotification;
      break;
    case ui::AX_EVENT_VALUE_CHANGED:
      event_id = NSAccessibilityValueChangedNotification;
      break;
    case ui::AX_EVENT_ARIA_ATTRIBUTE_CHANGED:
      // Not used on Mac.
      return;
    case ui::AX_EVENT_AUTOCORRECTION_OCCURED:
      // Not used on Mac.
      return;
    case ui::AX_EVENT_INVALID_STATUS_CHANGED:
      // Not used on Mac.
      return;
    case ui::AX_EVENT_LOCATION_CHANGED:
      // Not used on Mac.
      return;
    case ui::AX_EVENT_MENU_LIST_ITEM_SELECTED:
      // Not used on Mac.
      return;
    case ui::AX_EVENT_TEXT_CHANGED:
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

void BrowserAccessibilityManagerMac::OnAtomicUpdateFinished(
    ui::AXTree* tree,
    bool root_changed,
    const std::vector<ui::AXTreeDelegate::Change>& changes) {
  BrowserAccessibilityManager::OnAtomicUpdateFinished(
      tree, root_changed, changes);

  bool created_live_region = false;
  for (size_t i = 0; i < changes.size(); ++i) {
    if (changes[i].type != NODE_CREATED && changes[i].type != SUBTREE_CREATED)
      continue;

    const ui::AXNode* changed_node = changes[i].node;
    DCHECK(changed_node);
    BrowserAccessibility* obj = GetFromAXNode(changed_node);
    if (obj && obj->HasStringAttribute(ui::AX_ATTR_LIVE_STATUS)) {
      created_live_region = true;
      break;
    }
  }

  if (!created_live_region)
    return;

  // This code is to work around a bug in VoiceOver, where a new live
  // region that gets added is ignored. VoiceOver seems to only scan the
  // page for live regions once. By recreating the NSAccessibility
  // object for the root of the tree, we force VoiceOver to clear out its
  // internal state and find newly-added live regions this time.
  BrowserAccessibilityMac* root =
      static_cast<BrowserAccessibilityMac*>(GetRoot());
  if (root) {
    root->RecreateNativeObject();
    NotifyAccessibilityEvent(ui::AX_EVENT_CHILDREN_CHANGED, root);
  }
}

}  // namespace content
