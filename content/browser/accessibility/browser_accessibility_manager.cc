// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/accessibility/browser_accessibility_manager.h"

#include "base/logging.h"
#include "content/browser/accessibility/browser_accessibility.h"
#include "content/common/view_messages.h"

using webkit_glue::WebAccessibility;

BrowserAccessibility* BrowserAccessibilityFactory::Create() {
  return BrowserAccessibility::Create();
}

// Start child IDs at -1 and decrement each time, because clients use
// child IDs of 1, 2, 3, ... to access the children of an object by
// index, so we use negative IDs to clearly distinguish between indices
// and unique IDs.
// static
int32 BrowserAccessibilityManager::next_child_id_ = -1;

#if defined(OS_POSIX) && !defined(OS_MACOSX)
// There's no OS-specific implementation of BrowserAccessibilityManager
// on Unix, so just instantiate the base class.
// static
BrowserAccessibilityManager* BrowserAccessibilityManager::Create(
    gfx::NativeView parent_view,
    const WebAccessibility& src,
    BrowserAccessibilityDelegate* delegate,
    BrowserAccessibilityFactory* factory) {
  return new BrowserAccessibilityManager(
      parent_view, src, delegate, factory);
}
#endif

// static
BrowserAccessibilityManager* BrowserAccessibilityManager::CreateEmptyDocument(
    gfx::NativeView parent_view,
    WebAccessibility::State state,
    BrowserAccessibilityDelegate* delegate,
    BrowserAccessibilityFactory* factory) {
  // Use empty document to process notifications
  webkit_glue::WebAccessibility empty_document;
  // Renderer id's always start at 1000 as determined by webkit. Boot strap
  // our ability to reuse BrowserAccessibility instances.
  empty_document.id = 1000;
  empty_document.role = WebAccessibility::ROLE_WEB_AREA;
  empty_document.state = state;
  return BrowserAccessibilityManager::Create(
      parent_view, empty_document, delegate, factory);
}

BrowserAccessibilityManager::BrowserAccessibilityManager(
    gfx::NativeView parent_view,
    const WebAccessibility& src,
    BrowserAccessibilityDelegate* delegate,
    BrowserAccessibilityFactory* factory)
    : parent_view_(parent_view),
      delegate_(delegate),
      factory_(factory),
      focus_(NULL) {
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
    const std::vector<ViewHostMsg_AccessibilityNotification_Params>& params) {
  for (uint32 index = 0; index < params.size(); index++) {
    const ViewHostMsg_AccessibilityNotification_Params& param = params[index];

    switch (param.notification_type) {
      // Notifications where children are included.
      case ViewHostMsg_AccEvent::CHILDREN_CHANGED:
      case ViewHostMsg_AccEvent::LIVE_REGION_CHANGED:
        OnSimpleAccessibilityNotification(
            param.acc_obj, param.notification_type, true);
        break;

      case ViewHostMsg_AccEvent::FOCUS_CHANGED:
        OnAccessibilityObjectFocusChange(param.acc_obj);
        break;

      case ViewHostMsg_AccEvent::LOAD_COMPLETE:
        OnAccessibilityObjectLoadComplete(param.acc_obj);
        break;

      // All other notifications: the node is updated, but
      // children are not included.
      case ViewHostMsg_AccEvent::ACTIVE_DESCENDANT_CHANGED:
      case ViewHostMsg_AccEvent::ALERT:
      case ViewHostMsg_AccEvent::CHECK_STATE_CHANGED:
      case ViewHostMsg_AccEvent::LAYOUT_COMPLETE:
      case ViewHostMsg_AccEvent::MENU_LIST_VALUE_CHANGED:
      case ViewHostMsg_AccEvent::OBJECT_HIDE:
      case ViewHostMsg_AccEvent::OBJECT_SHOW:
      case ViewHostMsg_AccEvent::ROW_COLLAPSED:
      case ViewHostMsg_AccEvent::ROW_COUNT_CHANGED:
      case ViewHostMsg_AccEvent::ROW_EXPANDED:
      case ViewHostMsg_AccEvent::SCROLLED_TO_ANCHOR:
      case ViewHostMsg_AccEvent::SELECTED_CHILDREN_CHANGED:
      case ViewHostMsg_AccEvent::SELECTED_TEXT_CHANGED:
      case ViewHostMsg_AccEvent::TEXT_INSERTED:
      case ViewHostMsg_AccEvent::TEXT_REMOVED:
      case ViewHostMsg_AccEvent::VALUE_CHANGED:
        OnSimpleAccessibilityNotification(
            param.acc_obj, param.notification_type, false);
        break;

      default:
        DCHECK(0);
    }
  }
}

