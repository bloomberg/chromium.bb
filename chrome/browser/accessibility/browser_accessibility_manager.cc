// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/accessibility/browser_accessibility_manager.h"

#include "base/logging.h"
#include "chrome/browser/accessibility/browser_accessibility.h"
#include "chrome/common/render_messages_params.h"

using webkit_glue::WebAccessibility;

// Start child IDs at -1 and decrement each time, because clients use
// child IDs of 1, 2, 3, ... to access the children of an object by
// index, so we use negative IDs to clearly distinguish between indices
// and unique IDs.
// static
int32 BrowserAccessibilityManager::next_child_id_ = -1;

BrowserAccessibilityManager::BrowserAccessibilityManager(
    gfx::NativeView parent_view,
      const WebAccessibility& src,
      BrowserAccessibilityDelegate* delegate,
      BrowserAccessibilityFactory* factory)
    : parent_view_(parent_view),
      delegate_(delegate),
      factory_(factory),
      focus_(NULL) {
  root_ = CreateAccessibilityTree(NULL, GetNextChildID(), src, 0);
  if (!focus_)
    focus_ = root_;
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
  // calling Inactivate will make sure that as many nodes as possible are
  // released now, and remaining nodes are marked as inactive so that
  // calls to any methods on them will return E_FAIL;
  root_->ReleaseTree();
  root_->ReleaseReference();
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

void BrowserAccessibilityManager::Remove(int32 child_id) {
  child_id_map_.erase(child_id);
}

void BrowserAccessibilityManager::OnAccessibilityNotifications(
    const std::vector<ViewHostMsg_AccessibilityNotification_Params>& params) {
  for (uint32 index = 0; index < params.size(); index++) {
    const ViewHostMsg_AccessibilityNotification_Params& param = params[index];

    switch (param.notification_type) {
      case ViewHostMsg_AccessibilityNotification_Params::
          NOTIFICATION_TYPE_CHECK_STATE_CHANGED:
        OnAccessibilityObjectStateChange(param.acc_obj);
        break;
      case ViewHostMsg_AccessibilityNotification_Params::
          NOTIFICATION_TYPE_CHILDREN_CHANGED:
        OnAccessibilityObjectChildrenChange(param.acc_obj);
        break;
      case ViewHostMsg_AccessibilityNotification_Params::
          NOTIFICATION_TYPE_FOCUS_CHANGED:
        OnAccessibilityObjectFocusChange(param.acc_obj);
        break;
      case ViewHostMsg_AccessibilityNotification_Params::
          NOTIFICATION_TYPE_LOAD_COMPLETE:
        OnAccessibilityObjectLoadComplete(param.acc_obj);
        break;
      case ViewHostMsg_AccessibilityNotification_Params::
          NOTIFICATION_TYPE_VALUE_CHANGED:
        OnAccessibilityObjectValueChange(param.acc_obj);
        break;
      case ViewHostMsg_AccessibilityNotification_Params::
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
      ViewHostMsg_AccessibilityNotification_Params::
          NOTIFICATION_TYPE_CHECK_STATE_CHANGED,
      new_browser_acc);
}

void BrowserAccessibilityManager::OnAccessibilityObjectChildrenChange(
    const WebAccessibility& acc_obj) {
  BrowserAccessibility* new_browser_acc = UpdateNode(acc_obj, true);
  if (!new_browser_acc)
    return;

  NotifyAccessibilityEvent(
      ViewHostMsg_AccessibilityNotification_Params::
          NOTIFICATION_TYPE_CHILDREN_CHANGED,
      new_browser_acc);
}

void BrowserAccessibilityManager::OnAccessibilityObjectFocusChange(
  const WebAccessibility& acc_obj) {
  BrowserAccessibility* new_browser_acc = UpdateNode(acc_obj, false);
  if (!new_browser_acc)
    return;

  focus_ = new_browser_acc;
  if (delegate_ && delegate_->HasFocus())
    GotFocus();
  // Mac currently does not have a BrowserAccessibilityDelegate.
  else if (!delegate_) {
    NotifyAccessibilityEvent(
        ViewHostMsg_AccessibilityNotification_Params::
            NOTIFICATION_TYPE_FOCUS_CHANGED,
        focus_);
  }
}

void BrowserAccessibilityManager::OnAccessibilityObjectLoadComplete(
  const WebAccessibility& acc_obj) {
  root_->ReleaseTree();
  root_->ReleaseReference();
  focus_ = NULL;

  root_ = CreateAccessibilityTree(NULL, GetNextChildID(), acc_obj, 0);
  if (!focus_)
    focus_ = root_;

  NotifyAccessibilityEvent(
      ViewHostMsg_AccessibilityNotification_Params::
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
      ViewHostMsg_AccessibilityNotification_Params::
          NOTIFICATION_TYPE_VALUE_CHANGED,
      new_browser_acc);
}

void BrowserAccessibilityManager::OnAccessibilityObjectTextChange(
    const WebAccessibility& acc_obj) {
  BrowserAccessibility* new_browser_acc = UpdateNode(acc_obj, false);
  if (!new_browser_acc)
    return;

  NotifyAccessibilityEvent(
      ViewHostMsg_AccessibilityNotification_Params::
          NOTIFICATION_TYPE_SELECTED_TEXT_CHANGED,
      new_browser_acc);
}

void BrowserAccessibilityManager::GotFocus() {
  // TODO(ctguil): Remove when tree update logic handles focus changes.
  if (!focus_)
    return;

  NotifyAccessibilityEvent(
      ViewHostMsg_AccessibilityNotification_Params::
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
    const BrowserAccessibility& node) {
  if (delegate_)
    delegate_->SetAccessibilityFocus(node.renderer_id());
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

bool BrowserAccessibilityManager::CanModifyTreeInPlace(
    BrowserAccessibility* current_root,
    const WebAccessibility& new_root) {
  if (current_root->renderer_id() != new_root.id)
    return false;
  if (current_root->GetChildCount() != new_root.children.size())
    return false;
  for (unsigned int i = 0; i < current_root->GetChildCount(); i++) {
    if (!CanModifyTreeInPlace(current_root->GetChild(i),
                              new_root.children[i])) {
      return false;
    }
  }
  return true;
}

void BrowserAccessibilityManager::ModifyTreeInPlace(
      BrowserAccessibility* current_root,
      const WebAccessibility& new_root) {
  DCHECK_EQ(current_root->renderer_id(), new_root.id);
  DCHECK_EQ(current_root->GetChildCount(), new_root.children.size());
  for (unsigned int i = 0; i < current_root->GetChildCount(); i++)
    ModifyTreeInPlace(current_root->GetChild(i), new_root.children[i]);
  current_root->Initialize(
      this,
      current_root->GetParent(),
      current_root->child_id(),
      current_root->index_in_parent(),
      new_root);
}

BrowserAccessibility* BrowserAccessibilityManager::UpdateNode(
    const WebAccessibility& acc_obj,
    bool include_children) {
  base::hash_map<int32, int32>::iterator iter =
      renderer_id_to_child_id_map_.find(acc_obj.id);
  if (iter == renderer_id_to_child_id_map_.end())
    return NULL;

  int32 child_id = iter->second;
  BrowserAccessibility* old_browser_acc = GetFromChildID(child_id);
  if (!old_browser_acc)
    return NULL;

  if (!include_children) {
    DCHECK_EQ(0U, acc_obj.children.size());
    old_browser_acc->Initialize(
        this,
        old_browser_acc->GetParent(),
        old_browser_acc->child_id(),
        old_browser_acc->index_in_parent(),
        acc_obj);
    return old_browser_acc;
  }

  if (CanModifyTreeInPlace(old_browser_acc, acc_obj)) {
    ModifyTreeInPlace(old_browser_acc, acc_obj);
    return old_browser_acc;
  }

  BrowserAccessibility* new_browser_acc = CreateAccessibilityTree(
      old_browser_acc->GetParent(),
      child_id,
      acc_obj,
      old_browser_acc->index_in_parent());

  if (old_browser_acc->GetParent()) {
    old_browser_acc->GetParent()->ReplaceChild(
        old_browser_acc,
        new_browser_acc);
  } else {
    DCHECK_EQ(old_browser_acc, root_);
    root_ = new_browser_acc;
  }
  if (focus_ && focus_->IsDescendantOf(old_browser_acc))
    focus_ = root_;

  old_browser_acc->ReleaseTree();
  old_browser_acc->ReleaseReference();
  child_id_map_[child_id] = new_browser_acc;

  return new_browser_acc;
}

BrowserAccessibility* BrowserAccessibilityManager::CreateAccessibilityTree(
    BrowserAccessibility* parent,
    int child_id,
    const WebAccessibility& src,
    int index_in_parent) {
  BrowserAccessibility* instance = factory_->Create();

  instance->Initialize(this, parent, child_id, index_in_parent, src);
  child_id_map_[child_id] = instance;
  renderer_id_to_child_id_map_[src.id] = child_id;
  if ((src.state >> WebAccessibility::STATE_FOCUSED) & 1)
    focus_ = instance;
  for (int i = 0; i < static_cast<int>(src.children.size()); ++i) {
    BrowserAccessibility* child = CreateAccessibilityTree(
        instance, GetNextChildID(), src.children[i], i);
    instance->AddChild(child);
  }

  return instance;
}
