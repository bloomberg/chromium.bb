// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/accessibility/browser_accessibility.h"

#include "base/logging.h"
#include "base/string_number_conversions.h"
#include "chrome/browser/accessibility/browser_accessibility_manager.h"

#if defined(OS_LINUX)
// There's no OS-specific implementation of BrowserAccessibilityManager
// on Linux, so just instantiate the base class.
// static
BrowserAccessibility* BrowserAccessibility::Create() {
  return new BrowserAccessibility();
}
#endif

BrowserAccessibility::BrowserAccessibility()
    : manager_(NULL),
      parent_(NULL),
      child_id_(0),
      index_in_parent_(0),
      renderer_id_(0),
      ref_count_(1),
      role_(0),
      state_(0),
      instance_active_(false) {
}

BrowserAccessibility::~BrowserAccessibility() {
}

void BrowserAccessibility::ReplaceChild(
    BrowserAccessibility* old_acc, BrowserAccessibility* new_acc) {
  DCHECK_EQ(children_[old_acc->index_in_parent_], old_acc);

  old_acc = children_[old_acc->index_in_parent_];
  children_[old_acc->index_in_parent_] = new_acc;
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
  indirect_child_ids_ = src.indirect_child_ids;

  Initialize();
}

void BrowserAccessibility::Initialize() {
  instance_active_ = true;
}

void BrowserAccessibility::AddChild(BrowserAccessibility* child) {
  children_.push_back(child);
}

void BrowserAccessibility::DetachTree(
    std::vector<BrowserAccessibility*>* nodes) {
  nodes->push_back(this);
  for (size_t i = 0; i < children_.size(); i++)
    children_[i]->DetachTree(nodes);
  children_.clear();
  parent_ = NULL;
}

void BrowserAccessibility::UpdateParent(BrowserAccessibility* parent,
                                        int index_in_parent) {
  parent_ = parent;
  index_in_parent_ = index_in_parent;
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

gfx::Rect BrowserAccessibility::GetBoundsRect() {
  gfx::Rect bounds = location_;

  // Adjust the bounds by the top left corner of the containing view's bounds
  // in screen coordinates.
  gfx::Point top_left = manager_->GetViewBounds().origin();
  bounds.Offset(top_left);

  // Adjust top left position by the root document's scroll offset.
  BrowserAccessibility* root = manager_->GetRoot();
  int scroll_x = 0;
  int scroll_y = 0;
  root->GetAttributeAsInt(
    WebAccessibility::ATTR_DOC_SCROLLX, &scroll_x);
  root->GetAttributeAsInt(
    WebAccessibility::ATTR_DOC_SCROLLY, &scroll_y);
  bounds.Offset(-scroll_x, -scroll_y);

  return bounds;
}

BrowserAccessibility* BrowserAccessibility::BrowserAccessibilityForPoint(
    const gfx::Point& point) {
  // Walk the children recursively looking for the BrowserAccessibility that
  // most tightly encloses the specified point.
  for (int i = children_.size() - 1; i >= 0; --i) {
    BrowserAccessibility* child = children_[i];
    if (child->GetBoundsRect().Contains(point))
      return child->BrowserAccessibilityForPoint(point);
  }
  return this;
}

void BrowserAccessibility::InternalAddReference() {
  ref_count_++;
}

void BrowserAccessibility::InternalReleaseReference(bool recursive) {
  DCHECK_GT(ref_count_, 0);

  if (recursive || ref_count_ == 1) {
    for (std::vector<BrowserAccessibility*>::iterator iter = children_.begin();
         iter != children_.end();
         ++iter) {
      (*iter)->InternalReleaseReference(true);
    }
  }

  ref_count_--;
  if (ref_count_ == 0) {
    instance_active_ = false;
    children_.clear();
    manager_->Remove(child_id_, renderer_id_);
    NativeReleaseReference();
  }
}

void BrowserAccessibility::NativeReleaseReference() {
  delete this;
}

bool BrowserAccessibility::HasAttribute(
    WebAccessibility::Attribute attribute) {
  return (attributes_.find(attribute) != attributes_.end());
}

bool BrowserAccessibility::GetAttribute(
    WebAccessibility::Attribute attribute, string16* value) {
  std::map<int32, string16>::iterator iter = attributes_.find(attribute);
  if (iter != attributes_.end()) {
    *value = iter->second;
    return true;
  }

  return false;
}

bool BrowserAccessibility::GetAttributeAsInt(
    WebAccessibility::Attribute attribute, int* value_int) {
  string16 value_str;

  if (!GetAttribute(attribute, &value_str))
    return false;

  if (!base::StringToInt(value_str, value_int))
    return false;

  return true;
}
