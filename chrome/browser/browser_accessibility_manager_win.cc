// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser_accessibility_manager_win.h"

#include "chrome/browser/browser_accessibility_win.h"
#include "chrome/browser/renderer_host/render_process_host.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/render_messages_params.h"

using webkit_glue::WebAccessibility;

// Factory method to create an instance of BrowserAccessibility
BrowserAccessibility* BrowserAccessibilityFactory::Create() {
  CComObject<BrowserAccessibility>* instance;
  HRESULT hr = CComObject<BrowserAccessibility>::CreateInstance(&instance);
  DCHECK(SUCCEEDED(hr));
  return instance->NewReference();
}

// static
// Start child IDs at -1 and decrement each time, because clients use
// child IDs of 1, 2, 3, ... to access the children of an object by
// index, so we use negative IDs to clearly distinguish between indices
// and unique IDs.
LONG BrowserAccessibilityManager::next_child_id_ = -1;

BrowserAccessibilityManager::BrowserAccessibilityManager(
    HWND parent_hwnd,
    const webkit_glue::WebAccessibility& src,
    BrowserAccessibilityDelegate* delegate,
    BrowserAccessibilityFactory* factory)
    : parent_hwnd_(parent_hwnd),
      delegate_(delegate),
      factory_(factory),
      focus_(NULL) {
  HRESULT hr = ::CreateStdAccessibleObject(
      parent_hwnd_, OBJID_WINDOW, IID_IAccessible,
      reinterpret_cast<void **>(&window_iaccessible_));
  DCHECK(SUCCEEDED(hr));
  root_ = CreateAccessibilityTree(NULL, GetNextChildID(), src, 0);
  if (!focus_)
    focus_ = root_;
}

BrowserAccessibilityManager::~BrowserAccessibilityManager() {
  // Clients could still hold references to some nodes of the tree, so
  // calling Inactivate will make sure that as many nodes as possible are
  // released now, and remaining nodes are marked as inactive so that
  // calls to any methods on them will return E_FAIL;
  root_->InactivateTree();
  root_->Release();
}

BrowserAccessibility* BrowserAccessibilityManager::GetRoot() {
  return root_;
}

void BrowserAccessibilityManager::Remove(LONG child_id) {
  child_id_map_.erase(child_id);
}

BrowserAccessibility* BrowserAccessibilityManager::GetFromChildID(
    LONG child_id) {
  base::hash_map<LONG, BrowserAccessibility*>::iterator iter =
      child_id_map_.find(child_id);
  if (iter != child_id_map_.end()) {
    return iter->second;
  } else {
    return NULL;
  }
}

IAccessible* BrowserAccessibilityManager::GetParentWindowIAccessible() {
  return window_iaccessible_;
}

HWND BrowserAccessibilityManager::GetParentHWND() {
  return parent_hwnd_;
}

BrowserAccessibility* BrowserAccessibilityManager::GetFocus(
    BrowserAccessibility* root) {
  if (focus_ && (!root || focus_->IsDescendantOf(root)))
    return focus_;

  return NULL;
}

void BrowserAccessibilityManager::SetFocus(const BrowserAccessibility& node) {
  if (delegate_)
    delegate_->SetAccessibilityFocus(node.renderer_id());
}

void BrowserAccessibilityManager::DoDefaultAction(
    const BrowserAccessibility& node) {
  if (delegate_)
    delegate_->AccessibilityDoDefaultAction(node.renderer_id());
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
      default:
        DCHECK(0);
        break;
    }
  }
}

