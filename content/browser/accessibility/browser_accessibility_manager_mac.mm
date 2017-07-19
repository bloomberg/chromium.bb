// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/accessibility/browser_accessibility_manager_mac.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#import "base/mac/mac_util.h"
#import "base/mac/scoped_nsobject.h"
#import "base/mac/sdk_forward_declarations.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#import "content/browser/accessibility/browser_accessibility_cocoa.h"
#import "content/browser/accessibility/browser_accessibility_mac.h"
#include "content/common/accessibility_messages.h"
#include "content/public/browser/browser_thread.h"
#include "ui/accessibility/ax_role_properties.h"

namespace {

// Use same value as in Safari's WebKit.
const int kLiveRegionChangeIntervalMS = 20;

// Declare undocumented accessibility constants and enums only present in
// WebKit.

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

enum AXTextEditType {
  AXTextEditTypeUnknown,
  AXTextEditTypeDelete,
  AXTextEditTypeInsert,
  AXTextEditTypeTyping,
  AXTextEditTypeDictation,
  AXTextEditTypeCut,
  AXTextEditTypePaste,
  AXTextEditTypeAttributesChange
};

NSString* const NSAccessibilityAutocorrectionOccurredNotification =
    @"AXAutocorrectionOccurred";
NSString* const NSAccessibilityLayoutCompleteNotification =
    @"AXLayoutComplete";
NSString* const NSAccessibilityLoadCompleteNotification =
    @"AXLoadComplete";
NSString* const NSAccessibilityInvalidStatusChangedNotification =
    @"AXInvalidStatusChanged";
NSString* const NSAccessibilityLiveRegionCreatedNotification =
    @"AXLiveRegionCreated";
NSString* const NSAccessibilityLiveRegionChangedNotification =
    @"AXLiveRegionChanged";
NSString* const NSAccessibilityExpandedChanged =
    @"AXExpandedChanged";
NSString* const NSAccessibilityMenuItemSelectedNotification =
    @"AXMenuItemSelected";

// Attributes used for NSAccessibilitySelectedTextChangedNotification and
// NSAccessibilityValueChangedNotification.
NSString* const NSAccessibilityTextStateChangeTypeKey =
    @"AXTextStateChangeType";
NSString* const NSAccessibilityTextStateSyncKey = @"AXTextStateSync";
NSString* const NSAccessibilityTextSelectionDirection =
    @"AXTextSelectionDirection";
NSString* const NSAccessibilityTextSelectionGranularity =
    @"AXTextSelectionGranularity";
NSString* const NSAccessibilityTextSelectionChangedFocus =
    @"AXTextSelectionChangedFocus";
NSString* const NSAccessibilitySelectedTextMarkerRangeAttribute =
    @"AXSelectedTextMarkerRange";
NSString* const NSAccessibilityTextChangeElement = @"AXTextChangeElement";
NSString* const NSAccessibilityTextEditType = @"AXTextEditType";
NSString* const NSAccessibilityTextChangeValue = @"AXTextChangeValue";
NSString* const NSAccessibilityTextChangeValueLength =
    @"AXTextChangeValueLength";
NSString* const NSAccessibilityTextChangeValues = @"AXTextChangeValues";

}  // namespace

