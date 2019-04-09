// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/accessibility/browser_accessibility_manager_win.h"

#include <stddef.h>
#include <stdint.h>

#include <vector>

#include "base/command_line.h"
#include "base/win/scoped_variant.h"
#include "base/win/windows_version.h"
#include "content/browser/accessibility/browser_accessibility_state_impl.h"
#include "content/browser/accessibility/browser_accessibility_win.h"
#include "content/browser/renderer_host/legacy_render_widget_host_win.h"
#include "content/common/accessibility_messages.h"
#include "content/public/common/content_switches.h"
#include "ui/accessibility/accessibility_switches.h"
#include "ui/accessibility/ax_role_properties.h"
#include "ui/accessibility/platform/ax_fragment_root_win.h"
#include "ui/accessibility/platform/ax_platform_node_delegate_utils_win.h"
#include "ui/base/win/atl_module.h"

namespace content {

// static
BrowserAccessibilityManager* BrowserAccessibilityManager::Create(
    const ui::AXTreeUpdate& initial_tree,
    BrowserAccessibilityDelegate* delegate,
    BrowserAccessibilityFactory* factory) {
  return new BrowserAccessibilityManagerWin(initial_tree, delegate, factory);
}

BrowserAccessibilityManagerWin*
BrowserAccessibilityManager::ToBrowserAccessibilityManagerWin() {
  return static_cast<BrowserAccessibilityManagerWin*>(this);
}

BrowserAccessibilityManagerWin::BrowserAccessibilityManagerWin(
    const ui::AXTreeUpdate& initial_tree,
    BrowserAccessibilityDelegate* delegate,
    BrowserAccessibilityFactory* factory)
    : BrowserAccessibilityManager(delegate, factory),
      load_complete_pending_(false) {
  ui::win::CreateATLModuleIfNeeded();
  Initialize(initial_tree);
}

BrowserAccessibilityManagerWin::~BrowserAccessibilityManagerWin() {
  // Destroy the tree in the subclass, rather than in the inherited
  // destructor, otherwise our overrides of functions like
  // OnNodeWillBeDeleted won't be called.
  tree_.reset(NULL);
}

// static
ui::AXTreeUpdate BrowserAccessibilityManagerWin::GetEmptyDocument() {
  ui::AXNodeData empty_document;
  empty_document.id = 0;
  empty_document.role = ax::mojom::Role::kRootWebArea;
  empty_document.AddBoolAttribute(ax::mojom::BoolAttribute::kBusy, true);

  ui::AXTreeUpdate update;
  update.root_id = empty_document.id;
  update.nodes.push_back(empty_document);
  return update;
}

HWND BrowserAccessibilityManagerWin::GetParentHWND() {
  BrowserAccessibilityDelegate* delegate = GetDelegateFromRootManager();
  if (!delegate)
    return NULL;
  return delegate->AccessibilityGetAcceleratedWidget();
}

void BrowserAccessibilityManagerWin::OnSubtreeWillBeDeleted(ui::AXTree* tree,
                                                            ui::AXNode* node) {
  BrowserAccessibilityManager::OnSubtreeWillBeDeleted(tree, node);

  BrowserAccessibility* obj = GetFromAXNode(node);
  FireWinAccessibilityEvent(EVENT_OBJECT_HIDE, obj);
  FireUiaStructureChangedEvent(StructureChangeType_ChildRemoved, obj);
}

void BrowserAccessibilityManagerWin::UserIsReloading() {
  if (GetRoot())
    FireWinAccessibilityEvent(IA2_EVENT_DOCUMENT_RELOAD, GetRoot());
}

BrowserAccessibility* BrowserAccessibilityManagerWin::GetFocus() {
  BrowserAccessibility* focus = BrowserAccessibilityManager::GetFocus();
  return GetActiveDescendant(focus);
}

void BrowserAccessibilityManagerWin::FireFocusEvent(
    BrowserAccessibility* node) {
  BrowserAccessibilityManager::FireFocusEvent(node);
  DCHECK(node);

  if (node->GetRole() == ax::mojom::Role::kMenu)
    FireUiaAccessibilityEvent(UIA_MenuOpenedEventId, node);

  FireWinAccessibilityEvent(EVENT_OBJECT_FOCUS, node);
  FireUiaAccessibilityEvent(UIA_AutomationFocusChangedEventId, node);
}

void BrowserAccessibilityManagerWin::FireBlinkEvent(
    ax::mojom::Event event_type,
    BrowserAccessibility* node) {
  BrowserAccessibilityManager::FireBlinkEvent(event_type, node);
  switch (event_type) {
    case ax::mojom::Event::kClicked:
      if (ui::IsInvokable(node->GetData()))
        FireUiaAccessibilityEvent(UIA_Invoke_InvokedEventId, node);
      break;
    case ax::mojom::Event::kEndOfTest: {
      if (::switches::IsExperimentalAccessibilityPlatformUIAEnabled()) {
        // Event tests use kEndOfTest as a sentinel to mark the end of the test.
        Microsoft::WRL::ComPtr<IUIAutomationRegistrar> registrar;
        CoCreateInstance(CLSID_CUIAutomationRegistrar, NULL,
                         CLSCTX_INPROC_SERVER, IID_IUIAutomationRegistrar,
                         &registrar);
        CHECK(registrar.Get());
        UIAutomationEventInfo custom_event = {kUiaTestCompleteSentinelGuid,
                                              kUiaTestCompleteSentinel};
        EVENTID custom_event_id = 0;
        CHECK(SUCCEEDED(
            registrar->RegisterEvent(&custom_event, &custom_event_id)));

        FireUiaAccessibilityEvent(custom_event_id, node);
      }
      break;
    }
    case ax::mojom::Event::kLocationChanged:
      FireWinAccessibilityEvent(IA2_EVENT_VISIBLE_DATA_CHANGED, node);
      break;
    case ax::mojom::Event::kScrolledToAnchor:
      FireWinAccessibilityEvent(EVENT_SYSTEM_SCROLLINGSTART, node);
      break;
    case ax::mojom::Event::kTextChanged:
      FireUiaTextContainerEvent(UIA_Text_TextChangedEventId, node);
      break;
    case ax::mojom::Event::kTextSelectionChanged:
      FireUiaTextContainerEvent(UIA_Text_TextSelectionChangedEventId, node);
      break;
    default:
      break;
  }
}

void BrowserAccessibilityManagerWin::FireGeneratedEvent(
    ui::AXEventGenerator::Event event_type,
    BrowserAccessibility* node) {
  BrowserAccessibilityManager::FireGeneratedEvent(event_type, node);
  bool can_fire_events = CanFireEvents();

  if (event_type == ui::AXEventGenerator::Event::LOAD_COMPLETE &&
      can_fire_events)
    load_complete_pending_ = false;

  if (load_complete_pending_ && can_fire_events && GetRoot()) {
    load_complete_pending_ = false;
    FireWinAccessibilityEvent(IA2_EVENT_DOCUMENT_LOAD_COMPLETE, GetRoot());
  }

  if (!can_fire_events && !load_complete_pending_ &&
      event_type == ui::AXEventGenerator::Event::LOAD_COMPLETE && GetRoot() &&
      !GetRoot()->IsOffscreen() && GetRoot()->PlatformChildCount() > 0) {
    load_complete_pending_ = true;
  }

  switch (event_type) {
    case ui::AXEventGenerator::Event::ACCESS_KEY_CHANGED:
      FireUiaPropertyChangedEvent(UIA_AccessKeyPropertyId, node);
      break;
    case ui::AXEventGenerator::Event::ACTIVE_DESCENDANT_CHANGED:
      FireWinAccessibilityEvent(IA2_EVENT_ACTIVE_DESCENDANT_CHANGED, node);
      break;
    case ui::AXEventGenerator::Event::ALERT:
      FireWinAccessibilityEvent(EVENT_SYSTEM_ALERT, node);
      FireUiaAccessibilityEvent(UIA_SystemAlertEventId, node);
      break;
    case ui::AXEventGenerator::Event::CHECKED_STATE_CHANGED:
      FireUiaPropertyChangedEvent(UIA_ToggleToggleStatePropertyId, node);
      break;
    case ui::AXEventGenerator::Event::CHILDREN_CHANGED:
      FireWinAccessibilityEvent(EVENT_OBJECT_REORDER, node);
      FireUiaStructureChangedEvent(StructureChangeType_ChildrenReordered, node);
      break;
    case ui::AXEventGenerator::Event::CLASS_NAME_CHANGED:
      FireUiaPropertyChangedEvent(UIA_ClassNamePropertyId, node);
      break;
    case ui::AXEventGenerator::Event::COLLAPSED:
    case ui::AXEventGenerator::Event::EXPANDED:
      FireUiaPropertyChangedEvent(
          UIA_ExpandCollapseExpandCollapseStatePropertyId, node);
      break;
    case ui::AXEventGenerator::Event::CONTROLS_CHANGED:
      FireUiaPropertyChangedEvent(UIA_ControllerForPropertyId, node);
      break;
    case ui::AXEventGenerator::Event::DESCRIBED_BY_CHANGED:
      FireUiaPropertyChangedEvent(UIA_DescribedByPropertyId, node);
      break;
    case ui::AXEventGenerator::Event::DESCRIPTION_CHANGED:
      FireUiaPropertyChangedEvent(UIA_FullDescriptionPropertyId, node);
      break;
    case ui::AXEventGenerator::Event::DOCUMENT_SELECTION_CHANGED: {
      // Fire the event on the object where the focus of the selection is.
      int32_t focus_id = GetTreeData().sel_focus_object_id;
      BrowserAccessibility* focus_object = GetFromID(focus_id);
      if (focus_object && focus_object->HasVisibleCaretOrSelection())
        FireWinAccessibilityEvent(IA2_EVENT_TEXT_CARET_MOVED, focus_object);
      break;
    }
    case ui::AXEventGenerator::Event::ENABLED_CHANGED:
      FireUiaPropertyChangedEvent(UIA_IsEnabledPropertyId, node);
      break;
    case ui::AXEventGenerator::Event::FLOW_FROM_CHANGED:
      FireUiaPropertyChangedEvent(UIA_FlowsFromPropertyId, node);
      break;
    case ui::AXEventGenerator::Event::FLOW_TO_CHANGED:
      FireUiaPropertyChangedEvent(UIA_FlowsToPropertyId, node);
      break;
    case ui::AXEventGenerator::Event::HIERARCHICAL_LEVEL_CHANGED:
      FireUiaPropertyChangedEvent(UIA_LevelPropertyId, node);
      break;
    case ui::AXEventGenerator::Event::IMAGE_ANNOTATION_CHANGED:
      FireWinAccessibilityEvent(EVENT_OBJECT_NAMECHANGE, node);
      break;
    case ui::AXEventGenerator::Event::INVALID_STATUS_CHANGED:
      FireUiaPropertyChangedEvent(UIA_IsDataValidForFormPropertyId, node);
      break;
    case ui::AXEventGenerator::Event::KEY_SHORTCUTS_CHANGED:
      FireUiaPropertyChangedEvent(UIA_AcceleratorKeyPropertyId, node);
      break;
    case ui::AXEventGenerator::Event::LABELED_BY_CHANGED:
      FireUiaPropertyChangedEvent(UIA_LabeledByPropertyId, node);
      break;
    case ui::AXEventGenerator::Event::LANGUAGE_CHANGED:
      FireUiaPropertyChangedEvent(UIA_CulturePropertyId, node);
      break;
    case ui::AXEventGenerator::Event::LIVE_REGION_CREATED:
      FireUiaPropertyChangedEvent(UIA_LiveSettingPropertyId, node);
      break;
    case ui::AXEventGenerator::Event::LIVE_REGION_CHANGED:
      // This event is redundant with the IA2_EVENT_TEXT_INSERTED events;
      // however, JAWS 2018 and earlier do not process the text inserted
      // events when "virtual cursor mode" is turned off (Insert+Z).
      // Fortunately, firing the redudant event does not cause duplicate
      // verbalizations in either screen reader.
      // Future versions of JAWS may process the text inserted event when
      // in focus mode, and so at some point the live region
      // changed events may truly become redundant with the text inserted
      // events. Note: Firefox does not fire this event, but JAWS processes
      // Firefox live region events differently (utilizes MSAA's
      // EVENT_OBJECT_SHOW).
      FireWinAccessibilityEvent(EVENT_OBJECT_LIVEREGIONCHANGED, node);
      break;
    case ui::AXEventGenerator::Event::LOAD_COMPLETE:
      FireWinAccessibilityEvent(IA2_EVENT_DOCUMENT_LOAD_COMPLETE, node);
      break;
    case ui::AXEventGenerator::Event::LAYOUT_INVALIDATED:
      FireUiaAccessibilityEvent(UIA_LayoutInvalidatedEventId, node);
      break;
    case ui::AXEventGenerator::Event::MULTISELECTABLE_STATE_CHANGED:
      FireUiaPropertyChangedEvent(UIA_SelectionCanSelectMultiplePropertyId,
                                  node);
      break;
    case ui::AXEventGenerator::Event::NAME_CHANGED:
      FireUiaPropertyChangedEvent(UIA_NamePropertyId, node);
      break;
    case ui::AXEventGenerator::Event::PLACEHOLDER_CHANGED:
      FireUiaPropertyChangedEvent(UIA_HelpTextPropertyId, node);
      break;
    case ui::AXEventGenerator::Event::POSITION_IN_SET_CHANGED:
      FireUiaPropertyChangedEvent(UIA_PositionInSetPropertyId, node);
      break;
    case ui::AXEventGenerator::Event::READONLY_CHANGED:
      if (ui::IsRangeValueSupported(node->GetData()))
        FireUiaPropertyChangedEvent(UIA_RangeValueIsReadOnlyPropertyId, node);
      else if (ui::IsValuePatternSupported(node))
        FireUiaPropertyChangedEvent(UIA_ValueIsReadOnlyPropertyId, node);
      break;
    case ui::AXEventGenerator::Event::REQUIRED_STATE_CHANGED:
      FireUiaPropertyChangedEvent(UIA_IsRequiredForFormPropertyId, node);
      break;
    case ui::AXEventGenerator::Event::ROLE_CHANGED:
      FireUiaPropertyChangedEvent(UIA_AriaRolePropertyId, node);
      break;
    case ui::AXEventGenerator::Event::SCROLL_HORIZONTAL_POSITION_CHANGED:
      FireWinAccessibilityEvent(EVENT_SYSTEM_SCROLLINGEND, node);
      FireUiaPropertyChangedEvent(UIA_ScrollHorizontalScrollPercentPropertyId,
                                  node);
      break;
    case ui::AXEventGenerator::Event::SCROLL_VERTICAL_POSITION_CHANGED:
      FireWinAccessibilityEvent(EVENT_SYSTEM_SCROLLINGEND, node);
      FireUiaPropertyChangedEvent(UIA_ScrollVerticalScrollPercentPropertyId,
                                  node);
      break;
    case ui::AXEventGenerator::Event::SELECTED_CHANGED:
      HandleSelectedStateChanged(node);
      break;
    case ui::AXEventGenerator::Event::SELECTED_CHILDREN_CHANGED:
      FireWinAccessibilityEvent(EVENT_OBJECT_SELECTIONWITHIN, node);
      break;
    case ui::AXEventGenerator::Event::SET_SIZE_CHANGED:
      FireUiaPropertyChangedEvent(UIA_SizeOfSetPropertyId, node);
      break;
    case ui::AXEventGenerator::Event::SUBTREE_CREATED:
      FireWinAccessibilityEvent(EVENT_OBJECT_SHOW, node);
      FireUiaStructureChangedEvent(StructureChangeType_ChildAdded, node);
      break;
    case ui::AXEventGenerator::Event::VALUE_CHANGED:
      FireWinAccessibilityEvent(EVENT_OBJECT_VALUECHANGE, node);
      if (ui::IsRangeValueSupported(node->GetData()))
        FireUiaPropertyChangedEvent(UIA_RangeValueValuePropertyId, node);
      else if (ui::IsValuePatternSupported(node))
        FireUiaPropertyChangedEvent(UIA_ValueValuePropertyId, node);
      break;
    case ui::AXEventGenerator::Event::VALUE_MAX_CHANGED:
      if (IsRangeValueSupported(node->GetData()))
        FireUiaPropertyChangedEvent(UIA_RangeValueMaximumPropertyId, node);
      break;
    case ui::AXEventGenerator::Event::VALUE_MIN_CHANGED:
      if (IsRangeValueSupported(node->GetData()))
        FireUiaPropertyChangedEvent(UIA_RangeValueMinimumPropertyId, node);
      break;
    case ui::AXEventGenerator::Event::VALUE_STEP_CHANGED:
      if (IsRangeValueSupported(node->GetData())) {
        FireUiaPropertyChangedEvent(UIA_RangeValueSmallChangePropertyId, node);
        FireUiaPropertyChangedEvent(UIA_RangeValueLargeChangePropertyId, node);
      }
      break;
    case ui::AXEventGenerator::Event::AUTO_COMPLETE_CHANGED:
    case ui::AXEventGenerator::Event::DOCUMENT_TITLE_CHANGED:
    case ui::AXEventGenerator::Event::LIVE_REGION_NODE_CHANGED:
    case ui::AXEventGenerator::Event::LOAD_START:
    case ui::AXEventGenerator::Event::MENU_ITEM_SELECTED:
    case ui::AXEventGenerator::Event::OTHER_ATTRIBUTE_CHANGED:
    case ui::AXEventGenerator::Event::RELATED_NODE_CHANGED:
    case ui::AXEventGenerator::Event::ROW_COUNT_CHANGED:
    case ui::AXEventGenerator::Event::STATE_CHANGED:
      // There are some notifications that aren't meaningful on Windows.
      // It's okay to skip them.
      break;
  }
}

void BrowserAccessibilityManagerWin::OnFocusLost(BrowserAccessibility* node) {
  BrowserAccessibilityManager::OnFocusLost(node);
  DCHECK(node);

  if (node->GetRole() == ax::mojom::Role::kMenu)
    FireUiaAccessibilityEvent(UIA_MenuClosedEventId, node);
}

void BrowserAccessibilityManagerWin::FireWinAccessibilityEvent(
    LONG win_event_type,
    BrowserAccessibility* node) {
  if (::switches::IsExperimentalAccessibilityPlatformUIAEnabled())
    return;
  if (!ShouldFireEventForNode(node))
    return;

  HWND hwnd = GetParentHWND();
  if (!hwnd)
    return;

  // Pass the negation of this node's unique id in the |child_id|
  // argument to NotifyWinEvent; the AT client will then call get_accChild
  // on the HWND's accessibility object and pass it that same id, which
  // we can use to retrieve the IAccessible for this node.
  LONG child_id = -(ToBrowserAccessibilityWin(node)->GetCOM()->GetUniqueId());
  ::NotifyWinEvent(win_event_type, hwnd, OBJID_CLIENT, child_id);
}

void BrowserAccessibilityManagerWin::FireUiaAccessibilityEvent(
    LONG uia_event,
    BrowserAccessibility* node) {
  if (!::switches::IsExperimentalAccessibilityPlatformUIAEnabled())
    return;
  if (!ShouldFireEventForNode(node))
    return;

  ::UiaRaiseAutomationEvent(ToBrowserAccessibilityWin(node)->GetCOM(),
                            uia_event);
}

void BrowserAccessibilityManagerWin::FireUiaPropertyChangedEvent(
    LONG uia_property,
    BrowserAccessibility* node) {
  if (!::switches::IsExperimentalAccessibilityPlatformUIAEnabled())
    return;
  if (!ShouldFireEventForNode(node))
    return;

  // The old value is not used by the system
  VARIANT old_value = {};
  old_value.vt = VT_EMPTY;

  auto* provider = ToBrowserAccessibilityWin(node)->GetCOM();
  base::win::ScopedVariant new_value;
  if (SUCCEEDED(
          provider->GetPropertyValue(uia_property, new_value.Receive()))) {
    ::UiaRaiseAutomationPropertyChangedEvent(provider, uia_property, old_value,
                                             new_value);
  }
}

void BrowserAccessibilityManagerWin::FireUiaStructureChangedEvent(
    StructureChangeType change_type,
    BrowserAccessibility* node) {
  if (!::switches::IsExperimentalAccessibilityPlatformUIAEnabled())
    return;
  if (!ShouldFireEventForNode(node))
    return;

  auto* provider = ToBrowserAccessibilityWin(node);
  auto* provider_com = provider ? provider->GetCOM() : nullptr;
  if (!provider || !provider_com)
    return;

  switch (change_type) {
    case StructureChangeType_ChildRemoved: {
      // 'ChildRemoved' fires on the parent and provides the runtime ID of
      // the removed child (which was passed as |node|).
      auto* parent = ToBrowserAccessibilityWin(node->PlatformGetParent());
      auto* parent_com = parent ? parent->GetCOM() : nullptr;
      if (parent && parent_com) {
        ui::AXPlatformNodeWin::RuntimeIdArray runtime_id;
        provider_com->GetRuntimeIdArray(runtime_id);
        UiaRaiseStructureChangedEvent(parent_com, change_type,
                                      runtime_id.data(), runtime_id.size());
      }
      break;
    }

    default: {
      // All other types are fired on |node|.  For 'ChildAdded' |node| is the
      // child that was added; for other types, it's the parent container.
      UiaRaiseStructureChangedEvent(provider_com, change_type, nullptr, 0);
    }
  }
}

void BrowserAccessibilityManagerWin::FireUiaTextContainerEvent(
    LONG uia_event,
    BrowserAccessibility* node) {
  // If the node supports text pattern, fire the event from itself, otherwise,
  // fire the event from the closest ancestor that supports text pattern.
  while (node) {
    if (ToBrowserAccessibilityWin(node)->GetCOM()->IsPatternProviderSupported(
            UIA_TextPatternId)) {
      FireUiaAccessibilityEvent(uia_event, node);
      return;
    }
    node = node->PlatformGetParent();
  }
}

bool BrowserAccessibilityManagerWin::CanFireEvents() {
  if (!BrowserAccessibilityManager::CanFireEvents())
    return false;
  BrowserAccessibilityDelegate* root_delegate = GetDelegateFromRootManager();
  if (!root_delegate)
    return false;
  HWND hwnd = root_delegate->AccessibilityGetAcceleratedWidget();
  return hwnd != nullptr;
}

gfx::Rect BrowserAccessibilityManagerWin::GetViewBounds() {
  // We have to take the device scale factor into account on Windows.
  BrowserAccessibilityDelegate* delegate = GetDelegateFromRootManager();
  if (delegate) {
    gfx::Rect bounds = delegate->AccessibilityGetViewBounds();
    if (device_scale_factor() > 0.0 && device_scale_factor() != 1.0)
      bounds = ScaleToEnclosingRect(bounds, device_scale_factor());
    return bounds;
  }
  return gfx::Rect();
}

void BrowserAccessibilityManagerWin::OnAtomicUpdateFinished(
    ui::AXTree* tree,
    bool root_changed,
    const std::vector<ui::AXTreeObserver::Change>& changes) {
  BrowserAccessibilityManager::OnAtomicUpdateFinished(tree, root_changed,
                                                      changes);

  if (root_changed && IsRootTree() &&
      switches::IsExperimentalAccessibilityPlatformUIAEnabled()) {
    // If a fragment root has been created, inform it that the content root has
    // changed.
    BrowserAccessibilityDelegate* root_delegate = GetDelegateFromRootManager();
    if (root_delegate) {
      ui::AXFragmentRootWin* fragment_root =
          ui::AXFragmentRootWin::GetForAcceleratedWidget(
              root_delegate->AccessibilityGetAcceleratedWidget());
      if (fragment_root) {
        fragment_root->SetChild(ToBrowserAccessibilityWin(GetRoot())->GetCOM());
      }
    }
  }

  // Do a sequence of Windows-specific updates on each node. Each one is
  // done in a single pass that must complete before the next step starts.
  // The nodes that need to be updated are all of the nodes that were changed,
  // plus some parents.
  std::map<BrowserAccessibilityComWin*, bool /* is_subtree_created */>
      objs_to_update;
  for (const auto& change : changes) {
    const ui::AXNode* changed_node = change.node;
    DCHECK(changed_node);

    bool is_subtree_created = change.type == AXTreeObserver::SUBTREE_CREATED;
    BrowserAccessibility* obj = GetFromAXNode(changed_node);
    if (obj && obj->IsNative()) {
      objs_to_update[ToBrowserAccessibilityWin(obj)->GetCOM()] =
          is_subtree_created;
    }

    // When a node is a text node or line break, update its parent, because
    // its text is part of its hypertext.
    const ui::AXNode* parent = changed_node->parent();
    if (!parent)
      continue;
    if (ui::IsTextOrLineBreak(changed_node->data().role)) {
      BrowserAccessibility* parent_obj = GetFromAXNode(parent);
      if (parent_obj && parent_obj->IsNative()) {
        BrowserAccessibilityComWin* parent_com_obj =
            ToBrowserAccessibilityWin(parent_obj)->GetCOM();
        if (objs_to_update.find(parent_com_obj) == objs_to_update.end())
          objs_to_update[parent_com_obj] = false;
      }
    }

    // When a node is editable, update the editable root too.
    if (!changed_node->data().HasState(ax::mojom::State::kEditable))
      continue;
    const ui::AXNode* editable_root = changed_node;
    while (editable_root->parent() && editable_root->parent()->data().HasState(
                                          ax::mojom::State::kEditable)) {
      editable_root = editable_root->parent();
    }
    BrowserAccessibility* editable_root_obj = GetFromAXNode(editable_root);
    if (editable_root_obj && editable_root_obj->IsNative()) {
      BrowserAccessibilityComWin* editable_root_com_obj =
          ToBrowserAccessibilityWin(editable_root_obj)->GetCOM();
      if (objs_to_update.find(editable_root_com_obj) == objs_to_update.end())
        objs_to_update[editable_root_com_obj] = false;
    }
  }

  // The first step moves win_attributes_ to old_win_attributes_ and then
  // recomputes all of win_attributes_ other than IAccessibleText.
  for (auto& key_value : objs_to_update)
    key_value.first->UpdateStep1ComputeWinAttributes();

  // The next step updates the hypertext of each node, which is a
  // concatenation of all of its child text nodes, so it can't run until
  // the text of all of the nodes was computed in the previous step.
  for (auto& key_value : objs_to_update)
    key_value.first->UpdateStep2ComputeHypertext();

  // The third step fires events on nodes based on what's changed - like
  // if the name, value, or description changed, or if the hypertext had
  // text inserted or removed. It's able to figure out exactly what changed
  // because we still have old_win_attributes_ populated.
  // This step has to run after the previous two steps complete because the
  // client may walk the tree when it receives any of these events.
  // At the end, it deletes old_win_attributes_ since they're not needed
  // anymore.
  for (auto& key_value : objs_to_update) {
    BrowserAccessibilityComWin* obj = key_value.first;
    bool is_subtree_created = key_value.second;
    obj->UpdateStep3FireEvents(is_subtree_created);
  }
}

bool BrowserAccessibilityManagerWin::ShouldFireEventForNode(
    BrowserAccessibility* node) {
  if (!node || !node->CanFireEvents())
    return false;

  // If the root delegate isn't the main-frame, this may be a new frame that
  // hasn't yet been swapped in or added to the frame tree. Suppress firing
  // events until then.
  BrowserAccessibilityDelegate* root_delegate = GetDelegateFromRootManager();
  if (!root_delegate)
    return false;
  if (!root_delegate->AccessibilityIsMainFrame())
    return false;

  // Don't fire events when this document might be stale as the user has
  // started navigating to a new document.
  if (user_is_navigating_away_)
    return false;

  // Inline text boxes are an internal implementation detail, we don't
  // expose them to Windows.
  if (node->GetRole() == ax::mojom::Role::kInlineTextBox)
    return false;

  return true;
}

void BrowserAccessibilityManagerWin::HandleSelectedStateChanged(
    BrowserAccessibility* node) {
  const bool is_selected =
      node->GetBoolAttribute(ax::mojom::BoolAttribute::kSelected);

  bool multiselect = false;
  auto* selection_container = node->PlatformGetSelectionContainer();
  if (selection_container &&
      selection_container->HasState(ax::mojom::State::kMultiselectable))
    multiselect = true;

  if (multiselect) {
    if (is_selected) {
      FireWinAccessibilityEvent(EVENT_OBJECT_SELECTIONADD, node);
      FireUiaAccessibilityEvent(
          UIA_SelectionItem_ElementAddedToSelectionEventId, node);
    } else {
      FireWinAccessibilityEvent(EVENT_OBJECT_SELECTIONREMOVE, node);
      FireUiaAccessibilityEvent(
          UIA_SelectionItem_ElementRemovedFromSelectionEventId, node);
    }
  } else if (is_selected) {
    FireWinAccessibilityEvent(EVENT_OBJECT_SELECTION, node);
    FireUiaAccessibilityEvent(UIA_SelectionItem_ElementSelectedEventId, node);
  }
}

}  // namespace content
