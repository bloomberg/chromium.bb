// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/accessibility/browser_accessibility_manager_win.h"

#include <stddef.h>
#include <stdint.h>

#include <vector>

#include "base/command_line.h"
#include "base/win/scoped_comptr.h"
#include "base/win/windows_version.h"
#include "content/browser/accessibility/browser_accessibility_event_win.h"
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
  empty_document.AddState(ui::AX_STATE_BUSY);

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

IAccessible* BrowserAccessibilityManagerWin::GetParentIAccessible() {
  BrowserAccessibilityDelegate* delegate = GetDelegateFromRootManager();
  if (!delegate)
    return NULL;
  return delegate->AccessibilityGetNativeViewAccessible();
}

void BrowserAccessibilityManagerWin::OnIAccessible2Used() {
  // When IAccessible2 APIs have been used elsewhere in the codebase,
  // enable basic web accessibility support. (Full screen reader support is
  // detected later when specific more advanced APIs are accessed.)
  BrowserAccessibilityStateImpl::GetInstance()->AddAccessibilityModeFlags(
      AccessibilityMode::kNativeAPIs | AccessibilityMode::kWebContents);
}

void BrowserAccessibilityManagerWin::UserIsReloading() {
  if (GetRoot()) {
    (new BrowserAccessibilityEventWin(
        BrowserAccessibilityEvent::FromRenderFrameHost,
        ui::AX_EVENT_NONE,
        IA2_EVENT_DOCUMENT_RELOAD,
        GetRoot()))->Fire();
  }
}

BrowserAccessibility* BrowserAccessibilityManagerWin::GetFocus() {
  BrowserAccessibility* focus = BrowserAccessibilityManager::GetFocus();
  return GetActiveDescendant(focus);
}

void BrowserAccessibilityManagerWin::NotifyAccessibilityEvent(
    BrowserAccessibilityEvent::Source source,
    ui::AXEvent event_type,
    BrowserAccessibility* node) {
  bool can_fire_events = CanFireEvents();

  // TODO(dmazzoni): A better fix would be to always have a HWND.
  // http://crbug.com/521877
  if (event_type == ui::AX_EVENT_LOAD_COMPLETE && can_fire_events)
    load_complete_pending_ = false;

  if (load_complete_pending_ && can_fire_events && GetRoot()) {
    load_complete_pending_ = false;
    NotifyAccessibilityEvent(BrowserAccessibilityEvent::FromPendingLoadComplete,
                             ui::AX_EVENT_LOAD_COMPLETE,
                             GetRoot());
  }

  if (!can_fire_events &&
      !load_complete_pending_ &&
      event_type == ui::AX_EVENT_LOAD_COMPLETE &&
      GetRoot() &&
      !GetRoot()->HasState(ui::AX_STATE_OFFSCREEN) &&
      GetRoot()->PlatformChildCount() > 0) {
    load_complete_pending_ = true;
  }

  if (event_type == ui::AX_EVENT_BLUR) {
    // Equivalent to focus on the root.
    event_type = ui::AX_EVENT_FOCUS;
    node = GetRoot();
  }

  if (event_type == ui::AX_EVENT_DOCUMENT_SELECTION_CHANGED) {
    // Fire the event on the object where the focus of the selection is.
    int32_t focus_id = GetTreeData().sel_focus_object_id;
    BrowserAccessibility* focus_object = GetFromID(focus_id);
    if (focus_object) {
      (new BrowserAccessibilityEventWin(
          source,
          ui::AX_EVENT_NONE,
          IA2_EVENT_TEXT_CARET_MOVED,
          focus_object))->Fire();
      return;
    }
  }

  BrowserAccessibilityManager::NotifyAccessibilityEvent(
      source, event_type, node);
}

