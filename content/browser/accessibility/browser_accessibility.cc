// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/accessibility/browser_accessibility.h"

#include "base/logging.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "content/browser/accessibility/browser_accessibility_manager.h"
#include "content/common/accessibility_messages.h"

namespace content {

typedef AccessibilityNodeData::BoolAttribute BoolAttribute;
typedef AccessibilityNodeData::FloatAttribute FloatAttribute;
typedef AccessibilityNodeData::IntAttribute IntAttribute;
typedef AccessibilityNodeData::StringAttribute StringAttribute;

#if !defined(OS_MACOSX) && \
    !defined(OS_WIN) && \
    !defined(TOOLKIT_GTK)
// We have subclassess of BrowserAccessibility on Mac, Linux/GTK,
// and Win. For any other platform, instantiate the base class.
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

void BrowserAccessibility::InitializeTreeStructure(
    BrowserAccessibilityManager* manager,
    BrowserAccessibility* parent,
    int32 child_id,
    int32 renderer_id,
    int32 index_in_parent) {
  manager_ = manager;
  parent_ = parent;
  child_id_ = child_id;
  renderer_id_ = renderer_id;
  index_in_parent_ = index_in_parent;
}

void BrowserAccessibility::InitializeData(const AccessibilityNodeData& src) {
  DCHECK_EQ(renderer_id_, src.id);
  name_ = src.name;
  value_ = src.value;
  role_ = src.role;
  state_ = src.state;
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
  instance_active_ = true;

  PreInitialize();
}

bool BrowserAccessibility::IsNative() const {
  return false;
}

void BrowserAccessibility::SwapChildren(
    std::vector<BrowserAccessibility*>& children) {
  children.swap(children_);
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

  // Walk up the parent chain. Every time we encounter a Web Area, offset
  // based on the scroll bars and then offset based on the origin of that
  // nested web area.
  BrowserAccessibility* parent = parent_;
  bool need_to_offset_web_area =
      (role_ == AccessibilityNodeData::ROLE_WEB_AREA ||
       role_ == AccessibilityNodeData::ROLE_ROOT_WEB_AREA);
  while (parent) {
    if (need_to_offset_web_area &&
        parent->location().width() > 0 &&
        parent->location().height() > 0) {
      bounds.Offset(parent->location().x(), parent->location().y());
      need_to_offset_web_area = false;
    }
    if (parent->role() == AccessibilityNodeData::ROLE_WEB_AREA ||
        parent->role() == AccessibilityNodeData::ROLE_ROOT_WEB_AREA) {
      int sx = 0;
      int sy = 0;
      if (parent->GetIntAttribute(AccessibilityNodeData::ATTR_SCROLL_X, &sx) &&
          parent->GetIntAttribute(AccessibilityNodeData::ATTR_SCROLL_Y, &sy)) {
        bounds.Offset(-sx, -sy);
      }
      need_to_offset_web_area = true;
    }
    parent = parent->parent();
  }

  return bounds;
}

gfx::Rect BrowserAccessibility::GetGlobalBoundsRect() {
  gfx::Rect bounds = GetLocalBoundsRect();

  // Adjust the bounds by the top left corner of the containing view's bounds
  // in screen coordinates.
  bounds.Offset(manager_->GetViewBounds().OffsetFromOrigin());

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

void BrowserAccessibility::Destroy() {
  for (std::vector<BrowserAccessibility*>::iterator iter = children_.begin();
       iter != children_.end();
       ++iter) {
    (*iter)->Destroy();
  }
  children_.clear();

  // Allow the object to fire a TextRemoved notification.
  name_.clear();
  value_.clear();
  PostInitialize();

  manager_->NotifyAccessibilityEvent(
      AccessibilityNotificationObjectHide, this);

  instance_active_ = false;
  manager_->Remove(this);
  NativeReleaseReference();
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

bool BrowserAccessibility::HasState(
    AccessibilityNodeData::State state_enum) const {
  return (state_ >> state_enum) & 1;
}

bool BrowserAccessibility::IsEditableText() const {
  // Note: STATE_READONLY being false means it's either a text control,
  // or contenteditable. We also check for editable text roles to cover
  // another element that has role=textbox set on it.
  return (!HasState(AccessibilityNodeData::STATE_READONLY) ||
          role_ == AccessibilityNodeData::ROLE_TEXT_FIELD ||
          role_ == AccessibilityNodeData::ROLE_TEXTAREA);
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

}  // namespace content