void BrowserAccessibilityManager::OnSimpleAccessibilityNotification(
    const WebAccessibility& acc_obj,
    int notification_type,
    bool include_children) {
  BrowserAccessibility* new_browser_acc =
      UpdateNode(acc_obj, include_children);
  if (!new_browser_acc)
    return;

  NotifyAccessibilityEvent(notification_type, new_browser_acc);
}

void BrowserAccessibilityManager::OnAccessibilityObjectFocusChange(
  const WebAccessibility& acc_obj) {
  BrowserAccessibility* new_browser_acc = UpdateNode(acc_obj, false);
  if (!new_browser_acc)
    return;

  SetFocus(new_browser_acc, false);
  if (delegate_ && delegate_->HasFocus()) {
    GotFocus();
  } else if (!delegate_) {
    // Mac currently does not have a BrowserAccessibilityDelegate.
    NotifyAccessibilityEvent(ViewHostMsg_AccEvent::FOCUS_CHANGED, focus_);
  }
}

void BrowserAccessibilityManager::OnAccessibilityObjectLoadComplete(
  const WebAccessibility& acc_obj) {
  SetFocus(NULL, false);

  root_ = UpdateNode(acc_obj, true);
  if (!focus_)
    SetFocus(root_, false);

  NotifyAccessibilityEvent(ViewHostMsg_AccEvent::LOAD_COMPLETE, root_);
  if (delegate_ && delegate_->HasFocus())
    GotFocus();
}

void BrowserAccessibilityManager::GotFocus() {
  // TODO(ctguil): Remove when tree update logic handles focus changes.
  if (!focus_)
    return;

  NotifyAccessibilityEvent(ViewHostMsg_AccEvent::FOCUS_CHANGED, focus_);
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

gfx::Rect BrowserAccessibilityManager::GetViewBounds() {
  if (delegate_)
    return delegate_->GetViewBounds();
  return gfx::Rect();
}

BrowserAccessibility* BrowserAccessibilityManager::UpdateNode(
    const WebAccessibility& src,
    bool include_children) {
  base::hash_map<int32, int32>::iterator iter =
      renderer_id_to_child_id_map_.find(src.id);
  if (iter == renderer_id_to_child_id_map_.end())
    return NULL;

  int32 child_id = iter->second;
  BrowserAccessibility* current = GetFromChildID(child_id);
  if (!current)
    return NULL;

  if (!include_children) {
    DCHECK_EQ(0U, src.children.size());
    current->Initialize(
        this,
        current->parent(),
        current->child_id(),
        current->index_in_parent(),
        src);
    current->SendNodeUpdateEvents();
    return current;
  }

  BrowserAccessibility* current_parent = current->parent();
  int current_index_in_parent = current->index_in_parent();

  // Detach all of the nodes in the old tree and get a single flat vector
  // of all node pointers.
  std::vector<BrowserAccessibility*> old_tree_nodes;
  current->DetachTree(&old_tree_nodes);

  // Build a new tree, reusing old nodes if possible. Each node that's
  // reused will have its reference count incremented by one.
  current = CreateAccessibilityTree(
      current_parent, src, current_index_in_parent, true);

  // Decrement the reference count of all nodes in the old tree, which will
  // delete any nodes no longer needed.
  for (int i = 0; i < static_cast<int>(old_tree_nodes.size()); i++)
    old_tree_nodes[i]->InternalReleaseReference(false);

  if (!focus_ || !focus_->instance_active())
    SetFocus(root_, false);

  return current;
}

BrowserAccessibility* BrowserAccessibilityManager::CreateAccessibilityTree(
    BrowserAccessibility* parent,
    const WebAccessibility& src,
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
    NOTREACHED();
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

  instance->Initialize(this, parent, child_id, index_in_parent, src);
  child_id_map_[child_id] = instance;
  renderer_id_to_child_id_map_[src.id] = child_id;
  if ((src.state >> WebAccessibility::STATE_FOCUSED) & 1)
    SetFocus(instance, false);
  for (int i = 0; i < static_cast<int>(src.children.size()); ++i) {
    BrowserAccessibility* child = CreateAccessibilityTree(
        instance, src.children[i], i, children_can_send_show_events);
    instance->AddChild(child);
  }

  // Note: the purpose of send_show_events and children_can_send_show_events
  // is so that we send a single OBJECT_SHOW event for the root of a subtree
  // that just appeared for the first time, but not on any descendant of
  // that subtree.
  if (send_show_events)
    NotifyAccessibilityEvent(ViewHostMsg_AccEvent::OBJECT_SHOW, instance);

  instance->SendNodeUpdateEvents();

  return instance;
}
