// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/accessibility/browser_accessibility_manager_mac.h"

#include <stddef.h>

#import "base/mac/mac_util.h"
#import "base/mac/sdk_forward_declarations.h"
#include "base/logging.h"
#import "content/browser/accessibility/browser_accessibility_cocoa.h"
#import "content/browser/accessibility/browser_accessibility_mac.h"
#include "content/common/accessibility_messages.h"


namespace {

// Declare accessibility constants and enums only present in WebKit.

enum AXTextStateChangeType {
  AXTextStateChangeTypeUnknown,
  AXTextStateChangeTypeEdit,
  AXTextStateChangeTypeSelectionMove,
  AXTextStateChangeTypeSelectionExtend
};

enum AXTextSelectionDirection {
  AXTextSelectionDirectionUnknown,
  AXTextSelectionDirectionBeginning,
  AXTextSelectionDirectionEnd,
  AXTextSelectionDirectionPrevious,
  AXTextSelectionDirectionNext,
  AXTextSelectionDirectionDiscontiguous
};

enum AXTextSelectionGranularity {
  AXTextSelectionGranularityUnknown,
  AXTextSelectionGranularityCharacter,
  AXTextSelectionGranularityWord,
  AXTextSelectionGranularityLine,
  AXTextSelectionGranularitySentence,
  AXTextSelectionGranularityParagraph,
  AXTextSelectionGranularityPage,
  AXTextSelectionGranularityDocument,
  AXTextSelectionGranularityAll
};

NSString* const NSAccessibilityAutocorrectionOccurredNotification =
    @"AXAutocorrectionOccurred";
NSString* const NSAccessibilityLayoutCompleteNotification =
    @"AXLayoutComplete";
NSString* const NSAccessibilityLoadCompleteNotification =
    @"AXLoadComplete";
NSString* const NSAccessibilityInvalidStatusChangedNotification =
    @"AXInvalidStatusChanged";
NSString* const NSAccessibilityLiveRegionChangedNotification =
    @"AXLiveRegionChanged";
NSString* const NSAccessibilityMenuItemSelectedNotification =
    @"AXMenuItemSelected";
NSString* const NSAccessibilityTextStateChangeTypeKey =
    @"AXTextStateChangeType";
NSString* const NSAccessibilityTextStateSyncKey = @"AXTextStateSync";
NSString* const NSAccessibilityTextSelectionDirection =
    @"AXTextSelectionDirection";
NSString* const NSAccessibilityTextSelectionGranularity =
    @"AXTextSelectionGranularity";
NSString* const NSAccessibilityTextSelectionChangedFocus =
    @"AXTextSelectionChangedFocus";
NSString* const NSAccessibilityTextChangeElement =
    @"AXAccessibilityTextChangeElement";

}  // namespace

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

  if (event_type == ui::AX_EVENT_FOCUS) {
    BrowserAccessibility* active_descendant = GetActiveDescendantFocus(
        GetRoot());
    if (active_descendant)
      node = active_descendant;

    if (node->GetRole() == ui::AX_ROLE_LIST_BOX_OPTION &&
        node->HasState(ui::AX_STATE_SELECTED) &&
        node->GetParent() &&
        node->GetParent()->GetRole() == ui::AX_ROLE_LIST_BOX) {
      node = node->GetParent();
      SetFocus(node, false);
    }
  }

  auto native_node = node->ToBrowserAccessibilityCocoa();
  DCHECK(native_node);

  // Refer to |AXObjectCache::postPlatformNotification| in WebKit source code.
  NSString* mac_notification;
  switch (event_type) {
    case ui::AX_EVENT_ACTIVEDESCENDANTCHANGED:
      if (node->GetRole() == ui::AX_ROLE_TREE) {
        mac_notification = NSAccessibilitySelectedRowsChangedNotification;
      } else if (node->GetRole() == ui::AX_ROLE_COMBO_BOX) {
        // Even though the selected item in the combo box has changed, we don't
        // want to post a focus change because this will take the focus out of
        // the combo box where the user might be typing.
        mac_notification = NSAccessibilitySelectedChildrenChangedNotification;
      } else {
        mac_notification = NSAccessibilityFocusedUIElementChangedNotification;
      }
      break;
    case ui::AX_EVENT_AUTOCORRECTION_OCCURED:
      mac_notification = NSAccessibilityAutocorrectionOccurredNotification;
      break;
    case ui::AX_EVENT_FOCUS:
      mac_notification = NSAccessibilityFocusedUIElementChangedNotification;
      break;
    case ui::AX_EVENT_LAYOUT_COMPLETE:
      mac_notification = NSAccessibilityLayoutCompleteNotification;
      break;
    case ui::AX_EVENT_LOAD_COMPLETE:
      mac_notification = NSAccessibilityLoadCompleteNotification;
      break;
    case ui::AX_EVENT_INVALID_STATUS_CHANGED:
      mac_notification = NSAccessibilityInvalidStatusChangedNotification;
      break;
    case ui::AX_EVENT_SELECTED_CHILDREN_CHANGED:
      if (node->GetRole() == ui::AX_ROLE_GRID ||
          node->GetRole() == ui::AX_ROLE_TABLE) {
        mac_notification = NSAccessibilitySelectedRowsChangedNotification;
      } else {
        mac_notification = NSAccessibilitySelectedChildrenChangedNotification;
      }
      break;
    case ui::AX_EVENT_DOCUMENT_SELECTION_CHANGED: {
      mac_notification = NSAccessibilitySelectedTextChangedNotification;
      if (base::mac::IsOSElCapitanOrLater()) {
        // |NSAccessibilityPostNotificationWithUserInfo| should be used on OS X
        // 10.11 or later to notify Voiceover about text selection changes. This
        // API has been present on versions of OS X since 10.7 but doesn't
        // appear to be needed by Voiceover before version 10.11.
        NSDictionary* user_info =
            GetUserInfoForSelectedTextChangedNotification();
        NSAccessibilityPostNotificationWithUserInfo(
            native_node, mac_notification, user_info);
        return;
      }
      break;
    }
    case ui::AX_EVENT_CHECKED_STATE_CHANGED:
    case ui::AX_EVENT_VALUE_CHANGED:
      mac_notification = NSAccessibilityValueChangedNotification;
      break;
    // TODO(nektar): Need to add an event for live region created.
    case ui::AX_EVENT_LIVE_REGION_CHANGED:
      mac_notification = NSAccessibilityLiveRegionChangedNotification;
      break;
    case ui::AX_EVENT_ROW_COUNT_CHANGED:
      mac_notification = NSAccessibilityRowCountChangedNotification;
      break;
    case ui::AX_EVENT_ROW_EXPANDED:
      mac_notification = NSAccessibilityRowExpandedNotification;
      break;
    case ui::AX_EVENT_ROW_COLLAPSED:
      mac_notification = NSAccessibilityRowCollapsedNotification;
      break;
    // TODO(nektar): Add events for busy and expanded changed.
    // TODO(nektar): Support menu open/close notifications.
    case ui::AX_EVENT_MENU_LIST_ITEM_SELECTED:
      mac_notification = NSAccessibilityMenuItemSelectedNotification;
      break;

    // These events are not used on Mac for now.
    case ui::AX_EVENT_ALERT:
    case ui::AX_EVENT_TEXT_CHANGED:
    case ui::AX_EVENT_CHILDREN_CHANGED:
    case ui::AX_EVENT_MENU_LIST_VALUE_CHANGED:
    case ui::AX_EVENT_SCROLL_POSITION_CHANGED:
    case ui::AX_EVENT_SCROLLED_TO_ANCHOR:
    case ui::AX_EVENT_ARIA_ATTRIBUTE_CHANGED:
    case ui::AX_EVENT_LOCATION_CHANGED:
      return;

    // Deprecated events.
    case ui::AX_EVENT_BLUR:
    case ui::AX_EVENT_HIDE:
    case ui::AX_EVENT_HOVER:
    case ui::AX_EVENT_TEXT_SELECTION_CHANGED:
    case ui::AX_EVENT_SHOW:
      return;
    default:
      DLOG(WARNING) << "Unknown accessibility event: " << event_type;
      return;
  }

  NSAccessibilityPostNotification(native_node, mac_notification);
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

