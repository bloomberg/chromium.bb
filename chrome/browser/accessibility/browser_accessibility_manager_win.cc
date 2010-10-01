// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/accessibility/browser_accessibility_manager_win.h"

#include "chrome/browser/accessibility/browser_accessibility_win.h"
#include "chrome/browser/renderer_host/render_process_host.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/render_messages_params.h"

using webkit_glue::WebAccessibility;

// static
BrowserAccessibilityManager* BrowserAccessibilityManager::Create(
    gfx::NativeWindow parent_window,
    const webkit_glue::WebAccessibility& src,
    BrowserAccessibilityDelegate* delegate) {
  return new BrowserAccessibilityManagerWin(
      parent_window, src, delegate, new BrowserAccessibilityWinFactory());
}

// Factory method to create an instance of BrowserAccessibility
BrowserAccessibilityWin* BrowserAccessibilityWinFactory::Create() {
  CComObject<BrowserAccessibilityWin>* instance;
  HRESULT hr = CComObject<BrowserAccessibilityWin>::CreateInstance(&instance);
  DCHECK(SUCCEEDED(hr));
  return instance->NewReference();
}

// static
// Start child IDs at -1 and decrement each time, because clients use
// child IDs of 1, 2, 3, ... to access the children of an object by
// index, so we use negative IDs to clearly distinguish between indices
// and unique IDs.
LONG BrowserAccessibilityManagerWin::next_child_id_ = -1;

BrowserAccessibilityManagerWin::BrowserAccessibilityManagerWin(
    HWND parent_window,
    const webkit_glue::WebAccessibility& src,
    BrowserAccessibilityDelegate* delegate,
    BrowserAccessibilityWinFactory* factory)
    : BrowserAccessibilityManager(parent_window),
      delegate_(delegate),
      factory_(factory),
      focus_(NULL) {
  HRESULT hr = ::CreateStdAccessibleObject(
      parent_window, OBJID_WINDOW, IID_IAccessible,
      reinterpret_cast<void **>(&window_iaccessible_));
  DCHECK(SUCCEEDED(hr));
  root_ = CreateAccessibilityTree(NULL, GetNextChildID(), src, 0);
  if (!focus_)
    focus_ = root_;
}

BrowserAccessibilityManagerWin::~BrowserAccessibilityManagerWin() {
  // Clients could still hold references to some nodes of the tree, so
  // calling Inactivate will make sure that as many nodes as possible are
  // released now, and remaining nodes are marked as inactive so that
  // calls to any methods on them will return E_FAIL;
  root_->InactivateTree();
  root_->Release();
}

BrowserAccessibilityWin* BrowserAccessibilityManagerWin::GetRoot() {
  return root_;
}

void BrowserAccessibilityManagerWin::Remove(LONG child_id) {
  child_id_map_.erase(child_id);
}

BrowserAccessibilityWin* BrowserAccessibilityManagerWin::GetFromChildID(
    LONG child_id) {
  base::hash_map<LONG, BrowserAccessibilityWin*>::iterator iter =
      child_id_map_.find(child_id);
  if (iter != child_id_map_.end()) {
    return iter->second;
  } else {
    return NULL;
  }
}

IAccessible* BrowserAccessibilityManagerWin::GetParentWindowIAccessible() {
  return window_iaccessible_;
}

BrowserAccessibilityWin* BrowserAccessibilityManagerWin::GetFocus(
    BrowserAccessibilityWin* root) {
  if (focus_ && (!root || focus_->IsDescendantOf(root)))
    return focus_;

  return NULL;
}

void BrowserAccessibilityManagerWin::SetFocus(
    const BrowserAccessibilityWin& node) {
  if (delegate_)
    delegate_->SetAccessibilityFocus(node.renderer_id());
}

void BrowserAccessibilityManagerWin::DoDefaultAction(
    const BrowserAccessibilityWin& node) {
  if (delegate_)
    delegate_->AccessibilityDoDefaultAction(node.renderer_id());
}

