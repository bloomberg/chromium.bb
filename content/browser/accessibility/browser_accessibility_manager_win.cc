// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/accessibility/browser_accessibility_manager_win.h"

#include <stddef.h>
#include <stdint.h>

#include <vector>

#include "base/command_line.h"
#include "base/win/windows_version.h"
#include "content/browser/accessibility/browser_accessibility_state_impl.h"
#include "content/browser/accessibility/browser_accessibility_win.h"
#include "content/browser/renderer_host/legacy_render_widget_host_win.h"
#include "content/common/accessibility_messages.h"
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
  ui::GetIAccessible2UsageObserverList().AddObserver(this);
}

BrowserAccessibilityManagerWin::~BrowserAccessibilityManagerWin() {
  // Destroy the tree in the subclass, rather than in the inherited
  // destructor, otherwise our overrides of functions like
  // OnNodeWillBeDeleted won't be called.
  tree_.reset(NULL);
  ui::GetIAccessible2UsageObserverList().RemoveObserver(this);
}

// static
ui::AXTreeUpdate
    BrowserAccessibilityManagerWin::GetEmptyDocument() {
  ui::AXNodeData empty_document;
  empty_document.id = 0;
  empty_document.role = ui::AX_ROLE_ROOT_WEB_AREA;
  empty_document.AddBoolAttribute(ui::AX_ATTR_BUSY, true);

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

void BrowserAccessibilityManagerWin::OnIAccessible2Used() {
  // When IAccessible2 APIs have been used elsewhere in the codebase,
  // enable basic web accessibility support. (Full screen reader support is
  // detected later when specific more advanced APIs are accessed.)
  BrowserAccessibilityStateImpl::GetInstance()->AddAccessibilityModeFlags(
      ui::AXMode::kNativeAPIs | ui::AXMode::kWebContents);
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
  // On Windows, we always fire a FOCUS event on the root of a frame before
  // firing a focus event within that frame.
  if (node->manager() != last_focused_manager_ &&
      node != node->manager()->GetRoot()) {
    FireWinAccessibilityEvent(EVENT_OBJECT_FOCUS, node->manager()->GetRoot());
  }

  FireWinAccessibilityEvent(EVENT_OBJECT_FOCUS, node);
}

void BrowserAccessibilityManagerWin::FireBlinkEvent(
    ui::AXEvent event_type,
    BrowserAccessibility* node) {
  BrowserAccessibilityManager::FireBlinkEvent(event_type, node);
  switch (event_type) {
    case ui::AX_EVENT_LOCATION_CHANGED:
      FireWinAccessibilityEvent(IA2_EVENT_VISIBLE_DATA_CHANGED, node);
      break;
    case ui::AX_EVENT_SCROLLED_TO_ANCHOR:
      FireWinAccessibilityEvent(EVENT_SYSTEM_SCROLLINGSTART, node);
      break;
    default:
      break;
  }
}

void BrowserAccessibilityManagerWin::FireGeneratedEvent(
    AXEventGenerator::Event event_type,
    BrowserAccessibility* node) {
  BrowserAccessibilityManager::FireGeneratedEvent(event_type, node);
  bool can_fire_events = CanFireEvents();

  if (event_type == Event::LOAD_COMPLETE && can_fire_events)
    load_complete_pending_ = false;

  if (load_complete_pending_ && can_fire_events && GetRoot()) {
    load_complete_pending_ = false;
    FireWinAccessibilityEvent(IA2_EVENT_DOCUMENT_LOAD_COMPLETE, GetRoot());
  }

  if (!can_fire_events && !load_complete_pending_ &&
      event_type == Event::LOAD_COMPLETE && GetRoot() &&
      !GetRoot()->IsOffscreen() && GetRoot()->PlatformChildCount() > 0) {
    load_complete_pending_ = true;
  }

  switch (event_type) {
    case Event::ACTIVE_DESCENDANT_CHANGED:
      FireWinAccessibilityEvent(IA2_EVENT_ACTIVE_DESCENDANT_CHANGED, node);
      break;
    case Event::ALERT:
      FireWinAccessibilityEvent(EVENT_SYSTEM_ALERT, node);
      break;
    case Event::CHILDREN_CHANGED:
      FireWinAccessibilityEvent(EVENT_OBJECT_REORDER, node);
      break;
    case Event::LIVE_REGION_CHANGED:
      FireWinAccessibilityEvent(EVENT_OBJECT_LIVEREGIONCHANGED, node);
      break;
    case Event::LOAD_COMPLETE:
      FireWinAccessibilityEvent(IA2_EVENT_DOCUMENT_LOAD_COMPLETE, node);
      break;
    case Event::SCROLL_POSITION_CHANGED:
      FireWinAccessibilityEvent(EVENT_SYSTEM_SCROLLINGEND, node);
      break;
    case Event::SELECTED_CHILDREN_CHANGED:
      FireWinAccessibilityEvent(EVENT_OBJECT_SELECTIONWITHIN, node);
      break;
    case Event::DOCUMENT_SELECTION_CHANGED: {
      // Fire the event on the object where the focus of the selection is.
      int32_t focus_id = GetTreeData().sel_focus_object_id;
      BrowserAccessibility* focus_object = GetFromID(focus_id);
      if (focus_object)
        FireWinAccessibilityEvent(IA2_EVENT_TEXT_CARET_MOVED, focus_object);
      break;
    }
    case Event::CHECKED_STATE_CHANGED:
    case Event::COLLAPSED:
    case Event::DESCRIPTION_CHANGED:
    case Event::DOCUMENT_TITLE_CHANGED:
    case Event::EXPANDED:
    case Event::INVALID_STATUS_CHANGED:
    case Event::LIVE_REGION_CREATED:
    case Event::LIVE_REGION_NODE_CHANGED:
    case Event::MENU_ITEM_SELECTED:
    case Event::NAME_CHANGED:
    case Event::OTHER_ATTRIBUTE_CHANGED:
    case Event::ROLE_CHANGED:
    case Event::ROW_COUNT_CHANGED:
    case Event::SELECTED_CHANGED:
    case Event::STATE_CHANGED:
    case Event::VALUE_CHANGED:
      // There are some notifications that aren't meaningful on Windows.
      // It's okay to skip them.
      break;
  }
}

void BrowserAccessibilityManagerWin::FireWinAccessibilityEvent(
    LONG win_event_type,
    BrowserAccessibility* node) {
  // If there's no root delegate, this may be a new frame that hasn't
  // yet been swapped in or added to the frame tree. Suppress firing events
  // until then.
  BrowserAccessibilityDelegate* root_delegate = GetDelegateFromRootManager();
  if (!root_delegate)
    return;

  HWND hwnd = root_delegate->AccessibilityGetAcceleratedWidget();
  if (!hwnd)
    return;

  // Don't fire events when this document might be stale as the user has
  // started navigating to a new document.
  if (user_is_navigating_away_)
    return;

  // Inline text boxes are an internal implementation detail, we don't
  // expose them to Windows.
  if (node->GetRole() == ui::AX_ROLE_INLINE_TEXT_BOX)
    return;

  // Pass the negation of this node's unique id in the |child_id|
  // argument to NotifyWinEvent; the AT client will then call get_accChild
  // on the HWND's accessibility object and pass it that same id, which
  // we can use to retrieve the IAccessible for this node.
  LONG child_id = -(ToBrowserAccessibilityWin(node)->GetCOM()->unique_id());
  ::NotifyWinEvent(win_event_type, hwnd, OBJID_CLIENT, child_id);
}

bool BrowserAccessibilityManagerWin::CanFireEvents() {
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
    const std::vector<ui::AXTreeDelegate::Change>& changes) {
  BrowserAccessibilityManager::OnAtomicUpdateFinished(
      tree, root_changed, changes);

  // Do a sequence of Windows-specific updates on each node. Each one is
  // done in a single pass that must complete before the next step starts.
  // The first step moves win_attributes_ to old_win_attributes_ and then
  // recomputes all of win_attributes_ other than IAccessibleText.
  for (const auto& change : changes) {
    const ui::AXNode* changed_node = change.node;
    DCHECK(changed_node);
    BrowserAccessibility* obj = GetFromAXNode(changed_node);
    if (obj && obj->IsNative()) {
      ToBrowserAccessibilityWin(obj)
          ->GetCOM()
          ->UpdateStep1ComputeWinAttributes();
    }
  }

  // The next step updates the hypertext of each node, which is a
  // concatenation of all of its child text nodes, so it can't run until
  // the text of all of the nodes was computed in the previous step.
  for (const auto& change : changes) {
    const ui::AXNode* changed_node = change.node;
    DCHECK(changed_node);
    BrowserAccessibility* obj = GetFromAXNode(changed_node);
    if (obj && obj->IsNative())
      ToBrowserAccessibilityWin(obj)->GetCOM()->UpdateStep2ComputeHypertext();
  }

  // The third step fires events on nodes based on what's changed - like
  // if the name, value, or description changed, or if the hypertext had
  // text inserted or removed. It's able to figure out exactly what changed
  // because we still have old_win_attributes_ populated.
  // This step has to run after the previous two steps complete because the
  // client may walk the tree when it receives any of these events.
  // At the end, it deletes old_win_attributes_ since they're not needed
  // anymore.
  for (const auto& change : changes) {
    const ui::AXNode* changed_node = change.node;
    DCHECK(changed_node);
    BrowserAccessibility* obj = GetFromAXNode(changed_node);
    if (obj && obj->IsNative()) {
      ToBrowserAccessibilityWin(obj)->GetCOM()->UpdateStep3FireEvents(
          change.type == AXTreeDelegate::SUBTREE_CREATED);
    }
  }
}

}  // namespace content