BrowserAccessibility* BrowserAccessibilityManager::UpdateTree(
    const webkit_glue::WebAccessibility& acc_obj) {
  base::hash_map<int, LONG>::iterator iter =
      renderer_id_to_child_id_map_.find(acc_obj.id);
  if (iter == renderer_id_to_child_id_map_.end())
    return NULL;

  LONG child_id = iter->second;
  BrowserAccessibility* old_browser_acc = GetFromChildID(child_id);
  if (!old_browser_acc)
    return NULL;

  if (old_browser_acc->GetChildCount() == 0 && acc_obj.children.size() == 0) {
    // Reinitialize the BrowserAccessibility if there are no children to update.
    old_browser_acc->Initialize(
        this,
        old_browser_acc->GetParent(),
        child_id,
        old_browser_acc->index_in_parent(),
        acc_obj);

    return old_browser_acc;
  } else {
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
    old_browser_acc->InactivateTree();
    old_browser_acc->Release();
    child_id_map_[child_id] = new_browser_acc;

    return new_browser_acc;
  }
}

void BrowserAccessibilityManager::OnAccessibilityObjectStateChange(
    const webkit_glue::WebAccessibility& acc_obj) {
  BrowserAccessibility* new_browser_acc = UpdateTree(acc_obj);
  if (!new_browser_acc)
    return;

  LONG child_id = new_browser_acc->child_id();
  NotifyWinEvent(
      EVENT_OBJECT_STATECHANGE, parent_hwnd_, OBJID_CLIENT, child_id);
}

void BrowserAccessibilityManager::OnAccessibilityObjectChildrenChange(
    const webkit_glue::WebAccessibility& acc_obj) {
  BrowserAccessibility* new_browser_acc = UpdateTree(acc_obj);
  if (!new_browser_acc)
    return;

  LONG child_id;
  if (root_ != new_browser_acc) {
    child_id = new_browser_acc->GetParent()->child_id();
  } else {
    child_id = CHILDID_SELF;
  }

  NotifyWinEvent(EVENT_OBJECT_REORDER, parent_hwnd_, OBJID_CLIENT, child_id);
}

void BrowserAccessibilityManager::OnAccessibilityObjectFocusChange(
  const webkit_glue::WebAccessibility& acc_obj) {
  BrowserAccessibility* new_browser_acc = UpdateTree(acc_obj);
  if (!new_browser_acc)
    return;

  focus_ = new_browser_acc;
  LONG child_id = new_browser_acc->child_id();
  NotifyWinEvent(EVENT_OBJECT_FOCUS, parent_hwnd_, OBJID_CLIENT, child_id);
}

void BrowserAccessibilityManager::OnAccessibilityObjectLoadComplete(
  const webkit_glue::WebAccessibility& acc_obj) {
  root_->InactivateTree();
  root_->Release();
  focus_ = NULL;

  root_ = CreateAccessibilityTree(NULL, GetNextChildID(), acc_obj, 0);
  if (!focus_)
    focus_ = root_;

  LONG root_id = root_->child_id();
  NotifyWinEvent(EVENT_OBJECT_FOCUS, parent_hwnd_, OBJID_CLIENT, root_id);
  NotifyWinEvent(
    IA2_EVENT_DOCUMENT_LOAD_COMPLETE, parent_hwnd_, OBJID_CLIENT, root_id);
}

void BrowserAccessibilityManager::OnAccessibilityObjectValueChange(
    const webkit_glue::WebAccessibility& acc_obj) {
  BrowserAccessibility* new_browser_acc = UpdateTree(acc_obj);
  if (!new_browser_acc)
    return;

  LONG child_id = new_browser_acc->child_id();
  NotifyWinEvent(
      EVENT_OBJECT_VALUECHANGE, parent_hwnd_, OBJID_CLIENT, child_id);
}

LONG BrowserAccessibilityManager::GetNextChildID() {
  // Get the next child ID, and wrap around when we get near the end
  // of a 32-bit integer range. It's okay to wrap around; we just want
  // to avoid it as long as possible because clients may cache the ID of
  // an object for a while to determine if they've seen it before.
  next_child_id_--;
  if (next_child_id_ == -2000000000)
    next_child_id_ = -1;

  return next_child_id_;
}

BrowserAccessibility* BrowserAccessibilityManager::CreateAccessibilityTree(
    BrowserAccessibility* parent,
    int child_id,
    const webkit_glue::WebAccessibility& src,
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
