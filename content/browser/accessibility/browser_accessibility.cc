// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/accessibility/browser_accessibility.h"

#include <algorithm>

#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "content/browser/accessibility/browser_accessibility_manager.h"
#include "content/common/accessibility_messages.h"
#include "ui/accessibility/ax_text_utils.h"

namespace content {

#if !defined(OS_WIN) && \
    !defined(OS_MACOSX) && \
    !defined(OS_ANDROID) && \
    !(defined(OS_LINUX) && !defined(OS_CHROMEOS) && defined(USE_X11))
// We have subclassess of BrowserAccessibility on some platforms.
// On other platforms, instantiate the base class.
// static
BrowserAccessibility* BrowserAccessibility::Create() {
  return new BrowserAccessibility();
}
#endif

BrowserAccessibility::BrowserAccessibility()
    : manager_(NULL),
      node_(NULL) {
}

BrowserAccessibility::~BrowserAccessibility() {
}

void BrowserAccessibility::Init(BrowserAccessibilityManager* manager,
    ui::AXNode* node) {
  manager_ = manager;
  node_ = node;
}

bool BrowserAccessibility::PlatformIsLeaf() const {
  if (InternalChildCount() == 0)
    return true;

  // All of these roles may have children that we use as internal
  // implementation details, but we want to expose them as leaves
  // to platform accessibility APIs.
  switch (GetRole()) {
    case ui::AX_ROLE_COMBO_BOX:
    case ui::AX_ROLE_LINE_BREAK:
    case ui::AX_ROLE_SLIDER:
    case ui::AX_ROLE_STATIC_TEXT:
    case ui::AX_ROLE_TEXT_FIELD:
      return true;
    default:
      return false;
  }
}

uint32 BrowserAccessibility::PlatformChildCount() const {
  if (HasIntAttribute(ui::AX_ATTR_CHILD_TREE_ID)) {
    BrowserAccessibilityManager* child_manager =
        BrowserAccessibilityManager::FromID(
            GetIntAttribute(ui::AX_ATTR_CHILD_TREE_ID));
    if (child_manager)
      return 1;

    return 0;
  }

  return PlatformIsLeaf() ? 0 : InternalChildCount();
}

bool BrowserAccessibility::IsNative() const {
  return false;
}

bool BrowserAccessibility::IsDescendantOf(
    const BrowserAccessibility* ancestor) const {
  if (this == ancestor) {
    return true;
  } else if (GetParent()) {
    return GetParent()->IsDescendantOf(ancestor);
  }

  return false;
}

bool BrowserAccessibility::IsTextOnlyObject() const {
  return GetRole() == ui::AX_ROLE_STATIC_TEXT ||
      GetRole() == ui::AX_ROLE_LINE_BREAK;
}

BrowserAccessibility* BrowserAccessibility::PlatformGetChild(
    uint32 child_index) const {
  DCHECK(child_index < PlatformChildCount());
  BrowserAccessibility* result = nullptr;

  if (HasIntAttribute(ui::AX_ATTR_CHILD_TREE_ID)) {
    BrowserAccessibilityManager* child_manager =
        BrowserAccessibilityManager::FromID(
            GetIntAttribute(ui::AX_ATTR_CHILD_TREE_ID));
    if (child_manager)
      result = child_manager->GetRoot();
  } else {
    result = InternalGetChild(child_index);
  }

  return result;
}

bool BrowserAccessibility::PlatformIsChildOfLeaf() const {
  BrowserAccessibility* ancestor = GetParent();
  while (ancestor) {
    if (ancestor->PlatformIsLeaf())
      return true;
    ancestor = ancestor->GetParent();
  }

  return false;
}

BrowserAccessibility* BrowserAccessibility::GetPreviousSibling() const {
  if (GetParent() && GetIndexInParent() > 0)
    return GetParent()->InternalGetChild(GetIndexInParent() - 1);

  return nullptr;
}

BrowserAccessibility* BrowserAccessibility::GetNextSibling() const {
  if (GetParent() &&
      GetIndexInParent() >= 0 &&
      GetIndexInParent() < static_cast<int>(
          GetParent()->InternalChildCount() - 1)) {
    return GetParent()->InternalGetChild(GetIndexInParent() + 1);
  }

  return nullptr;
}

BrowserAccessibility* BrowserAccessibility::PlatformDeepestFirstChild() const {
  if (!PlatformChildCount())
    return nullptr;

  auto deepest_child = PlatformGetChild(0);
  while (deepest_child->PlatformChildCount())
    deepest_child = deepest_child->PlatformGetChild(0);

  return deepest_child;
}

BrowserAccessibility* BrowserAccessibility::PlatformDeepestLastChild() const {
  if (!PlatformChildCount())
    return nullptr;

  auto deepest_child = PlatformGetChild(PlatformChildCount() - 1);
  while (deepest_child->PlatformChildCount()) {
    deepest_child = deepest_child->PlatformGetChild(
        deepest_child->PlatformChildCount() - 1);
  }

  return deepest_child;
}

uint32 BrowserAccessibility::InternalChildCount() const {
  if (!node_ || !manager_)
    return 0;
  return static_cast<uint32>(node_->child_count());
}

BrowserAccessibility* BrowserAccessibility::InternalGetChild(
    uint32 child_index) const {
  if (!node_ || !manager_ || child_index >= InternalChildCount())
    return nullptr;

  const auto child_node = node_->ChildAtIndex(child_index);
  DCHECK(child_node);
  return manager_->GetFromAXNode(child_node);
}

BrowserAccessibility* BrowserAccessibility::GetParent() const {
  if (!node_ || !manager_)
    return NULL;
  ui::AXNode* parent = node_->parent();
  if (parent)
    return manager_->GetFromAXNode(parent);

  return manager_->GetParentNodeFromParentTree();
}

int32 BrowserAccessibility::GetIndexInParent() const {
  return node_ ? node_->index_in_parent() : -1;
}

int32 BrowserAccessibility::GetId() const {
  return node_ ? node_->id() : -1;
}

const ui::AXNodeData& BrowserAccessibility::GetData() const {
  CR_DEFINE_STATIC_LOCAL(ui::AXNodeData, empty_data, ());
  if (node_)
    return node_->data();
  else
    return empty_data;
}

gfx::Rect BrowserAccessibility::GetLocation() const {
  return GetData().location;
}

int32 BrowserAccessibility::GetRole() const {
  return GetData().role;
}

int32 BrowserAccessibility::GetState() const {
  return GetData().state;
}

const BrowserAccessibility::HtmlAttributes&
BrowserAccessibility::GetHtmlAttributes() const {
  return GetData().html_attributes;
}

gfx::Rect BrowserAccessibility::GetLocalBoundsRect() const {
  gfx::Rect bounds = GetLocation();
  FixEmptyBounds(&bounds);
  return ElementBoundsToLocalBounds(bounds);
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
  if (GetRole() != ui::AX_ROLE_STATIC_TEXT) {
    // Apply recursively to all static text descendants. For example, if
    // you call it on a div with two text node children, it just calls
    // GetLocalBoundsForRange on each of the two children (adjusting
    // |start| for each one) and unions the resulting rects.
    gfx::Rect bounds;
    for (size_t i = 0; i < InternalChildCount(); ++i) {
      BrowserAccessibility* child = InternalGetChild(i);
      int child_len = child->GetStaticTextLenRecursive();
      if (start < child_len && start + len > 0) {
        gfx::Rect child_rect = child->GetLocalBoundsForRange(start, len);
        bounds.Union(child_rect);
      }
      start -= child_len;
    }
    return ElementBoundsToLocalBounds(bounds);
  }

  int end = start + len;
  int child_start = 0;
  int child_end = 0;

  gfx::Rect bounds;
  for (size_t i = 0; i < InternalChildCount() && child_end < start + len; ++i) {
    BrowserAccessibility* child = InternalGetChild(i);
    if (child->GetRole() != ui::AX_ROLE_INLINE_TEXT_BOX) {
      DLOG(WARNING) << "BrowserAccessibility objects with role STATIC_TEXT " <<
          "should have children of role INLINE_TEXT_BOX.";
      continue;
    }

    std::string child_text;
    child->GetStringAttribute(ui::AX_ATTR_VALUE, &child_text);
    int child_len = static_cast<int>(child_text.size());
    child_start = child_end;
    child_end += child_len;

    if (child_end < start)
      continue;

    int overlap_start = std::max(start, child_start);
    int overlap_end = std::min(end, child_end);

    int local_start = overlap_start - child_start;
    int local_end = overlap_end - child_start;

    gfx::Rect child_rect = child->GetLocation();
    int text_direction = child->GetIntAttribute(
        ui::AX_ATTR_TEXT_DIRECTION);
    const std::vector<int32>& character_offsets = child->GetIntListAttribute(
        ui::AX_ATTR_CHARACTER_OFFSETS);
    int start_pixel_offset =
        local_start > 0 ? character_offsets[local_start - 1] : 0;
    int end_pixel_offset =
        local_end > 0 ? character_offsets[local_end - 1] : 0;

    gfx::Rect child_overlap_rect;
    switch (text_direction) {
      case ui::AX_TEXT_DIRECTION_NONE:
      case ui::AX_TEXT_DIRECTION_LTR: {
        int left = child_rect.x() + start_pixel_offset;
        int right = child_rect.x() + end_pixel_offset;
        child_overlap_rect = gfx::Rect(left, child_rect.y(),
                                       right - left, child_rect.height());
        break;
      }
      case ui::AX_TEXT_DIRECTION_RTL: {
        int right = child_rect.right() - start_pixel_offset;
        int left = child_rect.right() - end_pixel_offset;
        child_overlap_rect = gfx::Rect(left, child_rect.y(),
                                       right - left, child_rect.height());
        break;
      }
      case ui::AX_TEXT_DIRECTION_TTB: {
        int top = child_rect.y() + start_pixel_offset;
        int bottom = child_rect.y() + end_pixel_offset;
        child_overlap_rect = gfx::Rect(child_rect.x(), top,
                                       child_rect.width(), bottom - top);
        break;
      }
      case ui::AX_TEXT_DIRECTION_BTT: {
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

  return ElementBoundsToLocalBounds(bounds);
}

gfx::Rect BrowserAccessibility::GetGlobalBoundsForRange(int start, int len)
    const {
  gfx::Rect bounds = GetLocalBoundsForRange(start, len);

  // Adjust the bounds by the top left corner of the containing view's bounds
  // in screen coordinates.
  bounds.Offset(manager_->GetViewBounds().OffsetFromOrigin());

  return bounds;
}

int BrowserAccessibility::GetWordStartBoundary(
    int start, ui::TextBoundaryDirection direction) const {
  DCHECK_GE(start, -1);
  // Special offset that indicates that a word boundary has not been found.
  int word_start_not_found = GetStaticTextLenRecursive();
  int word_start = word_start_not_found;

  switch (GetRole()) {
    case ui::AX_ROLE_STATIC_TEXT: {
      int prev_word_start = word_start_not_found;
      int child_start = 0;
      int child_end = 0;

      // Go through the inline text boxes.
      for (size_t i = 0; i < InternalChildCount(); ++i) {
        // The next child starts where the previous one ended.
        child_start = child_end;
        BrowserAccessibility* child = InternalGetChild(i);
        DCHECK_EQ(child->GetRole(), ui::AX_ROLE_INLINE_TEXT_BOX);
        const std::string& child_text = child->GetStringAttribute(
            ui::AX_ATTR_VALUE);
        int child_len = static_cast<int>(child_text.size());
        child_end += child_len; // End is one past the last character.

        const std::vector<int32>& word_starts = child->GetIntListAttribute(
            ui::AX_ATTR_WORD_STARTS);
        if (word_starts.empty()) {
          word_start = child_end;
          continue;
        }

        int local_start = start - child_start;
        std::vector<int32>::const_iterator iter = std::upper_bound(
            word_starts.begin(), word_starts.end(), local_start);
        if (iter != word_starts.end()) {
          if (direction == ui::FORWARDS_DIRECTION) {
            word_start = child_start + *iter;
          } else if (direction == ui::BACKWARDS_DIRECTION) {
            if (iter == word_starts.begin()) {
              // Return the position of the last word in the previous child.
              word_start = prev_word_start;
            } else {
              word_start = child_start + *(iter - 1);
            }
          } else {
            NOTREACHED();
          }
          break;
        }

        // No word start that is greater than the requested offset has been
        // found.
        prev_word_start = child_start + *(iter - 1);
        if (direction == ui::FORWARDS_DIRECTION) {
          word_start = child_end;
        } else if (direction == ui::BACKWARDS_DIRECTION) {
          word_start = prev_word_start;
        } else {
          NOTREACHED();
        }
      }
      return word_start;
    }

    case ui::AX_ROLE_LINE_BREAK:
      // Words never start at a line break.
      return word_start_not_found;

    default:
      // If there are no children, the word start boundary is still unknown or
      // found previously depending on the direction.
      if (!InternalChildCount())
        return word_start_not_found;

      int child_start = 0;
      for (size_t i = 0; i < InternalChildCount(); ++i) {
        BrowserAccessibility* child = InternalGetChild(i);
        int child_len = child->GetStaticTextLenRecursive();
        int child_word_start = child->GetWordStartBoundary(start, direction);
        if (child_word_start < child_len) {
          // We have found a possible word boundary.
          word_start = child_start + child_word_start;
        }

        // Decide when to stop searching.
        if ((word_start != word_start_not_found &&
            direction == ui::FORWARDS_DIRECTION) ||
            (start < child_len &&
            direction == ui::BACKWARDS_DIRECTION)) {
          break;
        }

        child_start += child_len;
        if (start >= child_len)
          start -= child_len;
        else
          start = -1;
      }
      return word_start;
  }
}

BrowserAccessibility* BrowserAccessibility::BrowserAccessibilityForPoint(
    const gfx::Point& point) {
  // The best result found that's a child of this object.
  BrowserAccessibility* child_result = NULL;
  // The best result that's an indirect descendant like grandchild, etc.
  BrowserAccessibility* descendant_result = NULL;

  // Walk the children recursively looking for the BrowserAccessibility that
  // most tightly encloses the specified point. Walk backwards so that in
  // the absence of any other information, we assume the object that occurs
  // later in the tree is on top of one that comes before it.
  for (int i = static_cast<int>(PlatformChildCount()) - 1; i >= 0; --i) {
    BrowserAccessibility* child = PlatformGetChild(i);

    // Skip table columns because cells are only contained in rows,
    // not columns.
    if (child->GetRole() == ui::AX_ROLE_COLUMN)
      continue;

    if (child->GetGlobalBoundsRect().Contains(point)) {
      BrowserAccessibility* result = child->BrowserAccessibilityForPoint(point);
      if (result == child && !child_result)
        child_result = result;
      if (result != child && !descendant_result)
        descendant_result = result;
    }

    if (child_result && descendant_result)
      break;
  }

  // Explanation of logic: it's possible that this point overlaps more than
  // one child of this object. If so, as a heuristic we prefer if the point
  // overlaps a descendant of one of the two children and not the other.
  // As an example, suppose you have two rows of buttons - the buttons don't
  // overlap, but the rows do. Without this heuristic, we'd greedily only
  // consider one of the containers.
  if (descendant_result)
    return descendant_result;
  if (child_result)
    return child_result;

  return this;
}

void BrowserAccessibility::Destroy() {
  // Allow the object to fire a TextRemoved notification.
  manager_->NotifyAccessibilityEvent(ui::AX_EVENT_HIDE, this);
  node_ = NULL;
  manager_ = NULL;

  NativeReleaseReference();
}

void BrowserAccessibility::NativeReleaseReference() {
  delete this;
}

bool BrowserAccessibility::HasBoolAttribute(
    ui::AXBoolAttribute attribute) const {
  return GetData().HasBoolAttribute(attribute);
}

bool BrowserAccessibility::GetBoolAttribute(
    ui::AXBoolAttribute attribute) const {
  return GetData().GetBoolAttribute(attribute);
}

bool BrowserAccessibility::GetBoolAttribute(
    ui::AXBoolAttribute attribute, bool* value) const {
  return GetData().GetBoolAttribute(attribute, value);
}

bool BrowserAccessibility::HasFloatAttribute(
    ui::AXFloatAttribute attribute) const {
  return GetData().HasFloatAttribute(attribute);
}

float BrowserAccessibility::GetFloatAttribute(
    ui::AXFloatAttribute attribute) const {
  return GetData().GetFloatAttribute(attribute);
}

bool BrowserAccessibility::GetFloatAttribute(
    ui::AXFloatAttribute attribute, float* value) const {
  return GetData().GetFloatAttribute(attribute, value);
}

bool BrowserAccessibility::HasIntAttribute(
    ui::AXIntAttribute attribute) const {
  return GetData().HasIntAttribute(attribute);
}

int BrowserAccessibility::GetIntAttribute(ui::AXIntAttribute attribute) const {
  return GetData().GetIntAttribute(attribute);
}

bool BrowserAccessibility::GetIntAttribute(
    ui::AXIntAttribute attribute, int* value) const {
  return GetData().GetIntAttribute(attribute, value);
}

bool BrowserAccessibility::HasStringAttribute(
    ui::AXStringAttribute attribute) const {
  return GetData().HasStringAttribute(attribute);
}

const std::string& BrowserAccessibility::GetStringAttribute(
    ui::AXStringAttribute attribute) const {
  return GetData().GetStringAttribute(attribute);
}

bool BrowserAccessibility::GetStringAttribute(
    ui::AXStringAttribute attribute, std::string* value) const {
  return GetData().GetStringAttribute(attribute, value);
}

base::string16 BrowserAccessibility::GetString16Attribute(
    ui::AXStringAttribute attribute) const {
  return GetData().GetString16Attribute(attribute);
}

bool BrowserAccessibility::GetString16Attribute(
    ui::AXStringAttribute attribute,
    base::string16* value) const {
  return GetData().GetString16Attribute(attribute, value);
}

bool BrowserAccessibility::HasIntListAttribute(
    ui::AXIntListAttribute attribute) const {
  return GetData().HasIntListAttribute(attribute);
}

const std::vector<int32>& BrowserAccessibility::GetIntListAttribute(
    ui::AXIntListAttribute attribute) const {
  return GetData().GetIntListAttribute(attribute);
}

bool BrowserAccessibility::GetIntListAttribute(
    ui::AXIntListAttribute attribute,
    std::vector<int32>* value) const {
  return GetData().GetIntListAttribute(attribute, value);
}

bool BrowserAccessibility::GetHtmlAttribute(
    const char* html_attr, std::string* value) const {
  return GetData().GetHtmlAttribute(html_attr, value);
}

bool BrowserAccessibility::GetHtmlAttribute(
    const char* html_attr, base::string16* value) const {
  return GetData().GetHtmlAttribute(html_attr, value);
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
      base::EqualsASCII(value, "undefined")) {
    return false;  // Not set (and *is_defined is also false)
  }

  *is_defined = true;

  if (base::EqualsASCII(value, "true"))
    return true;

  if (base::EqualsASCII(value, "mixed"))
    *is_mixed = true;

  return false;  // Not set
}

bool BrowserAccessibility::HasState(ui::AXState state_enum) const {
  return (GetState() >> state_enum) & 1;
}

bool BrowserAccessibility::IsCellOrTableHeaderRole() const {
  return (GetRole() == ui::AX_ROLE_CELL ||
          GetRole() == ui::AX_ROLE_COLUMN_HEADER ||
          GetRole() == ui::AX_ROLE_ROW_HEADER);
}

bool BrowserAccessibility::HasCaret() const {
  if (IsEditableText() && !HasState(ui::AX_STATE_RICHLY_EDITABLE) &&
      HasIntAttribute(ui::AX_ATTR_TEXT_SEL_START) &&
      HasIntAttribute(ui::AX_ATTR_TEXT_SEL_END)) {
    return true;
  }

  // The caret is always at the focus of the selection.
  int32 focus_id = manager()->GetTreeData().sel_focus_object_id;
  BrowserAccessibility* focus_object = manager()->GetFromID(focus_id);
  if (!focus_object)
    return false;

  if (!focus_object->IsDescendantOf(this))
    return false;

  return true;
}

bool BrowserAccessibility::IsEditableText() const {
  return HasState(ui::AX_STATE_EDITABLE);
}

bool BrowserAccessibility::IsWebAreaForPresentationalIframe() const {
  if (GetRole() != ui::AX_ROLE_WEB_AREA &&
      GetRole() != ui::AX_ROLE_ROOT_WEB_AREA) {
    return false;
  }

  BrowserAccessibility* parent = GetParent();
  if (!parent)
    return false;

  BrowserAccessibility* grandparent = parent->GetParent();
  if (!grandparent)
    return false;

  return grandparent->GetRole() == ui::AX_ROLE_IFRAME_PRESENTATIONAL;
}

bool BrowserAccessibility::IsControl() const {
  switch (GetRole()) {
    case ui::AX_ROLE_BUTTON:
    case ui::AX_ROLE_BUTTON_DROP_DOWN:
    case ui::AX_ROLE_CHECK_BOX:
    case ui::AX_ROLE_COLOR_WELL:
    case ui::AX_ROLE_COMBO_BOX:
    case ui::AX_ROLE_DISCLOSURE_TRIANGLE:
    case ui::AX_ROLE_LIST_BOX:
    case ui::AX_ROLE_MENU_BAR:
    case ui::AX_ROLE_MENU_BUTTON:
    case ui::AX_ROLE_MENU_ITEM:
    case ui::AX_ROLE_MENU_ITEM_CHECK_BOX:
    case ui::AX_ROLE_MENU_ITEM_RADIO:
    case ui::AX_ROLE_MENU:
    case ui::AX_ROLE_POP_UP_BUTTON:
    case ui::AX_ROLE_RADIO_BUTTON:
    case ui::AX_ROLE_SCROLL_BAR:
    case ui::AX_ROLE_SEARCH_BOX:
    case ui::AX_ROLE_SLIDER:
    case ui::AX_ROLE_SPIN_BUTTON:
    case ui::AX_ROLE_SWITCH:
    case ui::AX_ROLE_TAB:
    case ui::AX_ROLE_TEXT_FIELD:
    case ui::AX_ROLE_TOGGLE_BUTTON:
    case ui::AX_ROLE_TREE:
      return true;
    default:
      return false;
  }
}

int BrowserAccessibility::GetStaticTextLenRecursive() const {
  if (GetRole() == ui::AX_ROLE_STATIC_TEXT ||
      GetRole() == ui::AX_ROLE_LINE_BREAK) {
    return static_cast<int>(GetStringAttribute(ui::AX_ATTR_VALUE).size());
  }

  int len = 0;
  for (size_t i = 0; i < InternalChildCount(); ++i)
    len += InternalGetChild(i)->GetStaticTextLenRecursive();
  return len;
}

void BrowserAccessibility::FixEmptyBounds(gfx::Rect* bounds) const
{
  if (bounds->width() > 0 && bounds->height() > 0)
    return;

  for (size_t i = 0; i < InternalChildCount(); ++i) {
    // Compute the bounds of each child - this calls FixEmptyBounds
    // recursively if necessary.
    BrowserAccessibility* child = InternalGetChild(i);
    gfx::Rect child_bounds = child->GetLocalBoundsRect();

    // Ignore children that don't have valid bounds themselves.
    if (child_bounds.width() == 0 || child_bounds.height() == 0)
      continue;

    // For the first valid child, just set the bounds to that child's bounds.
    if (bounds->width() == 0 || bounds->height() == 0) {
      *bounds = child_bounds;
      continue;
    }

    // Union each additional child's bounds.
    bounds->Union(child_bounds);
  }
}

gfx::Rect BrowserAccessibility::ElementBoundsToLocalBounds(gfx::Rect bounds)
    const {
  // Walk up the parent chain. Every time we encounter a Web Area, offset
  // based on the scroll bars and then offset based on the origin of that
  // nested web area.
  BrowserAccessibility* parent = GetParent();
  bool need_to_offset_web_area =
      (GetRole() == ui::AX_ROLE_WEB_AREA ||
       GetRole() == ui::AX_ROLE_ROOT_WEB_AREA);
  while (parent) {
    if (need_to_offset_web_area &&
        parent->GetLocation().width() > 0 &&
        parent->GetLocation().height() > 0) {
      bounds.Offset(parent->GetLocation().x(), parent->GetLocation().y());
      need_to_offset_web_area = false;
    }

    // On some platforms, we don't want to take the root scroll offsets
    // into account.
    if (parent->GetRole() == ui::AX_ROLE_ROOT_WEB_AREA &&
        !manager()->UseRootScrollOffsetsWhenComputingBounds()) {
      break;
    }

    if (parent->GetRole() == ui::AX_ROLE_WEB_AREA ||
        parent->GetRole() == ui::AX_ROLE_ROOT_WEB_AREA) {
      int sx = 0;
      int sy = 0;
      if (parent->GetIntAttribute(ui::AX_ATTR_SCROLL_X, &sx) &&
          parent->GetIntAttribute(ui::AX_ATTR_SCROLL_Y, &sy)) {
        bounds.Offset(-sx, -sy);
      }
      need_to_offset_web_area = true;
    }
    parent = parent->GetParent();
  }

  return bounds;
}

}  // namespace content
