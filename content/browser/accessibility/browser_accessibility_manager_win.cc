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
      tracked_scroll_object_(NULL) {
  ui::win::CreateATLModuleIfNeeded();
  Initialize(initial_tree);
  ui::GetIAccessible2UsageObserverList().AddObserver(this);
}

BrowserAccessibilityManagerWin::~BrowserAccessibilityManagerWin() {
  // Destroy the tree in the subclass, rather than in the inherited
  // destructor, otherwise our overrides of functions like
  // OnNodeWillBeDeleted won't be called.
  tree_.reset(NULL);

  if (tracked_scroll_object_) {
    tracked_scroll_object_->Release();
    tracked_scroll_object_ = NULL;
  }
  ui::GetIAccessible2UsageObserverList().RemoveObserver(this);
}

// static
ui::AXTreeUpdate
    BrowserAccessibilityManagerWin::GetEmptyDocument() {
  ui::AXNodeData empty_document;
  empty_document.id = 0;
  empty_document.role = ui::AX_ROLE_ROOT_WEB_AREA;
  empty_document.state =
      (1 << ui::AX_STATE_ENABLED) |
      (1 << ui::AX_STATE_READ_ONLY) |
      (1 << ui::AX_STATE_BUSY);

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

void BrowserAccessibilityManagerWin::MaybeCallNotifyWinEvent(
    DWORD event, BrowserAccessibility* node) {
  BrowserAccessibilityDelegate* delegate =
      node->manager()->GetDelegateFromRootManager();
  if (!delegate) {
    // This line and other LOG(WARNING) lines are temporary, to debug
    // flaky failures in DumpAccessibilityEvent* tests.
    // http://crbug.com/440579
    DLOG(WARNING) << "Not firing AX event because of no delegate";
    return;
  }

  if (!node->IsNative())
    return;

  HWND hwnd = delegate->AccessibilityGetAcceleratedWidget();
  if (!hwnd) {
    DLOG(WARNING) << "Not firing AX event because of no hwnd";
    return;
  }

  // Inline text boxes are an internal implementation detail, we don't
  // expose them to Windows.
  if (node->GetRole() == ui::AX_ROLE_INLINE_TEXT_BOX)
    return;

  // It doesn't make sense to fire a REORDER event on a leaf node; that
  // happens when the node has internal children line inline text boxes.
  if (event == EVENT_OBJECT_REORDER && node->PlatformChildCount() == 0)
    return;

  // Pass the negation of this node's unique id in the |child_id|
  // argument to NotifyWinEvent; the AT client will then call get_accChild
  // on the HWND's accessibility object and pass it that same id, which
  // we can use to retrieve the IAccessible for this node.
  LONG child_id = -node->unique_id();
  ::NotifyWinEvent(event, hwnd, OBJID_CLIENT, child_id);
}

void BrowserAccessibilityManagerWin::OnIAccessible2Used() {
  BrowserAccessibilityStateImpl::GetInstance()->OnScreenReaderDetected();
}

void BrowserAccessibilityManagerWin::UserIsReloading() {
  if (GetRoot())
    MaybeCallNotifyWinEvent(IA2_EVENT_DOCUMENT_RELOAD, GetRoot());
}

void BrowserAccessibilityManagerWin::NotifyAccessibilityEvent(
    ui::AXEvent event_type,
    BrowserAccessibility* node) {
  BrowserAccessibilityDelegate* root_delegate = GetDelegateFromRootManager();
  if (!root_delegate || !root_delegate->AccessibilityGetAcceleratedWidget()) {
    DLOG(WARNING) << "Not firing AX event because of no root_delegate or hwnd";
    return;
  }

  // Don't fire events when this document might be stale as the user has
  // started navigating to a new document.
  if (user_is_navigating_away_)
    return;

  // Inline text boxes are an internal implementation detail, we don't
  // expose them to Windows.
  if (node->GetRole() == ui::AX_ROLE_INLINE_TEXT_BOX)
    return;

  LONG event_id = EVENT_MIN;
  switch (event_type) {
    case ui::AX_EVENT_ACTIVEDESCENDANTCHANGED:
      event_id = IA2_EVENT_ACTIVE_DESCENDANT_CHANGED;
      break;
    case ui::AX_EVENT_ALERT:
      event_id = EVENT_SYSTEM_ALERT;
      break;
    case ui::AX_EVENT_AUTOCORRECTION_OCCURED:
      event_id = IA2_EVENT_OBJECT_ATTRIBUTE_CHANGED;
      break;
    case ui::AX_EVENT_BLUR:
      // Equivalent to focus on the root.
      event_id = EVENT_OBJECT_FOCUS;
      node = GetRoot();
      break;
    case ui::AX_EVENT_CHILDREN_CHANGED:
      event_id = EVENT_OBJECT_REORDER;
      break;
    case ui::AX_EVENT_FOCUS:
      event_id = EVENT_OBJECT_FOCUS;
      break;
    case ui::AX_EVENT_LIVE_REGION_CHANGED:
      if (node->GetBoolAttribute(ui::AX_ATTR_CONTAINER_LIVE_BUSY))
        return;
      event_id = EVENT_OBJECT_LIVEREGIONCHANGED;
      break;
    case ui::AX_EVENT_LOAD_COMPLETE:
      event_id = IA2_EVENT_DOCUMENT_LOAD_COMPLETE;
      break;
    case ui::AX_EVENT_SCROLL_POSITION_CHANGED:
      event_id = EVENT_SYSTEM_SCROLLINGEND;
      break;
    case ui::AX_EVENT_SCROLLED_TO_ANCHOR:
      event_id = EVENT_SYSTEM_SCROLLINGSTART;
      break;
    case ui::AX_EVENT_SELECTED_CHILDREN_CHANGED:
      event_id = EVENT_OBJECT_SELECTIONWITHIN;
      break;
    case ui::AX_EVENT_DOCUMENT_SELECTION_CHANGED: {
      // Fire the event on the object where the focus of the selection is.
      int32_t focus_id = GetTreeData().sel_focus_object_id;
      BrowserAccessibility* focus_object = GetFromID(focus_id);
      if (focus_object)
        node = focus_object;
      event_id = IA2_EVENT_TEXT_CARET_MOVED;
      break;
    }
    default:
      // Not all WebKit accessibility events result in a Windows
      // accessibility notification.
      break;
  }

  if (!node)
    return;

  if (event_id != EVENT_MIN)
    MaybeCallNotifyWinEvent(event_id, node);

  // If this is a layout complete notification (sent when a container scrolls)
  // and there is a descendant tracked object, send a notification on it.
  // TODO(dmazzoni): remove once http://crbug.com/113483 is fixed.
  if (event_type == ui::AX_EVENT_LAYOUT_COMPLETE &&
      tracked_scroll_object_ &&
      tracked_scroll_object_->IsDescendantOf(node)) {
    MaybeCallNotifyWinEvent(
        IA2_EVENT_VISIBLE_DATA_CHANGED, tracked_scroll_object_);
    tracked_scroll_object_->Release();
    tracked_scroll_object_ = NULL;
  }
}

bool BrowserAccessibilityManagerWin::CanFireEvents() {
  BrowserAccessibilityDelegate* root_delegate = GetDelegateFromRootManager();
  if (!root_delegate)
    return false;
  HWND hwnd = root_delegate->AccessibilityGetAcceleratedWidget();
  return hwnd != nullptr;
}

void BrowserAccessibilityManagerWin::FireFocusEvent(
    BrowserAccessibility* node) {
  // On Windows, we always fire a FOCUS event on the root of a frame before
  // firing a focus event within that frame.
  if (node->manager() != last_focused_manager_ &&
      node != node->manager()->GetRoot()) {
    NotifyAccessibilityEvent(ui::AX_EVENT_FOCUS, node->manager()->GetRoot());
  }

  BrowserAccessibilityManager::FireFocusEvent(node);
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

void BrowserAccessibilityManagerWin::OnNodeWillBeDeleted(ui::AXTree* tree,
                                                         ui::AXNode* node) {
  DCHECK(node);
  BrowserAccessibility* obj = GetFromAXNode(node);
  if (obj && obj->IsNative()) {
    if (obj == tracked_scroll_object_) {
      tracked_scroll_object_->Release();
      tracked_scroll_object_ = NULL;
    }
  }

  // Call the inherited function at the bottom, otherwise our call to
  // |GetFromAXNode|, above, will fail!
  BrowserAccessibilityManager::OnNodeWillBeDeleted(tree, node);
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
      ToBrowserAccessibilityWin(obj)->UpdateStep1ComputeWinAttributes();
  }

  // The next step updates the hypertext of each node, which is a
  // concatenation of all of its child text nodes, so it can't run until
  // the text of all of the nodes was computed in the previous step.
  for (size_t i = 0; i < changes.size(); ++i) {
    const ui::AXNode* changed_node = changes[i].node;
    DCHECK(changed_node);
    BrowserAccessibility* obj = GetFromAXNode(changed_node);
    if (obj && obj->IsNative() && !obj->PlatformIsChildOfLeaf())
      ToBrowserAccessibilityWin(obj)->UpdateStep2ComputeHypertext();
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
      ToBrowserAccessibilityWin(obj)->UpdateStep3FireEvents(
          changes[i].type == AXTreeDelegate::SUBTREE_CREATED);
    }
  }
}

void BrowserAccessibilityManagerWin::TrackScrollingObject(
    BrowserAccessibilityWin* node) {
  if (tracked_scroll_object_)
    tracked_scroll_object_->Release();
  tracked_scroll_object_ = node;
  tracked_scroll_object_->AddRef();
}

}  // namespace content
