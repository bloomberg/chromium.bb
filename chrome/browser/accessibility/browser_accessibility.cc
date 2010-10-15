// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/accessibility/browser_accessibility.h"

#include "base/logging.h"
#include "chrome/browser/accessibility/browser_accessibility_manager.h"

BrowserAccessibility::BrowserAccessibility()
    : manager_(NULL),
      parent_(NULL),
      child_id_(0),
      index_in_parent_(0),
      renderer_id_(0),
      role_(0),
      state_(0) {
}

BrowserAccessibility::~BrowserAccessibility() {
}

void BrowserAccessibility::Initialize(
    BrowserAccessibilityManager* manager,
    BrowserAccessibility* parent,
    int32 child_id,
    int32 index_in_parent,
    const webkit_glue::WebAccessibility& src) {
  manager_ = manager;
  parent_ = parent;
  child_id_ = child_id;
  index_in_parent_ = index_in_parent;

  renderer_id_ = src.id;
  name_ = src.name;
  value_ = src.value;
  attributes_ = src.attributes;
  html_attributes_ = src.html_attributes;
  location_ = src.location;
  role_ = src.role;
  state_ = src.state;

  Initialize();
}

void BrowserAccessibility::ReleaseTree() {
  // Now we can safely call InactivateTree on our children and remove
  // references to them, so that as much of the tree as possible will be
  // destroyed now - however, nodes that still have references to them
  // might stick around a while until all clients have released them.
  for (std::vector<BrowserAccessibility*>::iterator iter =
           children_.begin();
       iter != children_.end(); ++iter) {
    (*iter)->ReleaseTree();
    (*iter)->ReleaseReference();
  }
  children_.clear();
  manager_->Remove(child_id_);
}

void BrowserAccessibility::AddChild(BrowserAccessibility* child) {
  children_.push_back(child);
}

bool BrowserAccessibility::IsDescendantOf(
    BrowserAccessibility* ancestor) {
  if (this == ancestor) {
    return true;
  } else if (parent_) {
    return parent_->IsDescendantOf(ancestor);
  }

  return false;
}

BrowserAccessibility* BrowserAccessibility::GetParent() {
  return parent_;
}

uint32 BrowserAccessibility::GetChildCount() {
  return children_.size();
}

BrowserAccessibility* BrowserAccessibility::GetChild(uint32 child_index) {
  DCHECK(child_index < children_.size());
  return children_[child_index];
}

BrowserAccessibility* BrowserAccessibility::GetPreviousSibling() {
  if (parent_ && index_in_parent_ > 0)
    return parent_->children_[index_in_parent_ - 1];

  return NULL;
}

BrowserAccessibility* BrowserAccessibility::GetNextSibling() {
  if (parent_ &&
      index_in_parent_ >= 0 &&
      index_in_parent_ < static_cast<int>(parent_->children_.size() - 1)) {
    return parent_->children_[index_in_parent_ + 1];
  }

  return NULL;
}

void BrowserAccessibility::ReplaceChild(
    const BrowserAccessibility* old_acc, BrowserAccessibility* new_acc) {
  DCHECK_EQ(children_[old_acc->index_in_parent_], old_acc);

  old_acc = children_[old_acc->index_in_parent_];
  children_[old_acc->index_in_parent_] = new_acc;
}
