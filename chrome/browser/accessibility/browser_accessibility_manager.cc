// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/accessibility/browser_accessibility_manager.h"

#include "base/logging.h"
#include "chrome/browser/accessibility/browser_accessibility.h"
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

#if defined(OS_LINUX)
// There's no OS-specific implementation of BrowserAccessibilityManager
// on Linux, so just instantiate the base class.
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

BrowserAccessibilityManager::BrowserAccessibilityManager(
    gfx::NativeView parent_view,
    const WebAccessibility& src,
    BrowserAccessibilityDelegate* delegate,
    BrowserAccessibilityFactory* factory)
    : parent_view_(parent_view),
      delegate_(delegate),
      factory_(factory),
      focus_(NULL) {
  root_ = CreateAccessibilityTree(NULL, src, 0);
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

void BrowserAccessibilityManager::Remove(int32 child_id, int32 renderer_id) {
  child_id_map_.erase(child_id);
  renderer_id_to_child_id_map_.erase(renderer_id);
}

void BrowserAccessibilityManager::OnAccessibilityNotifications(
    const std::vector<ViewHostMsg_AccessibilityNotification_Params>& params) {
  for (uint32 index = 0; index < params.size(); index++) {
    const ViewHostMsg_AccessibilityNotification_Params& param = params[index];

    switch (param.notification_type) {
      case ViewHostMsg_AccessibilityNotification_Type::
          NOTIFICATION_TYPE_CHECK_STATE_CHANGED:
        OnAccessibilityObjectStateChange(param.acc_obj);
        break;
      case ViewHostMsg_AccessibilityNotification_Type::
          NOTIFICATION_TYPE_CHILDREN_CHANGED:
        OnAccessibilityObjectChildrenChange(param.acc_obj);
        break;
      case ViewHostMsg_AccessibilityNotification_Type::
          NOTIFICATION_TYPE_FOCUS_CHANGED:
        OnAccessibilityObjectFocusChange(param.acc_obj);
        break;
      case ViewHostMsg_AccessibilityNotification_Type::
          NOTIFICATION_TYPE_LOAD_COMPLETE:
        OnAccessibilityObjectLoadComplete(param.acc_obj);
        break;
      case ViewHostMsg_AccessibilityNotification_Type::
          NOTIFICATION_TYPE_VALUE_CHANGED:
        OnAccessibilityObjectValueChange(param.acc_obj);
        break;
      case ViewHostMsg_AccessibilityNotification_Type::
          NOTIFICATION_TYPE_SELECTED_TEXT_CHANGED:
        OnAccessibilityObjectTextChange(param.acc_obj);
        break;
      default:
        DCHECK(0);
        break;
    }
  }
}

void BrowserAccessibilityManager::OnAccessibilityObjectStateChange(
    const WebAccessibility& acc_obj) {
  BrowserAccessibility* new_browser_acc = UpdateNode(acc_obj, false);
  if (!new_browser_acc)
    return;

  NotifyAccessibilityEvent(
      ViewHostMsg_AccessibilityNotification_Type::
          NOTIFICATION_TYPE_CHECK_STATE_CHANGED,
      new_browser_acc);
}

void BrowserAccessibilityManager::OnAccessibilityObjectChildrenChange(
    const WebAccessibility& acc_obj) {
  BrowserAccessibility* new_browser_acc = UpdateNode(acc_obj, true);
  if (!new_browser_acc)
    return;

  NotifyAccessibilityEvent(
      ViewHostMsg_AccessibilityNotification_Type::
          NOTIFICATION_TYPE_CHILDREN_CHANGED,
      new_browser_acc);
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
    NotifyAccessibilityEvent(
        ViewHostMsg_AccessibilityNotification_Type::
        NOTIFICATION_TYPE_FOCUS_CHANGED,
        focus_);
  }
}

