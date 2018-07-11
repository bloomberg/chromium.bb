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
#include "ui/accelerated_widget_mac/accelerated_widget_mac.h"
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
  tree_->SetEnableExtraMacNodes(true);
}

BrowserAccessibilityManagerMac::~BrowserAccessibilityManagerMac() {}

// static
ui::AXTreeUpdate
    BrowserAccessibilityManagerMac::GetEmptyDocument() {
  ui::AXNodeData empty_document;
  empty_document.id = 0;
  empty_document.role = ax::mojom::Role::kRootWebArea;
  ui::AXTreeUpdate update;
  update.root_id = empty_document.id;
  update.nodes.push_back(empty_document);
  return update;
}

BrowserAccessibility* BrowserAccessibilityManagerMac::GetFocus() {
  BrowserAccessibility* focus = BrowserAccessibilityManager::GetFocus();

  // For editable combo boxes, focus should stay on the combo box so the user
  // will not be taken out of the combo box while typing.
  if (focus && focus->GetRole() == ax::mojom::Role::kTextFieldWithComboBox)
    return focus;

  // For other roles, follow the active descendant.
  return GetActiveDescendant(focus);
}

void BrowserAccessibilityManagerMac::FireFocusEvent(
    BrowserAccessibility* node) {
  BrowserAccessibilityManager::FireFocusEvent(node);
  FireNativeMacNotification(NSAccessibilityFocusedUIElementChangedNotification,
                            node);
}

void BrowserAccessibilityManagerMac::FireBlinkEvent(
    ax::mojom::Event event_type,
    BrowserAccessibility* node) {
  BrowserAccessibilityManager::FireBlinkEvent(event_type, node);
  NSString* mac_notification = nullptr;
  switch (event_type) {
    case ax::mojom::Event::kAutocorrectionOccured:
      mac_notification = NSAccessibilityAutocorrectionOccurredNotification;
      break;
    case ax::mojom::Event::kLayoutComplete:
      mac_notification = NSAccessibilityLayoutCompleteNotification;
      break;
    default:
      return;
  }

  FireNativeMacNotification(mac_notification, node);
}