bool BrowserAccessibilityManagerWin::CanModifyTreeInPlace(
    BrowserAccessibilityWin* current_root,
    const webkit_glue::WebAccessibility& new_root) {
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

void BrowserAccessibilityManagerWin::ModifyTreeInPlace(
      BrowserAccessibilityWin* current_root,
      const webkit_glue::WebAccessibility& new_root) {
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


BrowserAccessibilityWin* BrowserAccessibilityManagerWin::UpdateTree(
    const webkit_glue::WebAccessibility& acc_obj) {
  base::hash_map<int, LONG>::iterator iter =
      renderer_id_to_child_id_map_.find(acc_obj.id);
  if (iter == renderer_id_to_child_id_map_.end())
    return NULL;

  LONG child_id = iter->second;
  BrowserAccessibilityWin* old_browser_acc = GetFromChildID(child_id);
  if (!old_browser_acc)
    return NULL;

  if (CanModifyTreeInPlace(old_browser_acc, acc_obj)) {
    ModifyTreeInPlace(old_browser_acc, acc_obj);
    return old_browser_acc;
  }

  BrowserAccessibilityWin* new_browser_acc = CreateAccessibilityTree(
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

IAccessible* BrowserAccessibilityManagerWin::GetRootAccessible() {
  return root_;
}

void BrowserAccessibilityManagerWin::OnAccessibilityObjectStateChange(
    const webkit_glue::WebAccessibility& acc_obj) {
  BrowserAccessibilityWin* new_browser_acc = UpdateTree(acc_obj);
  if (!new_browser_acc)
    return;

  LONG child_id = new_browser_acc->child_id();
  NotifyWinEvent(
      EVENT_OBJECT_STATECHANGE, GetParentWindow(), OBJID_CLIENT, child_id);
}

void BrowserAccessibilityManagerWin::OnAccessibilityObjectChildrenChange(
    const webkit_glue::WebAccessibility& acc_obj) {
  BrowserAccessibilityWin* new_browser_acc = UpdateTree(acc_obj);
  if (!new_browser_acc)
    return;

  LONG child_id;
  if (root_ != new_browser_acc) {
    child_id = new_browser_acc->GetParent()->child_id();
  } else {
    child_id = CHILDID_SELF;
  }

  NotifyWinEvent(
      EVENT_OBJECT_REORDER, GetParentWindow(), OBJID_CLIENT, child_id);
}

void BrowserAccessibilityManagerWin::OnAccessibilityObjectFocusChange(
  const webkit_glue::WebAccessibility& acc_obj) {
  BrowserAccessibilityWin* new_browser_acc = UpdateTree(acc_obj);
  if (!new_browser_acc)
    return;

  focus_ = new_browser_acc;
  LONG child_id = new_browser_acc->child_id();
  NotifyWinEvent(EVENT_OBJECT_FOCUS, GetParentWindow(), OBJID_CLIENT, child_id);
}

void BrowserAccessibilityManagerWin::OnAccessibilityObjectLoadComplete(
  const webkit_glue::WebAccessibility& acc_obj) {
  root_->InactivateTree();
  root_->Release();
  focus_ = NULL;

  root_ = CreateAccessibilityTree(NULL, GetNextChildID(), acc_obj, 0);
  if (!focus_)
    focus_ = root_;

  LONG root_id = root_->child_id();
  NotifyWinEvent(EVENT_OBJECT_FOCUS, GetParentWindow(), OBJID_CLIENT, root_id);
  NotifyWinEvent(
    IA2_EVENT_DOCUMENT_LOAD_COMPLETE, GetParentWindow(), OBJID_CLIENT, root_id);
}

void BrowserAccessibilityManagerWin::OnAccessibilityObjectValueChange(
    const webkit_glue::WebAccessibility& acc_obj) {
  BrowserAccessibilityWin* new_browser_acc = UpdateTree(acc_obj);
  if (!new_browser_acc)
    return;

  LONG child_id = new_browser_acc->child_id();
  NotifyWinEvent(
      EVENT_OBJECT_VALUECHANGE, GetParentWindow(), OBJID_CLIENT, child_id);
}

void BrowserAccessibilityManagerWin::OnAccessibilityObjectTextChange(
    const webkit_glue::WebAccessibility& acc_obj) {
  BrowserAccessibilityWin* new_browser_acc = UpdateTree(acc_obj);
  if (!new_browser_acc)
    return;

  LONG child_id = new_browser_acc->child_id();
  NotifyWinEvent(
      IA2_EVENT_TEXT_CARET_MOVED, GetParentWindow(), OBJID_CLIENT, child_id);
}

LONG BrowserAccessibilityManagerWin::GetNextChildID() {
  // Get the next child ID, and wrap around when we get near the end
  // of a 32-bit integer range. It's okay to wrap around; we just want
  // to avoid it as long as possible because clients may cache the ID of
  // an object for a while to determine if they've seen it before.
  next_child_id_--;
  if (next_child_id_ == -2000000000)
    next_child_id_ = -1;

  return next_child_id_;
}

BrowserAccessibilityWin*
BrowserAccessibilityManagerWin::CreateAccessibilityTree(
    BrowserAccessibilityWin* parent,
    int child_id,
    const webkit_glue::WebAccessibility& src,
    int index_in_parent) {
  BrowserAccessibilityWin* instance = factory_->Create();

  instance->Initialize(this, parent, child_id, index_in_parent, src);
  child_id_map_[child_id] = instance;
  renderer_id_to_child_id_map_[src.id] = child_id;
  if ((src.state >> WebAccessibility::STATE_FOCUSED) & 1)
    focus_ = instance;
  for (int i = 0; i < static_cast<int>(src.children.size()); ++i) {
    BrowserAccessibilityWin* child = CreateAccessibilityTree(
        instance, GetNextChildID(), src.children[i], i);
    instance->AddChild(child);
  }

  return instance;
}