void BrowserAccessibilityManager::OnAccessibilityObjectLoadComplete(
  const WebAccessibility& acc_obj) {
  SetFocus(NULL, false);
  root_->InternalReleaseReference(true);

  root_ = CreateAccessibilityTree(NULL, acc_obj, 0);
  if (!focus_)
    SetFocus(root_, false);

  NotifyAccessibilityEvent(
      ViewHostMsg_AccessibilityNotification_Type::
          NOTIFICATION_TYPE_LOAD_COMPLETE,
      root_);
  if (delegate_ && delegate_->HasFocus())
    GotFocus();
}

void BrowserAccessibilityManager::OnAccessibilityObjectValueChange(
    const WebAccessibility& acc_obj) {
  BrowserAccessibility* new_browser_acc = UpdateNode(acc_obj, false);
  if (!new_browser_acc)
    return;

  NotifyAccessibilityEvent(
      ViewHostMsg_AccessibilityNotification_Type::
          NOTIFICATION_TYPE_VALUE_CHANGED,
      new_browser_acc);
}

void BrowserAccessibilityManager::OnAccessibilityObjectTextChange(
    const WebAccessibility& acc_obj) {
  BrowserAccessibility* new_browser_acc = UpdateNode(acc_obj, false);
  if (!new_browser_acc)
    return;

  NotifyAccessibilityEvent(
      ViewHostMsg_AccessibilityNotification_Type::
          NOTIFICATION_TYPE_SELECTED_TEXT_CHANGED,
      new_browser_acc);
}

void BrowserAccessibilityManager::GotFocus() {
  // TODO(ctguil): Remove when tree update logic handles focus changes.
  if (!focus_)
    return;

  NotifyAccessibilityEvent(
      ViewHostMsg_AccessibilityNotification_Type::
          NOTIFICATION_TYPE_FOCUS_CHANGED,
      focus_);
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
  if (focus_)
    focus_->InternalReleaseReference(false);
  focus_ = node;
  if (focus_)
    focus_->InternalAddReference();

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
    return current;
  }

  // Detach all of the nodes in the old tree and get a single flat vector
  // of all node pointers.
  std::vector<BrowserAccessibility*> old_tree_nodes;
  current->DetachTree(&old_tree_nodes);

  // Build a new tree, reusing old nodes if possible. Each node that's
  // reused will have its reference count incremented by one.
  current = CreateAccessibilityTree(NULL, src, -1);

  // Decrement the reference count of all nodes in the old tree, which will
  // delete any nodes no longer needed.
  for (int i = 0; i < static_cast<int>(old_tree_nodes.size()); i++)
    old_tree_nodes[i]->InternalReleaseReference(false);

  DCHECK(focus_);
  if (!focus_->instance_active())
    SetFocus(root_, false);

  return current;
}

BrowserAccessibility* BrowserAccessibilityManager::CreateAccessibilityTree(
    BrowserAccessibility* parent,
    const WebAccessibility& src,
    int index_in_parent) {
  BrowserAccessibility* instance = NULL;
  int32 child_id = 0;
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
  if (instance && instance->role() != src.role)
    instance = NULL;

  if (instance) {
    // If we're reusing a node, it should already be detached from a parent
    // and any children. If not, that means we have a serious bug somewhere,
    // like the same child is reachable from two places in the same tree.
    DCHECK_EQ(static_cast<BrowserAccessibility*>(NULL), instance->parent());
    DCHECK_EQ(0U, instance->child_count());

    // If we're reusing a node, update its parent and increment its
    // reference count.
    instance->UpdateParent(parent, index_in_parent);
    instance->InternalAddReference();
  } else {
    // Otherwise, create a new instance.
    instance = factory_->Create();
    child_id = GetNextChildID();
  }

  instance->Initialize(this, parent, child_id, index_in_parent, src);
  child_id_map_[child_id] = instance;
  renderer_id_to_child_id_map_[src.id] = child_id;
  if ((src.state >> WebAccessibility::STATE_FOCUSED) & 1)
    SetFocus(instance, false);
  for (int i = 0; i < static_cast<int>(src.children.size()); ++i) {
    BrowserAccessibility* child = CreateAccessibilityTree(
        instance, src.children[i], i);
    instance->AddChild(child);
  }

  return instance;
}