void BrowserAccessibilityManagerMac::FireGeneratedEvent(
    AXEventGenerator::Event event_type,
    BrowserAccessibility* node) {
  BrowserAccessibilityManager::FireGeneratedEvent(event_type, node);
  if (!node->IsNative())
    return;

  auto native_node = ToBrowserAccessibilityCocoa(node);
  DCHECK(native_node);

  // Refer to |AXObjectCache::postPlatformNotification| in WebKit source code.
  NSString* mac_notification = nullptr;
  switch (event_type) {
    case Event::ACTIVE_DESCENDANT_CHANGED:
      if (node->GetRole() == ax::mojom::Role::kTree) {
        mac_notification = NSAccessibilitySelectedRowsChangedNotification;
      } else if (node->GetRole() == ax::mojom::Role::kTextFieldWithComboBox) {
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
    case Event::LOAD_COMPLETE:
      // This notification should only be fired on the top document.
      // Iframes should use |ax::mojom::Event::kLayoutComplete| to signify that
      // they have finished loading.
      if (IsRootTree()) {
        mac_notification = NSAccessibilityLoadCompleteNotification;
      } else {
        mac_notification = NSAccessibilityLayoutCompleteNotification;
      }
      break;
    case Event::INVALID_STATUS_CHANGED:
      mac_notification = NSAccessibilityInvalidStatusChangedNotification;
      break;
    case Event::SELECTED_CHILDREN_CHANGED:
      if (ui::IsTableLikeRole(node->GetRole())) {
        mac_notification = NSAccessibilitySelectedRowsChangedNotification;
      } else {
        mac_notification = NSAccessibilitySelectedChildrenChangedNotification;
      }
      break;
    case Event::DOCUMENT_SELECTION_CHANGED: {
      mac_notification = NSAccessibilitySelectedTextChangedNotification;
      // WebKit fires a notification both on the focused object and the page
      // root.
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

        BrowserAccessibilityManager* root_manager = GetRootManager();
        if (!root_manager)
          return;
        BrowserAccessibility* root = root_manager->GetRoot();
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
    case Event::CHECKED_STATE_CHANGED:
      mac_notification = NSAccessibilityValueChangedNotification;
      break;
    case Event::VALUE_CHANGED:
      mac_notification = NSAccessibilityValueChangedNotification;
      if (base::mac::IsAtLeastOS10_11() && !text_edits_.empty()) {
        base::string16 deleted_text;
        base::string16 inserted_text;
        int32_t id = node->GetId();
        const auto iterator = text_edits_.find(id);
        if (iterator != text_edits_.end()) {
          AXTextEdit text_edit = iterator->second;
          deleted_text = text_edit.deleted_text;
          inserted_text = text_edit.inserted_text;
        }

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
    case Event::LIVE_REGION_CREATED:
      mac_notification = NSAccessibilityLiveRegionCreatedNotification;
      break;
    case Event::ALERT:
      NSAccessibilityPostNotification(
          native_node, NSAccessibilityLiveRegionCreatedNotification);
      // Voiceover requires a live region changed notification to actually
      // announce the live region.
      FireGeneratedEvent(Event::LIVE_REGION_CHANGED, node);
      return;
    case Event::LIVE_REGION_CHANGED: {
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
      return;
    }
    case Event::ROW_COUNT_CHANGED:
      mac_notification = NSAccessibilityRowCountChangedNotification;
      break;
    case Event::EXPANDED:
      if (node->GetRole() == ax::mojom::Role::kRow ||
          node->GetRole() == ax::mojom::Role::kTreeItem) {
        mac_notification = NSAccessibilityRowExpandedNotification;
      } else {
        mac_notification = NSAccessibilityExpandedChanged;
      }
      break;
    case Event::COLLAPSED:
      if (node->GetRole() == ax::mojom::Role::kRow ||
          node->GetRole() == ax::mojom::Role::kTreeItem) {
        mac_notification = NSAccessibilityRowCollapsedNotification;
      } else {
        mac_notification = NSAccessibilityExpandedChanged;
      }
      break;
    case Event::MENU_ITEM_SELECTED:
      mac_notification = NSAccessibilityMenuItemSelectedNotification;
      break;
    case Event::CHILDREN_CHANGED:
    case Event::DESCRIPTION_CHANGED:
    case Event::DOCUMENT_TITLE_CHANGED:
    case Event::LIVE_REGION_NODE_CHANGED:
    case Event::NAME_CHANGED:
    case Event::OTHER_ATTRIBUTE_CHANGED:
    case Event::RELATED_NODE_CHANGED:
    case Event::ROLE_CHANGED:
    case Event::SCROLL_POSITION_CHANGED:
    case Event::SELECTED_CHANGED:
    case Event::STATE_CHANGED:
      // There are some notifications that aren't meaningful on Mac.
      // It's okay to skip them.
      return;
  }

  FireNativeMacNotification(mac_notification, node);
}

void BrowserAccessibilityManagerMac::FireNativeMacNotification(
    NSString* mac_notification,
    BrowserAccessibility* node) {
  if (!node->IsNative())
    return;

  DCHECK(mac_notification);
  auto native_node = ToBrowserAccessibilityCocoa(node);
  DCHECK(native_node);
  NSAccessibilityPostNotification(native_node, mac_notification);
}

void BrowserAccessibilityManagerMac::OnAccessibilityEvents(
    const AXEventNotificationDetails& details) {
  text_edits_.clear();
  // Call the base method last as it might delete the tree if it receives an
  // invalid message.
  BrowserAccessibilityManager::OnAccessibilityEvents(details);
}

void BrowserAccessibilityManagerMac::OnAtomicUpdateFinished(
    ui::AXTree* tree,
    bool root_changed,
    const std::vector<Change>& changes) {
  BrowserAccessibilityManager::OnAtomicUpdateFinished(tree, root_changed,
                                                      changes);

  std::set<const BrowserAccessibilityCocoa*> changed_editable_roots;
  for (const auto& change : changes) {
    const BrowserAccessibility* obj = GetFromAXNode(change.node);
    if (obj && obj->IsNative() && obj->HasState(ax::mojom::State::kEditable)) {
      const BrowserAccessibilityCocoa* editable_root =
          [ToBrowserAccessibilityCocoa(obj) editableAncestor];
      if (editable_root && [editable_root instanceActive])
        changed_editable_roots.insert(editable_root);
    }
  }

  for (const BrowserAccessibilityCocoa* obj : changed_editable_roots) {
    DCHECK(obj);
    const AXTextEdit text_edit = [obj computeTextEdit];
    if (!text_edit.IsEmpty())
      text_edits_[[obj owner]->GetId()] = text_edit;
  }
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
        NSString* const NSAccessibilitySelectedTextMarkerRangeAttribute =
            @"AXSelectedTextMarkerRange";
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
      NSAccessibilityTextEditType : @(AXTextEditTypeDelete),
      NSAccessibilityTextChangeValueLength : @(deleted_text.length()),
      NSAccessibilityTextChangeValue : base::SysUTF16ToNSString(deleted_text)
    }];
  }
  if (!inserted_text.empty()) {
    // TODO(nektar): Figure out if this is a paste operation instead of typing.
    // Changes to Blink would be required.
    [changes addObject:@{
      NSAccessibilityTextEditType : @(AXTextEditTypeTyping),
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
  return delegate()->AccessibilityGetNativeViewAccessible();
}

}  // namespace content