namespace content {

// static
BrowserAccessibilityManager* BrowserAccessibilityManager::Create(
    const ui::AXTreeUpdate& initial_tree,
    BrowserAccessibilityDelegate* delegate,
    BrowserAccessibilityFactory* factory) {
  return new BrowserAccessibilityManagerMac(initial_tree, delegate, factory);
}

BrowserAccessibilityManagerMac*
BrowserAccessibilityManager::ToBrowserAccessibilityManagerMac() {
  return static_cast<BrowserAccessibilityManagerMac*>(this);
}

BrowserAccessibilityManagerMac::BrowserAccessibilityManagerMac(
    const ui::AXTreeUpdate& initial_tree,
    BrowserAccessibilityDelegate* delegate,
    BrowserAccessibilityFactory* factory)
    : BrowserAccessibilityManager(delegate, factory) {
  Initialize(initial_tree);
}

BrowserAccessibilityManagerMac::~BrowserAccessibilityManagerMac() {}

// static
ui::AXTreeUpdate
    BrowserAccessibilityManagerMac::GetEmptyDocument() {
  ui::AXNodeData empty_document;
  empty_document.id = 0;
  empty_document.role = ui::AX_ROLE_ROOT_WEB_AREA;
  ui::AXTreeUpdate update;
  update.root_id = empty_document.id;
  update.nodes.push_back(empty_document);
  return update;
}

BrowserAccessibility* BrowserAccessibilityManagerMac::GetFocus() {
  BrowserAccessibility* focus = BrowserAccessibilityManager::GetFocus();

  // On Mac, list boxes should always get focus on the whole list, otherwise
  // information about the number of selected items will never be reported.
  // For editable combo boxes, focus should stay on the combo box so the user
  // will not be taken out of the combo box while typing.
  if (focus && (focus->GetRole() == ui::AX_ROLE_LIST_BOX ||
                (focus->GetRole() == ui::AX_ROLE_COMBO_BOX &&
                 focus->HasState(ui::AX_STATE_EDITABLE)))) {
    return focus;
  }

  // For other roles, follow the active descendant.
  return GetActiveDescendant(focus);
}

void BrowserAccessibilityManagerMac::NotifyAccessibilityEvent(
    BrowserAccessibilityEvent::Source source,
    ui::AXEvent event_type,
    BrowserAccessibility* node) {
  if (!node->IsNative())
    return;

  auto native_node = ToBrowserAccessibilityCocoa(node);
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
        // In all other cases we should post
        // |NSAccessibilityFocusedUIElementChangedNotification|, but this is
        // handled elsewhere.
        return;
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
      // This notification should only be fired on the top document.
      // Iframes should use |AX_EVENT_LAYOUT_COMPLETE| to signify that they have
      // finished loading.
      if (IsRootTree()) {
        mac_notification = NSAccessibilityLoadCompleteNotification;
      } else {
        mac_notification = NSAccessibilityLayoutCompleteNotification;
      }
      break;
    case ui::AX_EVENT_INVALID_STATUS_CHANGED:
      mac_notification = NSAccessibilityInvalidStatusChangedNotification;
      break;
    case ui::AX_EVENT_SELECTED_CHILDREN_CHANGED:
      if (ui::IsTableLikeRole(node->GetRole())) {
        mac_notification = NSAccessibilitySelectedRowsChangedNotification;
      } else {
        mac_notification = NSAccessibilitySelectedChildrenChangedNotification;
      }
      break;
    case ui::AX_EVENT_DOCUMENT_SELECTION_CHANGED: {
      mac_notification = NSAccessibilitySelectedTextChangedNotification;
      // WebKit fires a notification both on the focused object and the root.
      BrowserAccessibility* focus = GetFocus();
      if (!focus)
        break;  // Just fire a notification on the root.

      if (base::mac::IsAtLeastOS10_11()) {
        // |NSAccessibilityPostNotificationWithUserInfo| should be used on OS X
        // 10.11 or later to notify Voiceover about text selection changes. This
        // API has been present on versions of OS X since 10.7 but doesn't
        // appear to be needed by Voiceover before version 10.11.
        NSDictionary* user_info =
            GetUserInfoForSelectedTextChangedNotification();

        BrowserAccessibility* root = GetRoot();
        if (!root)
          return;

        NSAccessibilityPostNotificationWithUserInfo(
            ToBrowserAccessibilityCocoa(focus), mac_notification, user_info);
        NSAccessibilityPostNotificationWithUserInfo(
            ToBrowserAccessibilityCocoa(root), mac_notification, user_info);
        return;
      } else {
        NSAccessibilityPostNotification(ToBrowserAccessibilityCocoa(focus),
                                        mac_notification);
      }
      break;
    }
    case ui::AX_EVENT_CHECKED_STATE_CHANGED:
      mac_notification = NSAccessibilityValueChangedNotification;
      break;
    case ui::AX_EVENT_VALUE_CHANGED:
      mac_notification = NSAccessibilityValueChangedNotification;
      if (base::mac::IsAtLeastOS10_11() && text_edits_.size()) {
        // It seems that we don't need to distinguish between deleted and
        // inserted text for now.
        base::string16 deleted_text;
        base::string16 inserted_text;
        int32_t id = node->GetId();
        const auto iterator = text_edits_.find(id);
        if (iterator != text_edits_.end())
          inserted_text = iterator->second;
        NSDictionary* user_info = GetUserInfoForValueChangedNotification(
            native_node, deleted_text, inserted_text);

        BrowserAccessibility* root = GetRoot();
        if (!root)
          return;

        NSAccessibilityPostNotificationWithUserInfo(
            native_node, mac_notification, user_info);
        NSAccessibilityPostNotificationWithUserInfo(
            ToBrowserAccessibilityCocoa(root), mac_notification, user_info);
        return;
      }
      break;
    case ui::AX_EVENT_LIVE_REGION_CREATED:
      mac_notification = NSAccessibilityLiveRegionCreatedNotification;
      break;
    case ui::AX_EVENT_ALERT:
      NSAccessibilityPostNotification(
          native_node, NSAccessibilityLiveRegionCreatedNotification);
      // Voiceover requires a live region changed notification to actually
      // announce the live region.
      NotifyAccessibilityEvent(BrowserAccessibilityEvent::FromTreeChange,
                               ui::AX_EVENT_LIVE_REGION_CHANGED, node);
      return;
    case ui::AX_EVENT_LIVE_REGION_CHANGED: {
      // Voiceover seems to drop live region changed notifications if they come
      // too soon after a live region created notification.
      // TODO(nektar): Limit the number of changed notifications as well.

      if (never_suppress_or_delay_events_for_testing_) {
        NSAccessibilityPostNotification(
            native_node, NSAccessibilityLiveRegionChangedNotification);
        return;
      }

      base::scoped_nsobject<BrowserAccessibilityCocoa> retained_node(
          [native_node retain]);
      BrowserThread::PostDelayedTask(
          BrowserThread::UI, FROM_HERE,
          base::Bind(
              [](base::scoped_nsobject<BrowserAccessibilityCocoa> node) {
                if (node && [node instanceActive]) {
                  NSAccessibilityPostNotification(
                      node, NSAccessibilityLiveRegionChangedNotification);
                }
              },
              std::move(retained_node)),
          base::TimeDelta::FromMilliseconds(kLiveRegionChangeIntervalMS));
    }
      return;
    case ui::AX_EVENT_ROW_COUNT_CHANGED:
      mac_notification = NSAccessibilityRowCountChangedNotification;
      break;
    case ui::AX_EVENT_ROW_EXPANDED:
      mac_notification = NSAccessibilityRowExpandedNotification;
      break;
    case ui::AX_EVENT_ROW_COLLAPSED:
      mac_notification = NSAccessibilityRowCollapsedNotification;
      break;
    // TODO(nektar): Add events for busy.
    case ui::AX_EVENT_EXPANDED_CHANGED:
      mac_notification = NSAccessibilityExpandedChanged;
      break;
    // TODO(nektar): Support menu open/close notifications.
    case ui::AX_EVENT_MENU_LIST_ITEM_SELECTED:
      mac_notification = NSAccessibilityMenuItemSelectedNotification;
      break;

    // These events are not used on Mac for now.
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

void BrowserAccessibilityManagerMac::OnAccessibilityEvents(
    const std::vector<AXEventNotificationDetails>& details) {
  text_edits_.clear();
  // Call the base method last as it might delete the tree if it receives an
  // invalid message.
  BrowserAccessibilityManager::OnAccessibilityEvents(details);
}

void BrowserAccessibilityManagerMac::OnTreeDataChanged(
    ui::AXTree* tree,
    const ui::AXTreeData& old_tree_data,
    const ui::AXTreeData& new_tree_data) {
  BrowserAccessibilityManager::OnTreeDataChanged(tree, old_tree_data,
                                                 new_tree_data);
  if (new_tree_data.loaded && !old_tree_data.loaded)
    tree_events_[tree->root()->id()].insert(ui::AX_EVENT_LOAD_COMPLETE);
  if (new_tree_data.sel_anchor_object_id !=
          old_tree_data.sel_anchor_object_id ||
      new_tree_data.sel_anchor_offset != old_tree_data.sel_anchor_offset ||
      new_tree_data.sel_anchor_affinity != old_tree_data.sel_anchor_affinity ||
      new_tree_data.sel_focus_object_id != old_tree_data.sel_focus_object_id ||
      new_tree_data.sel_focus_offset != old_tree_data.sel_focus_offset ||
      new_tree_data.sel_focus_affinity != old_tree_data.sel_focus_affinity) {
    tree_events_[tree->root()->id()].insert(
        ui::AX_EVENT_DOCUMENT_SELECTION_CHANGED);
  }
}

void BrowserAccessibilityManagerMac::OnNodeDataWillChange(
    ui::AXTree* tree,
    const ui::AXNodeData& old_node_data,
    const ui::AXNodeData& new_node_data) {
  BrowserAccessibilityManager::OnNodeDataWillChange(tree, old_node_data,
                                                    new_node_data);

  // Starting from OS X 10.11, if the user has edited some text we need to
  // dispatch the actual text that changed on the value changed notification.
  // We run this code on all OS X versions to get the highest test coverage.
  base::string16 old_text, new_text;
  ui::AXRole role = new_node_data.role;
  if (role == ui::AX_ROLE_COMBO_BOX || role == ui::AX_ROLE_SEARCH_BOX ||
      role == ui::AX_ROLE_TEXT_FIELD) {
    old_text = old_node_data.GetString16Attribute(ui::AX_ATTR_VALUE);
    new_text = new_node_data.GetString16Attribute(ui::AX_ATTR_VALUE);
  } else if (new_node_data.HasState(ui::AX_STATE_EDITABLE)) {
    old_text = old_node_data.GetString16Attribute(ui::AX_ATTR_NAME);
    new_text = new_node_data.GetString16Attribute(ui::AX_ATTR_NAME);
  }

  if ((old_text.empty() && new_text.empty()) ||
      old_text.length() == new_text.length()) {
    return;
  }

  if (old_text.length() < new_text.length()) {
    // Insertion.
    size_t i = 0;
    while (i < old_text.length() && i < new_text.length() &&
           old_text[i] == new_text[i]) {
      ++i;
    }
    size_t length = (new_text.length() - i) - (old_text.length() - i);
    base::string16 inserted_text = new_text.substr(i, length);
    text_edits_[new_node_data.id] = inserted_text;
  } else {
    // Deletion.
    size_t i = 0;
    while (i < old_text.length() && i < new_text.length() &&
           old_text[i] == new_text[i]) {
      ++i;
    }
    size_t length = (old_text.length() - i) - (new_text.length() - i);
    base::string16 deleted_text = old_text.substr(i, length);
    text_edits_[new_node_data.id] = deleted_text;
  }
}

void BrowserAccessibilityManagerMac::OnStateChanged(ui::AXTree* tree,
                                                    ui::AXNode* node,
                                                    ui::AXState state,
                                                    bool new_value) {
  BrowserAccessibilityManager::OnStateChanged(tree, node, state, new_value);
  if (state == ui::AX_STATE_EXPANDED) {
    if (node->data().role == ui::AX_ROLE_ROW ||
        node->data().role == ui::AX_ROLE_TREE_ITEM) {
      if (new_value)
        tree_events_[node->id()].insert(ui::AX_EVENT_ROW_EXPANDED);
      else
        tree_events_[node->id()].insert(ui::AX_EVENT_ROW_COLLAPSED);
      ui::AXNode* container = node;
      while (container && !ui::IsRowContainer(container->data().role))
        container = container->parent();
      if (container)
        tree_events_[container->id()].insert(ui::AX_EVENT_ROW_COUNT_CHANGED);
    } else {
      tree_events_[node->id()].insert(ui::AX_EVENT_EXPANDED_CHANGED);
    }
  }
  if (state == ui::AX_STATE_SELECTED) {
    ui::AXNode* container = node;
    while (container &&
           !ui::IsContainerWithSelectableChildrenRole(container->data().role))
      container = container->parent();
    if (container)
      tree_events_[container->id()].insert(
          ui::AX_EVENT_SELECTED_CHILDREN_CHANGED);
  }
}

void BrowserAccessibilityManagerMac::OnStringAttributeChanged(
    ui::AXTree* tree,
    ui::AXNode* node,
    ui::AXStringAttribute attr,
    const std::string& old_value,
    const std::string& new_value) {
  BrowserAccessibilityManager::OnStringAttributeChanged(tree, node, attr,
                                                        old_value, new_value);
  switch (attr) {
    case ui::AX_ATTR_VALUE:
      tree_events_[node->id()].insert(ui::AX_EVENT_VALUE_CHANGED);
      break;
    case ui::AX_ATTR_ARIA_INVALID_VALUE:
      tree_events_[node->id()].insert(ui::AX_EVENT_INVALID_STATUS_CHANGED);
      break;
    case ui::AX_ATTR_LIVE_STATUS:
      tree_events_[node->id()].insert(ui::AX_EVENT_LIVE_REGION_CREATED);
      break;
    default:
      break;
  }
}

void BrowserAccessibilityManagerMac::OnIntAttributeChanged(
    ui::AXTree* tree,
    ui::AXNode* node,
    ui::AXIntAttribute attr,
    int32_t old_value,
    int32_t new_value) {
  BrowserAccessibilityManager::OnIntAttributeChanged(tree, node, attr,
                                                     old_value, new_value);
  switch (attr) {
    case ui::AX_ATTR_CHECKED_STATE:
      tree_events_[node->id()].insert(ui::AX_EVENT_VALUE_CHANGED);
      break;
    case ui::AX_ATTR_ACTIVEDESCENDANT_ID:
      tree_events_[node->id()].insert(ui::AX_EVENT_ACTIVEDESCENDANTCHANGED);
      break;
    case ui::AX_ATTR_INVALID_STATE:
      tree_events_[node->id()].insert(ui::AX_EVENT_INVALID_STATUS_CHANGED);
      break;
    default:
      break;
  }
}

void BrowserAccessibilityManagerMac::OnFloatAttributeChanged(
    ui::AXTree* tree,
    ui::AXNode* node,
    ui::AXFloatAttribute attr,
    float old_value,
    float new_value) {
  BrowserAccessibilityManager::OnFloatAttributeChanged(tree, node, attr,
                                                       old_value, new_value);
  if (attr == ui::AX_ATTR_VALUE_FOR_RANGE)
    tree_events_[node->id()].insert(ui::AX_EVENT_VALUE_CHANGED);
}

void BrowserAccessibilityManagerMac::OnAtomicUpdateFinished(
    ui::AXTree* tree,
    bool root_changed,
    const std::vector<Change>& changes) {
  BrowserAccessibilityManager::OnAtomicUpdateFinished(tree, root_changed,
                                                      changes);
  for (const auto& change : changes) {
    if ((change.type == NODE_CREATED || change.type == SUBTREE_CREATED) &&
        change.node->data().HasStringAttribute(ui::AX_ATTR_LIVE_STATUS)) {
      if (change.node->data().role == ui::AX_ROLE_ALERT)
        tree_events_[change.node->id()].insert(ui::AX_EVENT_ALERT);
      else
        tree_events_[change.node->id()].insert(
            ui::AX_EVENT_LIVE_REGION_CREATED);
      continue;
    }
    if (change.node->data().HasStringAttribute(
            ui::AX_ATTR_CONTAINER_LIVE_STATUS)) {
      ui::AXNode* live_root = change.node;
      while (live_root &&
             !live_root->data().HasStringAttribute(ui::AX_ATTR_LIVE_STATUS))
        live_root = live_root->parent();
      if (live_root)
        tree_events_[live_root->id()].insert(ui::AX_EVENT_LIVE_REGION_CHANGED);
    }
  }

  if (root_changed && tree->data().loaded)
    tree_events_[tree->root()->id()].insert(ui::AX_EVENT_LOAD_COMPLETE);
}

NSDictionary* BrowserAccessibilityManagerMac::
    GetUserInfoForSelectedTextChangedNotification() {
  NSMutableDictionary* user_info = [[[NSMutableDictionary alloc] init]
      autorelease];
  [user_info setObject:@YES forKey:NSAccessibilityTextStateSyncKey];
  [user_info setObject:@(AXTextStateChangeTypeUnknown)
                forKey:NSAccessibilityTextStateChangeTypeKey];
  [user_info setObject:@(AXTextSelectionDirectionUnknown)
                forKey:NSAccessibilityTextSelectionDirection];
  [user_info setObject:@(AXTextSelectionGranularityUnknown)
                forKey:NSAccessibilityTextSelectionGranularity];
  [user_info setObject:@YES forKey:NSAccessibilityTextSelectionChangedFocus];

  int32_t focus_id = GetTreeData().sel_focus_object_id;
  BrowserAccessibility* focus_object = GetFromID(focus_id);
  if (focus_object) {
    focus_object = focus_object->GetClosestPlatformObject();
    auto native_focus_object = ToBrowserAccessibilityCocoa(focus_object);
    if (native_focus_object && [native_focus_object instanceActive]) {
      [user_info setObject:native_focus_object
                    forKey:NSAccessibilityTextChangeElement];

      id selected_text = [native_focus_object selectedTextMarkerRange];
      if (selected_text) {
        [user_info setObject:selected_text
                      forKey:NSAccessibilitySelectedTextMarkerRangeAttribute];
      }
    }
  }

  return user_info;
}

NSDictionary*
BrowserAccessibilityManagerMac::GetUserInfoForValueChangedNotification(
    const BrowserAccessibilityCocoa* native_node,
    const base::string16& deleted_text,
    const base::string16& inserted_text) const {
  DCHECK(native_node);
  if (deleted_text.empty() && inserted_text.empty())
    return nil;

  NSMutableArray* changes = [[[NSMutableArray alloc] init] autorelease];
  if (!deleted_text.empty()) {
    [changes addObject:@{
      NSAccessibilityTextEditType : @(AXTextEditTypeUnknown),
      NSAccessibilityTextChangeValueLength : @(deleted_text.length()),
      NSAccessibilityTextChangeValue : base::SysUTF16ToNSString(deleted_text)
    }];
  }
  if (!inserted_text.empty()) {
    [changes addObject:@{
      NSAccessibilityTextEditType : @(AXTextEditTypeUnknown),
      NSAccessibilityTextChangeValueLength : @(inserted_text.length()),
      NSAccessibilityTextChangeValue : base::SysUTF16ToNSString(inserted_text)
    }];
  }

  return @{
    NSAccessibilityTextStateChangeTypeKey : @(AXTextStateChangeTypeEdit),
    NSAccessibilityTextChangeValues : changes,
    NSAccessibilityTextChangeElement : native_node
  };
}

NSView* BrowserAccessibilityManagerMac::GetParentView() {
  return delegate() ? delegate()->AccessibilityGetAcceleratedWidget() : nullptr;
}

}  // namespace content
