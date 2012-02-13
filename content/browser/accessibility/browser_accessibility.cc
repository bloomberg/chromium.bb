// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/accessibility/browser_accessibility.h"

#include "base/logging.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "content/browser/accessibility/browser_accessibility_manager.h"
#include "content/common/accessibility_messages.h"

typedef WebAccessibility::BoolAttribute BoolAttribute;
typedef WebAccessibility::FloatAttribute FloatAttribute;
typedef WebAccessibility::IntAttribute IntAttribute;
typedef WebAccessibility::StringAttribute StringAttribute;

#if (defined(OS_POSIX) && !defined(OS_MACOSX)) || defined(USE_AURA)
// There's no OS-specific implementation of BrowserAccessibilityManager
// on Unix, so just instantiate the base class.
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

void BrowserAccessibility::DetachTree(
    std::vector<BrowserAccessibility*>* nodes) {
  nodes->push_back(this);
  for (size_t i = 0; i < children_.size(); i++)
    children_[i]->DetachTree(nodes);
  children_.clear();
  parent_ = NULL;
}

void BrowserAccessibility::PreInitialize(
    BrowserAccessibilityManager* manager,
    BrowserAccessibility* parent,
    int32 child_id,
    int32 index_in_parent,
    const webkit_glue::WebAccessibility& src) {
  manager_ = manager;
  parent_ = parent;
  child_id_ = child_id;
  index_in_parent_ = index_in_parent;

  // Update all of the rest of the attributes.
  name_ = src.name;
  value_ = src.value;
  role_ = src.role;
  state_ = src.state;
  renderer_id_ = src.id;
  string_attributes_ = src.string_attributes;
  int_attributes_ = src.int_attributes;
  float_attributes_ = src.float_attributes;
  bool_attributes_ = src.bool_attributes;
  html_attributes_ = src.html_attributes;
  location_ = src.location;
  indirect_child_ids_ = src.indirect_child_ids;
  line_breaks_ = src.line_breaks;
  cell_ids_ = src.cell_ids;
  unique_cell_ids_ = src.unique_cell_ids;

  PreInitialize();
}

