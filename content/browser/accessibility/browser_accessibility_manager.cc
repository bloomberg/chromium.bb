// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/accessibility/browser_accessibility_manager.h"

#include "base/logging.h"
#include "content/browser/accessibility/browser_accessibility.h"
#include "content/browser/accessibility/browser_accessibility_state_impl.h"
#include "content/common/accessibility_messages.h"

namespace content {

BrowserAccessibility* BrowserAccessibilityFactory::Create() {
  return BrowserAccessibility::Create();
}

// Start child IDs at -1 and decrement each time, because clients use
// child IDs of 1, 2, 3, ... to access the children of an object by
// index, so we use negative IDs to clearly distinguish between indices
// and unique IDs.
// static
int32 BrowserAccessibilityManager::next_child_id_ = -1;

#if !defined(OS_MACOSX) && \
    !(defined(OS_WIN) && !defined(USE_AURA)) && \
    !defined(TOOLKIT_GTK)
// We have subclassess of BrowserAccessibilityManager on Mac, Linux/GTK,
// and non-Aura Win. For any other platform, instantiate the base class.
// static
BrowserAccessibilityManager* BrowserAccessibilityManager::Create(
    gfx::NativeView parent_view,
    const AccessibilityNodeData& src,
    BrowserAccessibilityDelegate* delegate,
    BrowserAccessibilityFactory* factory) {
  return new BrowserAccessibilityManager(
      parent_view, src, delegate, factory);
}
#endif

// static
BrowserAccessibilityManager* BrowserAccessibilityManager::CreateEmptyDocument(
    gfx::NativeView parent_view,
    AccessibilityNodeData::State state,
    BrowserAccessibilityDelegate* delegate,
    BrowserAccessibilityFactory* factory) {
  // Use empty document to process notifications
  AccessibilityNodeData empty_document;
  empty_document.id = 0;
  empty_document.role = AccessibilityNodeData::ROLE_ROOT_WEB_AREA;
  empty_document.state = state | (1 << AccessibilityNodeData::STATE_READONLY);
  return BrowserAccessibilityManager::Create(
      parent_view, empty_document, delegate, factory);
}

BrowserAccessibilityManager::BrowserAccessibilityManager(
    gfx::NativeView parent_view,
    const AccessibilityNodeData& src,
    BrowserAccessibilityDelegate* delegate,
    BrowserAccessibilityFactory* factory)
    : parent_view_(parent_view),
      delegate_(delegate),
      factory_(factory),
      focus_(NULL),
      osk_state_(OSK_ALLOWED) {
  root_ = CreateAccessibilityTree(NULL, src, 0, false);
  if (!focus_)
    SetFocus(root_, false);
}

// static
int32 BrowserAccessibilityManager::GetNextChildID() {
  // Get the next child ID, and wrap around when we get near the end
  // of a 32-bit integer range. It's okay to wrap around; we just want
  // to avoid it as long as possible because clients may cache the ID of
  // an object for a while to determine if they've seen it before.
  next_child_id_--;
  if (next_child_id_ == -2000000000)
    next_child_id_ = -1;

  return next_child_id_;
}

BrowserAccessibilityManager::~BrowserAccessibilityManager() {
  // Clients could still hold references to some nodes of the tree, so
  // calling InternalReleaseReference will make sure that as many nodes
  // as possible are released now, and remaining nodes are marked as
  // inactive so that calls to any methods on them will fail gracefully.
  focus_->InternalReleaseReference(false);
  root_->InternalReleaseReference(true);
}

BrowserAccessibility* BrowserAccessibilityManager::GetRoot() {
  return root_;
}

BrowserAccessibility* BrowserAccessibilityManager::GetFromChildID(
    int32 child_id) {
  base::hash_map<int32, BrowserAccessibility*>::iterator iter =
      child_id_map_.find(child_id);
  if (iter != child_id_map_.end()) {
    return iter->second;
  } else {
    return NULL;
  }
}

BrowserAccessibility* BrowserAccessibilityManager::GetFromRendererID(
    int32 renderer_id) {
  base::hash_map<int32, int32>::iterator iter =
      renderer_id_to_child_id_map_.find(renderer_id);
  if (iter == renderer_id_to_child_id_map_.end())
    return NULL;

  int32 child_id = iter->second;
  return GetFromChildID(child_id);
}

void BrowserAccessibilityManager::GotFocus(bool touch_event_context) {
  if (!touch_event_context)
    osk_state_ = OSK_DISALLOWED_BECAUSE_TAB_JUST_APPEARED;

  if (!focus_)
    return;

  NotifyAccessibilityEvent(AccessibilityNotificationFocusChanged, focus_);
}

void BrowserAccessibilityManager::WasHidden() {
  osk_state_ = OSK_DISALLOWED_BECAUSE_TAB_HIDDEN;
}

void BrowserAccessibilityManager::GotMouseDown() {
  osk_state_ = OSK_ALLOWED_WITHIN_FOCUSED_OBJECT;
  NotifyAccessibilityEvent(AccessibilityNotificationFocusChanged, focus_);
}

bool BrowserAccessibilityManager::IsOSKAllowed(const gfx::Rect& bounds) {
  if (!delegate_ || !delegate_->HasFocus())
    return false;

  gfx::Point touch_point = delegate_->GetLastTouchEventLocation();
  return bounds.Contains(touch_point);
}

void BrowserAccessibilityManager::Remove(int32 child_id, int32 renderer_id) {
  child_id_map_.erase(child_id);

  // TODO(ctguil): Investigate if hit. We should never have a newer entry.
  DCHECK(renderer_id_to_child_id_map_[renderer_id] == child_id);
  // Make sure we don't overwrite a newer entry (see UpdateNode for a possible
  // corner case).
  if (renderer_id_to_child_id_map_[renderer_id] == child_id)
    renderer_id_to_child_id_map_.erase(renderer_id);
}

void BrowserAccessibilityManager::OnAccessibilityNotifications(
    const std::vector<AccessibilityHostMsg_NotificationParams>& params) {
  for (uint32 index = 0; index < params.size(); index++) {
    const AccessibilityHostMsg_NotificationParams& param = params[index];

    // Update the tree.
    UpdateNode(param.acc_tree, param.includes_children);

    // Find the node corresponding to the id that's the target of the
    // notification (which may not be the root of the update tree).
    base::hash_map<int32, int32>::iterator iter =
        renderer_id_to_child_id_map_.find(param.id);
    if (iter == renderer_id_to_child_id_map_.end()) {
      continue;
    }
    int32 child_id = iter->second;
    BrowserAccessibility* node = GetFromChildID(child_id);
    if (!node) {
      NOTREACHED();
      continue;
    }

    int notification_type = param.notification_type;
    if (notification_type == AccessibilityNotificationFocusChanged ||
        notification_type == AccessibilityNotificationBlur) {
      SetFocus(node, false);

      if (osk_state_ != OSK_DISALLOWED_BECAUSE_TAB_HIDDEN &&
          osk_state_ != OSK_DISALLOWED_BECAUSE_TAB_JUST_APPEARED)
        osk_state_ = OSK_ALLOWED;

      // Don't send a native focus event if the window itself doesn't
      // have focus.
      if (delegate_ && !delegate_->HasFocus())
        continue;
    }

    // Send the notification event to the operating system.
    NotifyAccessibilityEvent(notification_type, node);

    // Set initial focus when a page is loaded.
    if (notification_type == AccessibilityNotificationLoadComplete) {
      if (!focus_)
        SetFocus(root_, false);
      if (!delegate_ || delegate_->HasFocus())
        NotifyAccessibilityEvent(AccessibilityNotificationFocusChanged, focus_);
    }
  }
}

gfx::NativeView BrowserAccessibilityManager::GetParentView() {
  return parent_view_;
}

BrowserAccessibility* BrowserAccessibilityManager::GetFocus(
    BrowserAccessibility* root) {
  if (focus_ && (!root || focus_->IsDescendantOf(root)))
    return focus_;

  return NULL;
}

void BrowserAccessibilityManager::SetFocus(
    BrowserAccessibility* node, bool notify) {
  if (focus_ != node) {
    if (focus_)
      focus_->InternalReleaseReference(false);
    focus_ = node;
    if (focus_)
      focus_->InternalAddReference();
  }

  if (notify && node && delegate_)
    delegate_->SetAccessibilityFocus(node->renderer_id());
}

void BrowserAccessibilityManager::DoDefaultAction(
    const BrowserAccessibility& node) {
  if (delegate_)
    delegate_->AccessibilityDoDefaultAction(node.renderer_id());
}

void BrowserAccessibilityManager::ScrollToMakeVisible(
    const BrowserAccessibility& node, gfx::Rect subfocus) {
  if (delegate_) {
    delegate_->AccessibilityScrollToMakeVisible(node.renderer_id(), subfocus);
  }
}

void BrowserAccessibilityManager::ScrollToPoint(
    const BrowserAccessibility& node, gfx::Point point) {
  if (delegate_) {
    delegate_->AccessibilityScrollToPoint(node.renderer_id(), point);
  }
}

void BrowserAccessibilityManager::SetTextSelection(
    const BrowserAccessibility& node, int start_offset, int end_offset) {
  if (delegate_) {
    delegate_->AccessibilitySetTextSelection(
        node.renderer_id(), start_offset, end_offset);
  }
}

gfx::Rect BrowserAccessibilityManager::GetViewBounds() {
  if (delegate_)
    return delegate_->GetViewBounds();
  return gfx::Rect();
}

void BrowserAccessibilityManager::UpdateNode(
    const AccessibilityNodeData& src,
    bool include_children) {
  BrowserAccessibility* current = NULL;

  // Look for the node to replace. Either we're replacing the whole tree
  // (role is ROOT_WEB_AREA) or we look it up based on its renderer ID.
  if (src.role == AccessibilityNodeData::ROLE_ROOT_WEB_AREA) {
    current = root_;
  } else {
    base::hash_map<int32, int32>::iterator iter =
        renderer_id_to_child_id_map_.find(src.id);
    if (iter != renderer_id_to_child_id_map_.end()) {
      int32 child_id = iter->second;
      current = GetFromChildID(child_id);
    }
  }

  // If we can't find the node to replace, we're out of sync with the
  // renderer (this would be a bug).
  DCHECK(current);
  if (!current)
    return;

  // If this update is just for a single node (|include_children| is false),
  // modify |current| directly and return - no tree changes are needed.
  if (!include_children) {
    DCHECK_EQ(0U, src.children.size());
    current->PreInitialize(
        this,
        current->parent(),
        current->child_id(),
        current->index_in_parent(),
        src);
    current->PostInitialize();
    return;
  }

  BrowserAccessibility* current_parent = current->parent();
  int current_index_in_parent = current->index_in_parent();

  // Detach all of the nodes in the old tree and get a single flat vector
  // of all node pointers.
  std::vector<BrowserAccessibility*> old_tree_nodes;
  current->DetachTree(&old_tree_nodes);

  // Build a new tree, reusing old nodes if possible. Each node that's
  // reused will have its reference count incremented by one.
  CreateAccessibilityTree(current_parent, src, current_index_in_parent, true);

  // Decrement the reference count of all nodes in the old tree, which will
  // delete any nodes no longer needed.
  for (int i = 0; i < static_cast<int>(old_tree_nodes.size()); i++)
    old_tree_nodes[i]->InternalReleaseReference(false);

  // If the only reference to the focused node is focus_ itself, then the
  // focused node is no longer in the tree, so set the focus to the root.
  if (focus_ && focus_->ref_count() == 1) {
    SetFocus(root_, false);

    if (delegate_ && delegate_->HasFocus())
      NotifyAccessibilityEvent(AccessibilityNotificationBlur, focus_);
  }
}

BrowserAccessibility* BrowserAccessibilityManager::CreateAccessibilityTree(
    BrowserAccessibility* parent,
    const AccessibilityNodeData& src,
    int index_in_parent,
    bool send_show_events) {
  BrowserAccessibility* instance = NULL;
  int32 child_id = 0;
  bool children_can_send_show_events = send_show_events;
  base::hash_map<int32, int32>::iterator iter =
      renderer_id_to_child_id_map_.find(src.id);

  // If a BrowserAccessibility instance for this ID already exists, add a
  // new reference to it and retrieve its children vector.
  if (iter != renderer_id_to_child_id_map_.end()) {
    child_id = iter->second;
    instance = GetFromChildID(child_id);
  }

  // If the node has changed roles, don't reuse a BrowserAccessibility
  // object, that could confuse a screen reader.
  // TODO(dtseng): Investigate when this gets hit; See crbug.com/93095.
  DCHECK(!instance || instance->role() == src.role);

  // If we're reusing a node, it should already be detached from a parent
  // and any children. If not, that means we have a serious bug somewhere,
  // like the same child is reachable from two places in the same tree.
  if (instance && (instance->parent() != NULL || instance->child_count() > 0)) {
    // TODO(dmazzoni): investigate this: http://crbug.com/161726
    LOG(WARNING) << "Reusing node that wasn't detached from parent";
    instance = NULL;
  }

  if (instance) {
    // If we're reusing a node, update its parent and increment its
    // reference count.
    instance->UpdateParent(parent, index_in_parent);
    instance->InternalAddReference();
    send_show_events = false;
  } else {
    // Otherwise, create a new instance.
    instance = factory_->Create();
    child_id = GetNextChildID();
    children_can_send_show_events = false;
  }

  instance->PreInitialize(this, parent, child_id, index_in_parent, src);
  child_id_map_[child_id] = instance;
  renderer_id_to_child_id_map_[src.id] = child_id;

  if ((src.state >> AccessibilityNodeData::STATE_FOCUSED) & 1)
    SetFocus(instance, false);

  for (int i = 0; i < static_cast<int>(src.children.size()); ++i) {
    BrowserAccessibility* child = CreateAccessibilityTree(
        instance, src.children[i], i, children_can_send_show_events);
    instance->AddChild(child);
  }

  if (src.role == AccessibilityNodeData::ROLE_ROOT_WEB_AREA)
    root_ = instance;

  // Note: the purpose of send_show_events and children_can_send_show_events
  // is so that we send a single ObjectShow event for the root of a subtree
  // that just appeared for the first time, but not on any descendant of
  // that subtree.
  if (send_show_events)
    NotifyAccessibilityEvent(AccessibilityNotificationObjectShow, instance);

  instance->PostInitialize();

  return instance;
}

}  // namespace content