// Returns an autoreleased object.
NSDictionary* BrowserAccessibilityManagerMac::
    GetUserInfoForSelectedTextChangedNotification() {
  NSMutableDictionary* user_info = [[[NSMutableDictionary alloc] init]
      autorelease];
  [user_info setObject:[NSNumber numberWithBool:YES]
                forKey:NSAccessibilityTextStateSyncKey];
  [user_info setObject:[NSNumber numberWithInt:AXTextStateChangeTypeUnknown]
                forKey:NSAccessibilityTextStateChangeTypeKey];
  [user_info
      setObject:[NSNumber numberWithInt:AXTextSelectionDirectionUnknown]
         forKey:NSAccessibilityTextSelectionDirection];
  [user_info
      setObject:[NSNumber numberWithInt:AXTextSelectionGranularityUnknown]
         forKey:NSAccessibilityTextSelectionGranularity];
  [user_info setObject:[NSNumber numberWithBool:YES]
                forKey:NSAccessibilityTextSelectionChangedFocus];
  // TODO(nektar): Set selected text marker range.

  int32_t focus_id = GetTreeData().sel_focus_object_id;
  BrowserAccessibility* focus_object = GetFromID(focus_id);
  if (focus_object) {
    auto native_focus_object = focus_object->ToBrowserAccessibilityCocoa();
    if (native_focus_object)
      [user_info setObject:native_focus_object
                    forKey:NSAccessibilityTextChangeElement];
  }

  return user_info;
}

}  // namespace content
