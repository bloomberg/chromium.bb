// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/accessibility/browser_accessibility.h"

#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "content/browser/accessibility/browser_accessibility_manager.h"
#include "content/common/accessibility_messages.h"

namespace content {

typedef AccessibilityNodeData::BoolAttribute BoolAttribute;
typedef AccessibilityNodeData::FloatAttribute FloatAttribute;
typedef AccessibilityNodeData::IntAttribute IntAttribute;
typedef AccessibilityNodeData::StringAttribute StringAttribute;

#if !defined(OS_MACOSX) && \
    !defined(OS_WIN) && \
    !defined(TOOLKIT_GTK) && \
    !defined(OS_ANDROID)
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
      index_in_parent_(0),
      renderer_id_(0),
      role_(0),
      state_(0),
      instance_active_(false) {
}

BrowserAccessibility::~BrowserAccessibility() {
}

bool BrowserAccessibility::PlatformIsLeaf() const {
  return role_ == blink::WebAXRoleStaticText || child_count() == 0;
}

uint32 BrowserAccessibility::PlatformChildCount() const {
  return PlatformIsLeaf() ? 0 : children_.size();
}

void BrowserAccessibility::DetachTree(
    std::vector<BrowserAccessibility*>* nodes) {
  nodes->push_back(this);
  for (size_t i = 0; i < children_.size(); ++i)
    children_[i]->DetachTree(nodes);
  children_.clear();
  parent_ = NULL;
}

void BrowserAccessibility::InitializeTreeStructure(
    BrowserAccessibilityManager* manager,
    BrowserAccessibility* parent,
    int32 renderer_id,
    int32 index_in_parent) {
  manager_ = manager;
  parent_ = parent;
  renderer_id_ = renderer_id;
  index_in_parent_ = index_in_parent;
}

void BrowserAccessibility::InitializeData(const AccessibilityNodeData& src) {
  DCHECK_EQ(renderer_id_, src.id);
  role_ = src.role;
  state_ = src.state;
  string_attributes_ = src.string_attributes;
  int_attributes_ = src.int_attributes;
  float_attributes_ = src.float_attributes;
  bool_attributes_ = src.bool_attributes;
  intlist_attributes_ = src.intlist_attributes;
  html_attributes_ = src.html_attributes;
  location_ = src.location;
  instance_active_ = true;

  GetStringAttribute(AccessibilityNodeData::ATTR_NAME, &name_);
  GetStringAttribute(AccessibilityNodeData::ATTR_VALUE, &value_);

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

void BrowserAccessibility::SetLocation(const gfx::Rect& new_location) {
  location_ = new_location;
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

BrowserAccessibility* BrowserAccessibility::PlatformGetChild(
    uint32 child_index) const {
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

gfx::Rect BrowserAccessibility::GetLocalBoundsRect() const {
  gfx::Rect bounds = location_;

  // Walk up the parent chain. Every time we encounter a Web Area, offset
  // based on the scroll bars and then offset based on the origin of that
  // nested web area.
  BrowserAccessibility* parent = parent_;
  bool need_to_offset_web_area =
      (role_ == blink::WebAXRoleWebArea ||
       role_ == blink::WebAXRoleRootWebArea);
  while (parent) {
    if (need_to_offset_web_area &&
        parent->location().width() > 0 &&
        parent->location().height() > 0) {
      bounds.Offset(parent->location().x(), parent->location().y());
      need_to_offset_web_area = false;
    }

    // On some platforms, we don't want to take the root scroll offsets
    // into account.
    if (parent->role() == blink::WebAXRoleRootWebArea &&
        !manager()->UseRootScrollOffsetsWhenComputingBounds()) {
      break;
    }

    if (parent->role() == blink::WebAXRoleWebArea ||
        parent->role() == blink::WebAXRoleRootWebArea) {
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

gfx::Rect BrowserAccessibility::GetGlobalBoundsRect() const {
  gfx::Rect bounds = GetLocalBoundsRect();

  // Adjust the bounds by the top left corner of the containing view's bounds
  // in screen coordinates.
  bounds.Offset(manager_->GetViewBounds().OffsetFromOrigin());

  return bounds;
}

gfx::Rect BrowserAccessibility::GetLocalBoundsForRange(int start, int len)
    const {
  DCHECK_EQ(role_, blink::WebAXRoleStaticText);
  int end = start + len;
  int child_start = 0;
  int child_end = 0;

  gfx::Rect bounds;
  for (size_t i = 0; i < children_.size() && child_end < start + len; ++i) {
    BrowserAccessibility* child = children_[i];
    DCHECK_EQ(child->role(), blink::WebAXRoleInlineTextBox);
    std::string child_text;
    child->GetStringAttribute(AccessibilityNodeData::ATTR_VALUE, &child_text);
    int child_len = static_cast<int>(child_text.size());
    child_start = child_end;
    child_end += child_len;

    if (child_end < start)
      continue;

    int overlap_start = std::max(start, child_start);
    int overlap_end = std::min(end, child_end);

    int local_start = overlap_start - child_start;
    int local_end = overlap_end - child_start;

    gfx::Rect child_rect = child->location();
    int text_direction = child->GetIntAttribute(
        AccessibilityNodeData::ATTR_TEXT_DIRECTION);
    const std::vector<int32>& character_offsets = child->GetIntListAttribute(
        AccessibilityNodeData::ATTR_CHARACTER_OFFSETS);
    int start_pixel_offset =
        local_start > 0 ? character_offsets[local_start - 1] : 0;
    int end_pixel_offset =
        local_end > 0 ? character_offsets[local_end - 1] : 0;

    gfx::Rect child_overlap_rect;
    switch (text_direction) {
      case blink::WebAXTextDirectionLR: {
        int left = child_rect.x() + start_pixel_offset;
        int right = child_rect.x() + end_pixel_offset;
        child_overlap_rect = gfx::Rect(left, child_rect.y(),
                                       right - left, child_rect.height());
        break;
      }
      case blink::WebAXTextDirectionRL: {
        int right = child_rect.right() - start_pixel_offset;
        int left = child_rect.right() - end_pixel_offset;
        child_overlap_rect = gfx::Rect(left, child_rect.y(),
                                       right - left, child_rect.height());
        break;
      }
      case blink::WebAXTextDirectionTB: {
        int top = child_rect.y() + start_pixel_offset;
        int bottom = child_rect.y() + end_pixel_offset;
        child_overlap_rect = gfx::Rect(child_rect.x(), top,
                                       child_rect.width(), bottom - top);
        break;
      }
      case blink::WebAXTextDirectionBT: {
        int bottom = child_rect.bottom() - start_pixel_offset;
        int top = child_rect.bottom() - end_pixel_offset;
        child_overlap_rect = gfx::Rect(child_rect.x(), top,
                                       child_rect.width(), bottom - top);
        break;
      }
      default:
        NOTREACHED();
    }

    if (bounds.width() == 0 && bounds.height() == 0)
      bounds = child_overlap_rect;
    else
      bounds.Union(child_overlap_rect);
  }

  return bounds;
}

gfx::Rect BrowserAccessibility::GetGlobalBoundsForRange(int start, int len)
    const {
  gfx::Rect bounds = GetLocalBoundsForRange(start, len);

  // Adjust the bounds by the top left corner of the containing view's bounds
  // in screen coordinates.
  bounds.Offset(manager_->GetViewBounds().OffsetFromOrigin());

  return bounds;
}

BrowserAccessibility* BrowserAccessibility::BrowserAccessibilityForPoint(
    const gfx::Point& point) {
  // Walk the children recursively looking for the BrowserAccessibility that
  // most tightly encloses the specified point.
  for (int i = static_cast<int>(PlatformChildCount()) - 1; i >= 0; --i) {
    BrowserAccessibility* child = PlatformGetChild(i);
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
      blink::WebAXEventHide, this);

  instance_active_ = false;
  manager_->RemoveNode(this);
  NativeReleaseReference();
}

void BrowserAccessibility::NativeReleaseReference() {
  delete this;
}

bool BrowserAccessibility::HasBoolAttribute(BoolAttribute attribute) const {
  for (size_t i = 0; i < bool_attributes_.size(); ++i) {
    if (bool_attributes_[i].first == attribute)
      return true;
  }

  return false;
}


bool BrowserAccessibility::GetBoolAttribute(BoolAttribute attribute) const {
  for (size_t i = 0; i < bool_attributes_.size(); ++i) {
    if (bool_attributes_[i].first == attribute)
      return bool_attributes_[i].second;
  }

  return false;
}

bool BrowserAccessibility::GetBoolAttribute(
    BoolAttribute attribute, bool* value) const {
  for (size_t i = 0; i < bool_attributes_.size(); ++i) {
    if (bool_attributes_[i].first == attribute) {
      *value = bool_attributes_[i].second;
      return true;
    }
  }

  return false;
}

bool BrowserAccessibility::HasFloatAttribute(FloatAttribute attribute) const {
  for (size_t i = 0; i < float_attributes_.size(); ++i) {
    if (float_attributes_[i].first == attribute)
      return true;
  }

  return false;
}

float BrowserAccessibility::GetFloatAttribute(FloatAttribute attribute) const {
  for (size_t i = 0; i < float_attributes_.size(); ++i) {
    if (float_attributes_[i].first == attribute)
      return float_attributes_[i].second;
  }

  return 0.0;
}

bool BrowserAccessibility::GetFloatAttribute(
    FloatAttribute attribute, float* value) const {
  for (size_t i = 0; i < float_attributes_.size(); ++i) {
    if (float_attributes_[i].first == attribute) {
      *value = float_attributes_[i].second;
      return true;
    }
  }

  return false;
}

bool BrowserAccessibility::HasIntAttribute(IntAttribute attribute) const {
  for (size_t i = 0; i < int_attributes_.size(); ++i) {
    if (int_attributes_[i].first == attribute)
      return true;
  }

  return false;
}

int BrowserAccessibility::GetIntAttribute(IntAttribute attribute) const {
  for (size_t i = 0; i < int_attributes_.size(); ++i) {
    if (int_attributes_[i].first == attribute)
      return int_attributes_[i].second;
  }

  return 0;
}

bool BrowserAccessibility::GetIntAttribute(
    IntAttribute attribute, int* value) const {
  for (size_t i = 0; i < int_attributes_.size(); ++i) {
    if (int_attributes_[i].first == attribute) {
      *value = int_attributes_[i].second;
      return true;
    }
  }

  return false;
}

bool BrowserAccessibility::HasStringAttribute(StringAttribute attribute) const {
  for (size_t i = 0; i < string_attributes_.size(); ++i) {
    if (string_attributes_[i].first == attribute)
      return true;
  }

  return false;
}

const std::string& BrowserAccessibility::GetStringAttribute(
    StringAttribute attribute) const {
  CR_DEFINE_STATIC_LOCAL(std::string, empty_string, ());
  for (size_t i = 0; i < string_attributes_.size(); ++i) {
    if (string_attributes_[i].first == attribute)
      return string_attributes_[i].second;
  }

  return empty_string;
}

bool BrowserAccessibility::GetStringAttribute(
    StringAttribute attribute, std::string* value) const {
  for (size_t i = 0; i < string_attributes_.size(); ++i) {
    if (string_attributes_[i].first == attribute) {
      *value = string_attributes_[i].second;
      return true;
    }
  }

  return false;
}

base::string16 BrowserAccessibility::GetString16Attribute(
    StringAttribute attribute) const {
  std::string value_utf8;
  if (!GetStringAttribute(attribute, &value_utf8))
    return base::string16();
  return UTF8ToUTF16(value_utf8);
}

bool BrowserAccessibility::GetString16Attribute(
    StringAttribute attribute,
    base::string16* value) const {
  std::string value_utf8;
  if (!GetStringAttribute(attribute, &value_utf8))
    return false;
  *value = UTF8ToUTF16(value_utf8);
  return true;
}

void BrowserAccessibility::SetStringAttribute(
    StringAttribute attribute, const std::string& value) {
  for (size_t i = 0; i < string_attributes_.size(); ++i) {
    if (string_attributes_[i].first == attribute) {
      string_attributes_[i].second = value;
      return;
    }
  }
  if (!value.empty())
    string_attributes_.push_back(std::make_pair(attribute, value));
}

bool BrowserAccessibility::HasIntListAttribute(
    AccessibilityNodeData::IntListAttribute attribute) const {
  for (size_t i = 0; i < intlist_attributes_.size(); ++i) {
    if (intlist_attributes_[i].first == attribute)
      return true;
  }

  return false;
}

const std::vector<int32>& BrowserAccessibility::GetIntListAttribute(
    AccessibilityNodeData::IntListAttribute attribute) const {
  CR_DEFINE_STATIC_LOCAL(std::vector<int32>, empty_vector, ());
  for (size_t i = 0; i < intlist_attributes_.size(); ++i) {
    if (intlist_attributes_[i].first == attribute)
      return intlist_attributes_[i].second;
  }

  return empty_vector;
}

bool BrowserAccessibility::GetIntListAttribute(
    AccessibilityNodeData::IntListAttribute attribute,
    std::vector<int32>* value) const {
  for (size_t i = 0; i < intlist_attributes_.size(); ++i) {
    if (intlist_attributes_[i].first == attribute) {
      *value = intlist_attributes_[i].second;
      return true;
    }
  }

  return false;
}

bool BrowserAccessibility::GetHtmlAttribute(
    const char* html_attr, std::string* value) const {
  for (size_t i = 0; i < html_attributes_.size(); ++i) {
    const std::string& attr = html_attributes_[i].first;
    if (LowerCaseEqualsASCII(attr, html_attr)) {
      *value = html_attributes_[i].second;
      return true;
    }
  }

  return false;
}

bool BrowserAccessibility::GetHtmlAttribute(
    const char* html_attr, base::string16* value) const {
  std::string value_utf8;
  if (!GetHtmlAttribute(html_attr, &value_utf8))
    return false;
  *value = UTF8ToUTF16(value_utf8);
  return true;
}

bool BrowserAccessibility::GetAriaTristate(
    const char* html_attr,
    bool* is_defined,
    bool* is_mixed) const {
  *is_defined = false;
  *is_mixed = false;

  base::string16 value;
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

bool BrowserAccessibility::HasState(blink::WebAXState state_enum) const {
  return (state_ >> state_enum) & 1;
}

bool BrowserAccessibility::IsEditableText() const {
  // These roles don't have readonly set, but they're not editable text.
  if (role_ == blink::WebAXRoleScrollArea ||
      role_ == blink::WebAXRoleColumn ||
      role_ == blink::WebAXRoleTableHeaderContainer) {
    return false;
  }

  // Note: WebAXStateReadonly being false means it's either a text control,
  // or contenteditable. We also check for editable text roles to cover
  // another element that has role=textbox set on it.
  return (!HasState(blink::WebAXStateReadonly) ||
          role_ == blink::WebAXRoleTextField ||
          role_ == blink::WebAXRoleTextArea);
}

std::string BrowserAccessibility::GetTextRecursive() const {
  if (!name_.empty()) {
    return name_;
  }

  std::string result;
  for (uint32 i = 0; i < PlatformChildCount(); ++i)
    result += PlatformGetChild(i)->GetTextRecursive();
  return result;
}

}  // namespace content