BrowserAccessibilityEvent::Result
    BrowserAccessibilityManagerWin::FireWinAccessibilityEvent(
        BrowserAccessibilityEventWin* event) {
  const BrowserAccessibility* target = event->target();
  ui::AXEvent event_type = event->event_type();
  LONG win_event_type = event->win_event_type();

  BrowserAccessibilityDelegate* root_delegate = GetDelegateFromRootManager();
  if (!root_delegate)
    return BrowserAccessibilityEvent::FailedBecauseFrameIsDetached;

  HWND hwnd = root_delegate->AccessibilityGetAcceleratedWidget();
  if (!hwnd)
    return BrowserAccessibilityEvent::FailedBecauseNoWindow;

  // Don't fire events when this document might be stale as the user has
  // started navigating to a new document.
  if (user_is_navigating_away_)
    return BrowserAccessibilityEvent::DiscardedBecauseUserNavigatingAway;

  // Inline text boxes are an internal implementation detail, we don't
  // expose them to Windows.
  if (target->GetRole() == ui::AX_ROLE_INLINE_TEXT_BOX)
    return BrowserAccessibilityEvent::NotNeededOnThisPlatform;

  if ((event_type == ui::AX_EVENT_LIVE_REGION_CREATED ||
       event_type == ui::AX_EVENT_LIVE_REGION_CHANGED) &&
      target->GetBoolAttribute(ui::AX_ATTR_CONTAINER_LIVE_BUSY)) {
    return BrowserAccessibilityEvent::DiscardedBecauseLiveRegionBusy;
  }

  if (!target)
    return BrowserAccessibilityEvent::FailedBecauseNoFocus;

  event->set_target(target);

  // It doesn't make sense to fire a REORDER event on a leaf node; that
  // happens when the target has internal children inline text boxes.
  if (win_event_type == EVENT_OBJECT_REORDER &&
      target->PlatformChildCount() == 0) {
    return BrowserAccessibilityEvent::NotNeededOnThisPlatform;
  }

  // Pass the negation of this node's unique id in the |child_id|
  // argument to NotifyWinEvent; the AT client will then call get_accChild
  // on the HWND's accessibility object and pass it that same id, which
  // we can use to retrieve the IAccessible for this node.
  LONG child_id = -target->unique_id();
  ::NotifyWinEvent(win_event_type, hwnd, OBJID_CLIENT, child_id);
  return BrowserAccessibilityEvent::Sent;
}

bool BrowserAccessibilityManagerWin::CanFireEvents() {
  BrowserAccessibilityDelegate* root_delegate = GetDelegateFromRootManager();
  if (!root_delegate)
    return false;
  HWND hwnd = root_delegate->AccessibilityGetAcceleratedWidget();
  return hwnd != nullptr;
}

void BrowserAccessibilityManagerWin::FireFocusEvent(
    BrowserAccessibilityEvent::Source source,
    BrowserAccessibility* node) {
  DCHECK(node);
  // On Windows, we always fire a FOCUS event on the root of a frame before
  // firing a focus event within that frame.
  if (node->manager() != last_focused_manager_ &&
      node != node->manager()->GetRoot()) {
    BrowserAccessibilityEvent::Create(source,
                                      ui::AX_EVENT_FOCUS,
                                      node->manager()->GetRoot())->Fire();
  }

  BrowserAccessibilityManager::FireFocusEvent(source, node);
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

void BrowserAccessibilityManagerWin::OnNodeCreated(ui::AXTree* tree,
                                                   ui::AXNode* node) {
  DCHECK(node);
  BrowserAccessibilityManager::OnNodeCreated(tree, node);
  BrowserAccessibility* obj = GetFromAXNode(node);
  if (!obj)
    return;
  if (!obj->IsNative())
    return;
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
  for (size_t i = 0; i < changes.size(); ++i) {
    const ui::AXNode* changed_node = changes[i].node;
    DCHECK(changed_node);
    BrowserAccessibility* obj = GetFromAXNode(changed_node);
    if (obj && obj->IsNative() && !obj->PlatformIsChildOfLeaf())
      ToBrowserAccessibilityWin(obj)
          ->GetCOM()
          ->UpdateStep1ComputeWinAttributes();
  }

  // The next step updates the hypertext of each node, which is a
  // concatenation of all of its child text nodes, so it can't run until
  // the text of all of the nodes was computed in the previous step.
  for (size_t i = 0; i < changes.size(); ++i) {
    const ui::AXNode* changed_node = changes[i].node;
    DCHECK(changed_node);
    BrowserAccessibility* obj = GetFromAXNode(changed_node);
    if (obj && obj->IsNative() && !obj->PlatformIsChildOfLeaf())
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
  for (size_t i = 0; i < changes.size(); ++i) {
    const ui::AXNode* changed_node = changes[i].node;
    DCHECK(changed_node);
    BrowserAccessibility* obj = GetFromAXNode(changed_node);
    if (obj && obj->IsNative() && !obj->PlatformIsChildOfLeaf()) {
      ToBrowserAccessibilityWin(obj)->GetCOM()->UpdateStep3FireEvents(
          changes[i].type == AXTreeDelegate::SUBTREE_CREATED);
    }
  }
}

}  // namespace content