void BrowserAccessibility::AddChild(BrowserAccessibility* child) {
  children_.push_back(child);
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

gfx::Rect BrowserAccessibility::GetLocalBoundsRect() {
  gfx::Rect bounds = location_;

  // Adjust top left position by the root document's scroll offset.
  BrowserAccessibility* root = manager_->GetRoot();
  int scroll_x = 0;
  int scroll_y = 0;
  if (!root->GetIntAttribute(WebAccessibility::ATTR_SCROLL_X, &scroll_x) ||
      !root->GetIntAttribute(WebAccessibility::ATTR_SCROLL_Y, &scroll_y)) {
    return bounds;
  }
  bounds.Offset(-scroll_x, -scroll_y);

  return bounds;
}

gfx::Rect BrowserAccessibility::GetGlobalBoundsRect() {
  gfx::Rect bounds = GetLocalBoundsRect();

  // Adjust the bounds by the top left corner of the containing view's bounds
  // in screen coordinates.
  gfx::Point top_left = manager_->GetViewBounds().origin();
  bounds.Offset(top_left);

  return bounds;
}

BrowserAccessibility* BrowserAccessibility::BrowserAccessibilityForPoint(
    const gfx::Point& point) {
  // Walk the children recursively looking for the BrowserAccessibility that
  // most tightly encloses the specified point.
  for (int i = children_.size() - 1; i >= 0; --i) {
    BrowserAccessibility* child = children_[i];
    if (child->GetGlobalBoundsRect().Contains(point))
      return child->BrowserAccessibilityForPoint(point);
  }
  return this;
}

void BrowserAccessibility::InternalAddReference() {
  ref_count_++;
}

void BrowserAccessibility::InternalReleaseReference(bool recursive) {
  DCHECK_GT(ref_count_, 0);
  // It is a bug for ref_count_ to be gt 1 when |recursive| is true.
  DCHECK(!recursive || ref_count_ == 1);

  if (recursive || ref_count_ == 1) {
    for (std::vector<BrowserAccessibility*>::iterator iter = children_.begin();
         iter != children_.end();
         ++iter) {
      (*iter)->InternalReleaseReference(true);
    }
    children_.clear();
    // Force this to be the last ref. As the DCHECK above indicates, this
    // should always be the case. Make it so defensively.
    ref_count_ = 1;
  }

  ref_count_--;
  if (ref_count_ == 0) {
    // Allow the object to fire a TextRemoved notification.
    name_.clear();
    value_.clear();
    PostInitialize();

    manager_->NotifyAccessibilityEvent(
        AccessibilityNotificationObjectHide, this);

    instance_active_ = false;
    manager_->Remove(child_id_, renderer_id_);
    NativeReleaseReference();
  }
}

void BrowserAccessibility::NativeReleaseReference() {
  delete this;
}

bool BrowserAccessibility::GetBoolAttribute(
    BoolAttribute attribute, bool* value) const {
  BoolAttrMap::const_iterator iter = bool_attributes_.find(attribute);
  if (iter != bool_attributes_.end()) {
    *value = iter->second;
    return true;
  }

  return false;
}

bool BrowserAccessibility::GetFloatAttribute(
    FloatAttribute attribute, float* value) const {
  FloatAttrMap::const_iterator iter = float_attributes_.find(attribute);
  if (iter != float_attributes_.end()) {
    *value = iter->second;
    return true;
  }

  return false;
}

bool BrowserAccessibility::GetIntAttribute(
    IntAttribute attribute, int* value) const {
  IntAttrMap::const_iterator iter = int_attributes_.find(attribute);
  if (iter != int_attributes_.end()) {
    *value = iter->second;
    return true;
  }

  return false;
}

bool BrowserAccessibility::GetStringAttribute(
    StringAttribute attribute,
    string16* value) const {
  StringAttrMap::const_iterator iter = string_attributes_.find(attribute);
  if (iter != string_attributes_.end()) {
    *value = iter->second;
    return true;
  }

  return false;
}

bool BrowserAccessibility::GetHtmlAttribute(
    const char* html_attr, string16* value) const {
  for (size_t i = 0; i < html_attributes_.size(); i++) {
    const string16& attr = html_attributes_[i].first;
    if (LowerCaseEqualsASCII(attr, html_attr)) {
      *value = html_attributes_[i].second;
      return true;
    }
  }

  return false;
}

bool BrowserAccessibility::GetAriaTristate(
    const char* html_attr,
    bool* is_defined,
    bool* is_mixed) const {
  *is_defined = false;
  *is_mixed = false;

  string16 value;
  if (!GetHtmlAttribute(html_attr, &value) ||
      value.empty() ||
      EqualsASCII(value, "undefined")) {
    return false;  // Not set (and *is_defined is also false)
  }

  *is_defined = true;

  if (EqualsASCII(value, "true"))
    return true;

  if (EqualsASCII(value, "mixed"))
    *is_mixed = true;

  return false;  // Not set
}

bool BrowserAccessibility::HasState(WebAccessibility::State state_enum) const {
  return (state_ >> state_enum) & 1;
}

bool BrowserAccessibility::IsEditableText() const {
  return (role_ == WebAccessibility::ROLE_TEXT_FIELD ||
          role_ == WebAccessibility::ROLE_TEXTAREA);
}

string16 BrowserAccessibility::GetTextRecursive() const {
  if (!name_.empty()) {
    return name_;
  }

  string16 result;
  for (size_t i = 0; i < children_.size(); ++i)
    result += children_[i]->GetTextRecursive();
  return result;
}

void BrowserAccessibility::PreInitialize() {
  instance_active_ = true;
}
