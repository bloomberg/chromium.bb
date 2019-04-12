// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/accessibility/browser_accessibility.h"

#include <cstddef>

#include <algorithm>
#include <iterator>

#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "content/app/strings/grit/content_strings.h"
#include "content/browser/accessibility/browser_accessibility_manager.h"
#include "content/browser/accessibility/browser_accessibility_state_impl.h"
#include "content/common/accessibility_messages.h"
#include "content/public/common/content_client.h"
#include "ui/accessibility/ax_node_position.h"
#include "ui/accessibility/ax_role_properties.h"
#include "ui/accessibility/ax_text_utils.h"
#include "ui/accessibility/platform/ax_unique_id.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/rect_f.h"

namespace content {

#if !defined(PLATFORM_HAS_NATIVE_ACCESSIBILITY_IMPL)
// static
BrowserAccessibility* BrowserAccessibility::Create() {
  return new BrowserAccessibility();
}
#endif

BrowserAccessibility::BrowserAccessibility()
    : manager_(nullptr), node_(nullptr) {}

BrowserAccessibility::~BrowserAccessibility() {}

void BrowserAccessibility::Init(BrowserAccessibilityManager* manager,
                                ui::AXNode* node) {
  manager_ = manager;
  node_ = node;
}

bool BrowserAccessibility::PlatformIsLeaf() const {
  if (InternalChildCount() == 0)
    return true;

  // These types of objects may have children that we use as internal
  // implementation details, but we want to expose them as leaves to platform
  // accessibility APIs because screen readers might be confused if they find
  // any children.
  if (IsPlainTextField() || IsTextOnlyObject())
    return true;

  // Roles whose children are only presentational according to the ARIA and
  // HTML5 Specs should be hidden from screen readers.
  // (Note that whilst ARIA buttons can have only presentational children, HTML5
  // buttons are allowed to have content.)
  switch (GetRole()) {
    case ax::mojom::Role::kDocCover:
    case ax::mojom::Role::kGraphicsSymbol:
    case ax::mojom::Role::kImage:
    case ax::mojom::Role::kMeter:
    case ax::mojom::Role::kScrollBar:
    case ax::mojom::Role::kSlider:
    case ax::mojom::Role::kSplitter:
    case ax::mojom::Role::kProgressIndicator:
      return true;
    default:
      return false;
  }
}

bool BrowserAccessibility::CanFireEvents() const {
  if (!instance_active())
    return false;
  // Allow events unless this object would be trimmed away.
  return !PlatformIsChildOfLeaf();
}

uint32_t BrowserAccessibility::PlatformChildCount() const {
  if (HasStringAttribute(ax::mojom::StringAttribute::kChildTreeId)) {
    AXTreeID child_tree_id = AXTreeID::FromString(
        GetStringAttribute(ax::mojom::StringAttribute::kChildTreeId));
    BrowserAccessibilityManager* child_manager =
        BrowserAccessibilityManager::FromID(child_tree_id);
    if (child_manager && child_manager->GetRoot()->PlatformGetParent() == this)
      return 1;

    return 0;
  }

  return PlatformIsLeaf() ? 0 : InternalChildCount();
}

BrowserAccessibility* BrowserAccessibility::PlatformGetParent() const {
  if (!instance_active())
    return nullptr;

  ui::AXNode* parent = node_->parent();
  if (parent)
    return manager_->GetFromAXNode(parent);

  return manager_->GetParentNodeFromParentTree();
}

BrowserAccessibility* BrowserAccessibility::PlatformGetSelectionContainer()
    const {
  BrowserAccessibility* container = PlatformGetParent();
  while (container &&
         !ui::IsContainerWithSelectableChildren(container->GetRole())) {
    container = container->PlatformGetParent();
  }
  return container;
}

bool BrowserAccessibility::IsNative() const {
  return false;
}

bool BrowserAccessibility::IsDescendantOf(
    const BrowserAccessibility* ancestor) const {
  if (!ancestor)
    return false;

  if (this == ancestor)
    return true;

  if (PlatformGetParent())
    return PlatformGetParent()->IsDescendantOf(ancestor);

  return false;
}

bool BrowserAccessibility::IsDocument() const {
  return GetRole() == ax::mojom::Role::kRootWebArea ||
         GetRole() == ax::mojom::Role::kWebArea;
}

bool BrowserAccessibility::IsTextOnlyObject() const {
  return node_ && node_->IsText();
}

bool BrowserAccessibility::IsLineBreakObject() const {
  return node_ && node_->IsLineBreak();
}

BrowserAccessibility* BrowserAccessibility::PlatformGetChild(
    uint32_t child_index) const {
  BrowserAccessibility* result = nullptr;

  if (child_index == 0 &&
      HasStringAttribute(ax::mojom::StringAttribute::kChildTreeId)) {
    AXTreeID child_tree_id = AXTreeID::FromString(
        GetStringAttribute(ax::mojom::StringAttribute::kChildTreeId));
    BrowserAccessibilityManager* child_manager =
        BrowserAccessibilityManager::FromID(child_tree_id);
    if (child_manager && child_manager->GetRoot()->PlatformGetParent() == this)
      result = child_manager->GetRoot();
  } else {
    result = InternalGetChild(child_index);
  }

  return result;
}

bool BrowserAccessibility::PlatformIsChildOfLeaf() const {
  BrowserAccessibility* ancestor = InternalGetParent();

  while (ancestor) {
    if (ancestor->PlatformIsLeaf())
      return true;
    ancestor = ancestor->InternalGetParent();
  }

  return false;
}

BrowserAccessibility* BrowserAccessibility::GetClosestPlatformObject() const {
  BrowserAccessibility* platform_object =
      const_cast<BrowserAccessibility*>(this);
  while (platform_object && platform_object->PlatformIsChildOfLeaf())
    platform_object = platform_object->InternalGetParent();

  DCHECK(platform_object);
  return platform_object;
}

BrowserAccessibility* BrowserAccessibility::GetPreviousSibling() const {
  if (PlatformGetParent() && GetIndexInParent() > 0)
    return PlatformGetParent()->InternalGetChild(GetIndexInParent() - 1);

  return nullptr;
}

BrowserAccessibility* BrowserAccessibility::GetNextSibling() const {
  if (PlatformGetParent() && GetIndexInParent() >= 0 &&
      GetIndexInParent() <
          static_cast<int>(PlatformGetParent()->InternalChildCount() - 1)) {
    return PlatformGetParent()->InternalGetChild(GetIndexInParent() + 1);
  }

  return nullptr;
}

bool BrowserAccessibility::IsPreviousSiblingOnSameLine() const {
  const BrowserAccessibility* previous_sibling = GetPreviousSibling();
  if (!previous_sibling)
    return false;

  // Line linkage information might not be provided on non-leaf objects.
  const BrowserAccessibility* leaf_object = PlatformDeepestFirstChild();
  if (!leaf_object)
    leaf_object = this;

  int32_t previous_on_line_id;
  if (leaf_object->GetIntAttribute(ax::mojom::IntAttribute::kPreviousOnLineId,
                                   &previous_on_line_id)) {
    const BrowserAccessibility* previous_on_line =
        manager()->GetFromID(previous_on_line_id);
    // In the case of a static text sibling, the object designated to be the
    // previous object on this line might be one of its children, i.e. the last
    // inline text box.
    return previous_on_line &&
           previous_on_line->IsDescendantOf(previous_sibling);
  }
  return false;
}

bool BrowserAccessibility::IsNextSiblingOnSameLine() const {
  const BrowserAccessibility* next_sibling = GetNextSibling();
  if (!next_sibling)
    return false;

  // Line linkage information might not be provided on non-leaf objects.
  const BrowserAccessibility* leaf_object = PlatformDeepestLastChild();
  if (!leaf_object)
    leaf_object = this;

  int32_t next_on_line_id;
  if (leaf_object->GetIntAttribute(ax::mojom::IntAttribute::kNextOnLineId,
                                   &next_on_line_id)) {
    const BrowserAccessibility* next_on_line =
        manager()->GetFromID(next_on_line_id);
    // In the case of a static text sibling, the object designated to be the
    // next object on this line might be one of its children, i.e. the first
    // inline text box.
    return next_on_line && next_on_line->IsDescendantOf(next_sibling);
  }
  return false;
}

BrowserAccessibility* BrowserAccessibility::PlatformDeepestFirstChild() const {
  if (!PlatformChildCount())
    return nullptr;

  BrowserAccessibility* deepest_child = PlatformGetChild(0);
  while (deepest_child->PlatformChildCount())
    deepest_child = deepest_child->PlatformGetChild(0);

  return deepest_child;
}

BrowserAccessibility* BrowserAccessibility::PlatformDeepestLastChild() const {
  if (!PlatformChildCount())
    return nullptr;

  BrowserAccessibility* deepest_child =
      PlatformGetChild(PlatformChildCount() - 1);
  while (deepest_child->PlatformChildCount()) {
    deepest_child = deepest_child->PlatformGetChild(
        deepest_child->PlatformChildCount() - 1);
  }

  return deepest_child;
}

BrowserAccessibility* BrowserAccessibility::InternalDeepestFirstChild() const {
  if (!InternalChildCount())
    return nullptr;

  BrowserAccessibility* deepest_child = InternalGetChild(0);
  while (deepest_child->InternalChildCount())
    deepest_child = deepest_child->InternalGetChild(0);

  return deepest_child;
}

BrowserAccessibility* BrowserAccessibility::InternalDeepestLastChild() const {
  if (!InternalChildCount())
    return nullptr;

  BrowserAccessibility* deepest_child =
      InternalGetChild(InternalChildCount() - 1);
  while (deepest_child->InternalChildCount()) {
    deepest_child = deepest_child->InternalGetChild(
        deepest_child->InternalChildCount() - 1);
  }

  return deepest_child;
}

uint32_t BrowserAccessibility::InternalChildCount() const {
  if (!node_ || !manager_)
    return 0;
  return static_cast<uint32_t>(node_->child_count());
}

BrowserAccessibility* BrowserAccessibility::InternalGetChild(
    uint32_t child_index) const {
  if (!node_ || !manager_ || child_index >= InternalChildCount())
    return nullptr;

  auto* child_node = node_->ChildAtIndex(child_index);
  DCHECK(child_node);
  return manager_->GetFromAXNode(child_node);
}

BrowserAccessibility* BrowserAccessibility::InternalGetParent() const {
  if (!node_ || !manager_)
    return nullptr;
  ui::AXNode* parent = node_->parent();
  if (parent)
    return manager_->GetFromAXNode(parent);

  return nullptr;
}

int32_t BrowserAccessibility::GetId() const {
  return node_ ? node_->id() : -1;
}

gfx::RectF BrowserAccessibility::GetLocation() const {
  return GetData().relative_bounds.bounds;
}

ax::mojom::Role BrowserAccessibility::GetRole() const {
  return GetData().role;
}

int32_t BrowserAccessibility::GetState() const {
  return GetData().state;
}

const BrowserAccessibility::HtmlAttributes&
BrowserAccessibility::GetHtmlAttributes() const {
  return GetData().html_attributes;
}

gfx::Rect BrowserAccessibility::GetClippedScreenBoundsRect(
    ui::AXOffscreenResult* offscreen_result) const {
  return GetBoundsRect(ui::AXCoordinateSystem::kScreen,
                       ui::AXClippingBehavior::kClipped, offscreen_result);
}

gfx::Rect BrowserAccessibility::GetUnclippedScreenBoundsRect(
    ui::AXOffscreenResult* offscreen_result) const {
  return GetBoundsRect(ui::AXCoordinateSystem::kScreen,
                       ui::AXClippingBehavior::kUnclipped, offscreen_result);
}

gfx::Rect BrowserAccessibility::GetClippedRootFrameBoundsRect(
    ui::AXOffscreenResult* offscreen_result) const {
  return GetBoundsRect(ui::AXCoordinateSystem::kRootFrame,
                       ui::AXClippingBehavior::kClipped, offscreen_result);
}

gfx::Rect BrowserAccessibility::GetUnclippedRootFrameBoundsRect(
    ui::AXOffscreenResult* offscreen_result) const {
  return GetBoundsRect(ui::AXCoordinateSystem::kRootFrame,
                       ui::AXClippingBehavior::kUnclipped, offscreen_result);
}

gfx::Rect BrowserAccessibility::GetClippedFrameBoundsRect(
    ui::AXOffscreenResult* offscreen_result) const {
  return GetBoundsRect(ui::AXCoordinateSystem::kFrame,
                       ui::AXClippingBehavior::kUnclipped, offscreen_result);
}

gfx::Rect BrowserAccessibility::GetUnclippedScreenRangeBoundsRect(
    const int start_offset,
    const int end_offset,
    ui::AXOffscreenResult* offscreen_result) const {
  return GetRangeBoundsRect(
      start_offset, end_offset, ui::AXCoordinateSystem::kScreen,
      ui::AXClippingBehavior::kUnclipped, offscreen_result);
}

gfx::Rect BrowserAccessibility::GetUnclippedRootFrameRangeBoundsRect(
    const int start_offset,
    const int end_offset,
    ui::AXOffscreenResult* offscreen_result) const {
  return GetRangeBoundsRect(
      start_offset, end_offset, ui::AXCoordinateSystem::kRootFrame,
      ui::AXClippingBehavior::kUnclipped, offscreen_result);
}

gfx::Rect BrowserAccessibility::GetBoundsRect(
    const ui::AXCoordinateSystem coordinate_system,
    const ui::AXClippingBehavior clipping_behavior,
    ui::AXOffscreenResult* offscreen_result) const {
  return RelativeToAbsoluteBounds(gfx::RectF(), coordinate_system,
                                  clipping_behavior, offscreen_result);
}

gfx::Rect BrowserAccessibility::GetRangeBoundsRect(
    const int start_offset,
    const int end_offset,
    const ui::AXCoordinateSystem coordinate_system,
    const ui::AXClippingBehavior clipping_behavior,
    ui::AXOffscreenResult* offscreen_result) const {
  int effective_start_offset = start_offset;
  int effective_end_offset = end_offset;

  if (effective_start_offset == effective_end_offset)
    return gfx::Rect();
  if (effective_start_offset > effective_end_offset)
    std::swap(effective_start_offset, effective_end_offset);

  const base::string16& text_str = GetText();
  if (effective_start_offset < 0 ||
      effective_start_offset >= static_cast<int>(text_str.size()))
    return gfx::Rect();
  if (effective_end_offset < 0 ||
      effective_end_offset > static_cast<int>(text_str.size()))
    return gfx::Rect();

  switch (coordinate_system) {
    case ui::AXCoordinateSystem::kScreen: {
      gfx::Rect bounds = GetRootFrameRangeBoundsRect(
          effective_start_offset, effective_end_offset - effective_start_offset,
          clipping_behavior, offscreen_result);
      bounds.Offset(manager_->GetViewBounds().OffsetFromOrigin());
      return bounds;
    }
    case ui::AXCoordinateSystem::kRootFrame:
      return GetRootFrameRangeBoundsRect(
          effective_start_offset, effective_end_offset - effective_start_offset,
          clipping_behavior, offscreen_result);
    case ui::AXCoordinateSystem::kFrame:
      NOTIMPLEMENTED();
      return gfx::Rect();
  }
}

gfx::Rect BrowserAccessibility::GetRootFrameRangeBoundsRect(
    int start,
    int len,
    const ui::AXClippingBehavior clipping_behavior,
    ui::AXOffscreenResult* offscreen_result) const {
  DCHECK_GE(start, 0);
  DCHECK_GE(len, 0);

  // Standard text fields such as textarea have an embedded div inside them that
  // holds all the text.
  // TODO(nektar): This is fragile! Replace with code that flattens tree.
  if (IsPlainTextField() && InternalChildCount() == 1) {
    return InternalGetChild(0)->GetRootFrameRangeBoundsRect(
        start, len, clipping_behavior, offscreen_result);
  }

  if (GetRole() != ax::mojom::Role::kStaticText) {
    gfx::Rect bounds;
    for (size_t i = 0; i < InternalChildCount() && len > 0; ++i) {
      BrowserAccessibility* child = InternalGetChild(i);
      // Child objects are of length one, since they are represented by a single
      // embedded object character. The exception is text-only objects.
      int child_length_in_parent = 1;
      if (child->IsTextOnlyObject())
        child_length_in_parent = static_cast<int>(child->GetText().size());
      if (start < child_length_in_parent) {
        gfx::Rect child_rect;
        if (child->IsTextOnlyObject()) {
          child_rect = child->GetRootFrameRangeBoundsRect(
              start, len, clipping_behavior, offscreen_result);
        } else {
          child_rect = child->GetRootFrameRangeBoundsRect(
              0, static_cast<int>(child->GetText().size()), clipping_behavior,
              offscreen_result);
        }
        bounds.Union(child_rect);
        len -= (child_length_in_parent - start);
      }
      if (start > child_length_in_parent)
        start -= child_length_in_parent;
      else
        start = 0;
    }
    // When past the end of text, the area will be 0.
    // In this case, use bounds provided for the caret.
    return bounds.IsEmpty() ? GetRootFrameBoundsPastEndOfText(clipping_behavior,
                                                              offscreen_result)
                            : bounds;
  }

  int end = start + len;
  int child_start = 0;
  int child_end = 0;
  gfx::Rect bounds;
  for (size_t i = 0; i < InternalChildCount() && child_end < start + len; ++i) {
    BrowserAccessibility* child = InternalGetChild(i);
    if (child->GetRole() != ax::mojom::Role::kInlineTextBox) {
      DLOG(WARNING) << "BrowserAccessibility objects with role STATIC_TEXT "
                    << "should have children of role INLINE_TEXT_BOX.";
      continue;
    }

    int child_length = static_cast<int>(child->GetText().size());
    child_start = child_end;
    child_end += child_length;

    if (child_end < start)
      continue;

    int overlap_start = std::max(start, child_start);
    int overlap_end = std::min(end, child_end);

    int local_start = overlap_start - child_start;
    int local_end = overlap_end - child_start;
    // |local_end| and |local_start| may equal |child_length| when the caret is
    // at the end of a text field.
    DCHECK_GE(local_start, 0);
    DCHECK_LE(local_start, child_length);
    DCHECK_GE(local_end, 0);
    DCHECK_LE(local_end, child_length);

    const std::vector<int32_t>& character_offsets = child->GetIntListAttribute(
        ax::mojom::IntListAttribute::kCharacterOffsets);
    int character_offsets_length = static_cast<int>(character_offsets.size());
    if (character_offsets_length < child_length) {
      // Blink might not return pixel offsets for all characters.
      // Clamp the character range to be within the number of provided pixels.
      local_start = std::min(local_start, character_offsets_length);
      local_end = std::min(local_end, character_offsets_length);
    }
    int start_pixel_offset =
        local_start > 0 ? character_offsets[local_start - 1] : 0;
    int end_pixel_offset = local_end > 0 ? character_offsets[local_end - 1] : 0;
    int max_pixel_offset = character_offsets_length > 0
                               ? character_offsets[character_offsets_length - 1]
                               : 0;

    auto text_direction = static_cast<ax::mojom::TextDirection>(
        child->GetIntAttribute(ax::mojom::IntAttribute::kTextDirection));
    gfx::RectF child_overlap_rect;
    switch (text_direction) {
      case ax::mojom::TextDirection::kNone:
      case ax::mojom::TextDirection::kLtr: {
        int height = child->GetLocation().height();
        child_overlap_rect =
            gfx::RectF(start_pixel_offset, 0,
                       end_pixel_offset - start_pixel_offset, height);
        break;
      }
      case ax::mojom::TextDirection::kRtl: {
        int right = max_pixel_offset - start_pixel_offset;
        int left = max_pixel_offset - end_pixel_offset;
        int height = child->GetLocation().height();
        child_overlap_rect = gfx::RectF(left, 0, right - left, height);
        break;
      }
      case ax::mojom::TextDirection::kTtb: {
        int width = child->GetLocation().width();
        child_overlap_rect = gfx::RectF(0, start_pixel_offset, width,
                                        end_pixel_offset - start_pixel_offset);
        break;
      }
      case ax::mojom::TextDirection::kBtt: {
        int bottom = max_pixel_offset - start_pixel_offset;
        int top = max_pixel_offset - end_pixel_offset;
        int width = child->GetLocation().width();
        child_overlap_rect = gfx::RectF(0, top, width, bottom - top);
        break;
      }
    }

    // Don't clip bounds. Some screen magnifiers (e.g. ZoomText) prefer to
    // get unclipped bounds so that they can make smooth scrolling calculations.
    gfx::Rect absolute_child_rect = child->RelativeToAbsoluteBounds(
        child_overlap_rect, ui::AXCoordinateSystem::kRootFrame,
        clipping_behavior, offscreen_result);
    if (bounds.width() == 0 && bounds.height() == 0) {
      bounds = absolute_child_rect;
    } else {
      bounds.Union(absolute_child_rect);
    }
  }

  return bounds;
}

gfx::Rect BrowserAccessibility::GetScreenRangeBoundsRect(
    int start,
    int len,
    const ui::AXClippingBehavior clipping_behavior,
    ui::AXOffscreenResult* offscreen_result) const {
  gfx::Rect bounds = GetRootFrameRangeBoundsRect(start, len, clipping_behavior,
                                                 offscreen_result);

  // Adjust the bounds by the top left corner of the containing view's bounds
  // in screen coordinates.
  bounds.Offset(manager_->GetViewBounds().OffsetFromOrigin());

  return bounds;
}

gfx::Rect BrowserAccessibility::GetRootFrameBoundsPastEndOfText(
    const ui::AXClippingBehavior clipping_behavior,
    ui::AXOffscreenResult* offscreen_result) const {
  // Step 1: get approximate caret bounds. The thickness may not yet be correct.
  gfx::Rect bounds;
  if (InternalChildCount() > 0) {
    // When past the end of text, use bounds provided by a last child if
    // available, and then correct for thickness of caret.
    BrowserAccessibility* child = InternalGetChild(InternalChildCount() - 1);
    int child_text_len = child->GetText().size();
    bounds = child->GetRootFrameRangeBoundsRect(
        child_text_len, child_text_len, clipping_behavior, offscreen_result);
    if (bounds.width() == 0 && bounds.height() == 0)
      return bounds;  // Inline text boxes info not yet available.
  } else {
    // Compute bounds of where caret would be, based on bounds of object.
    bounds = GetBoundsRect(ui::AXCoordinateSystem::kRootFrame,
                           clipping_behavior, offscreen_result);
  }

  // Step 2: correct for the thickness of the caret.
  auto text_direction = static_cast<ax::mojom::TextDirection>(
      GetIntAttribute(ax::mojom::IntAttribute::kTextDirection));
  constexpr int kCaretThickness = 1;
  switch (text_direction) {
    case ax::mojom::TextDirection::kNone:
    case ax::mojom::TextDirection::kLtr: {
      bounds.set_width(kCaretThickness);
      break;
    }
    case ax::mojom::TextDirection::kRtl: {
      bounds.set_x(bounds.right() - kCaretThickness);
      bounds.set_width(kCaretThickness);
      break;
    }
    case ax::mojom::TextDirection::kTtb: {
      bounds.set_height(kCaretThickness);
      break;
    }
    case ax::mojom::TextDirection::kBtt: {
      bounds.set_y(bounds.bottom() - kCaretThickness);
      bounds.set_height(kCaretThickness);
      break;
    }
  }
  return bounds;
}

base::string16 BrowserAccessibility::GetValue() const {
  base::string16 value =
      GetString16Attribute(ax::mojom::StringAttribute::kValue);
  // Some screen readers like Jaws and VoiceOver require a value to be set in
  // text fields with rich content, even though the same information is
  // available on the children.
  if (value.empty() && IsRichTextField())
    return GetInnerText();
  return value;
}

BrowserAccessibility* BrowserAccessibility::ApproximateHitTest(
    const gfx::Point& point) {
  // The best result found that's a child of this object.
  BrowserAccessibility* child_result = nullptr;
  // The best result that's an indirect descendant like grandchild, etc.
  BrowserAccessibility* descendant_result = nullptr;

  // Walk the children recursively looking for the BrowserAccessibility that
  // most tightly encloses the specified point. Walk backwards so that in
  // the absence of any other information, we assume the object that occurs
  // later in the tree is on top of one that comes before it.
  for (int i = static_cast<int>(PlatformChildCount()) - 1; i >= 0; --i) {
    BrowserAccessibility* child = PlatformGetChild(i);

    // Skip table columns because cells are only contained in rows,
    // not columns.
    if (child->GetRole() == ax::mojom::Role::kColumn)
      continue;

    if (child->GetClippedScreenBoundsRect().Contains(point)) {
      BrowserAccessibility* result = child->ApproximateHitTest(point);
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
  node_ = nullptr;
  manager_ = nullptr;

  NativeReleaseReference();
}

void BrowserAccessibility::NativeReleaseReference() {
  delete this;
}

bool BrowserAccessibility::HasBoolAttribute(
    ax::mojom::BoolAttribute attribute) const {
  return GetData().HasBoolAttribute(attribute);
}

bool BrowserAccessibility::GetBoolAttribute(
    ax::mojom::BoolAttribute attribute) const {
  return GetData().GetBoolAttribute(attribute);
}

bool BrowserAccessibility::GetBoolAttribute(ax::mojom::BoolAttribute attribute,
                                            bool* value) const {
  return GetData().GetBoolAttribute(attribute, value);
}

bool BrowserAccessibility::HasFloatAttribute(
    ax::mojom::FloatAttribute attribute) const {
  return GetData().HasFloatAttribute(attribute);
}

float BrowserAccessibility::GetFloatAttribute(
    ax::mojom::FloatAttribute attribute) const {
  return GetData().GetFloatAttribute(attribute);
}

bool BrowserAccessibility::GetFloatAttribute(
    ax::mojom::FloatAttribute attribute,
    float* value) const {
  return GetData().GetFloatAttribute(attribute, value);
}

bool BrowserAccessibility::HasInheritedStringAttribute(
    ax::mojom::StringAttribute attribute) const {
  if (!instance_active())
    return false;

  if (GetData().HasStringAttribute(attribute))
    return true;
  return PlatformGetParent() &&
         PlatformGetParent()->HasInheritedStringAttribute(attribute);
}

const std::string& BrowserAccessibility::GetInheritedStringAttribute(
    ax::mojom::StringAttribute attribute) const {
  return node_->GetInheritedStringAttribute(attribute);
}

base::string16 BrowserAccessibility::GetInheritedString16Attribute(
    ax::mojom::StringAttribute attribute) const {
  return node_->GetInheritedString16Attribute(attribute);
}

bool BrowserAccessibility::HasIntAttribute(
    ax::mojom::IntAttribute attribute) const {
  return GetData().HasIntAttribute(attribute);
}

int BrowserAccessibility::GetIntAttribute(
    ax::mojom::IntAttribute attribute) const {
  return GetData().GetIntAttribute(attribute);
}

bool BrowserAccessibility::GetIntAttribute(ax::mojom::IntAttribute attribute,
                                           int* value) const {
  return GetData().GetIntAttribute(attribute, value);
}

bool BrowserAccessibility::HasStringAttribute(
    ax::mojom::StringAttribute attribute) const {
  return GetData().HasStringAttribute(attribute);
}

const std::string& BrowserAccessibility::GetStringAttribute(
    ax::mojom::StringAttribute attribute) const {
  return GetData().GetStringAttribute(attribute);
}

bool BrowserAccessibility::GetStringAttribute(
    ax::mojom::StringAttribute attribute,
    std::string* value) const {
  return GetData().GetStringAttribute(attribute, value);
}

base::string16 BrowserAccessibility::GetString16Attribute(
    ax::mojom::StringAttribute attribute) const {
  return GetData().GetString16Attribute(attribute);
}

bool BrowserAccessibility::GetString16Attribute(
    ax::mojom::StringAttribute attribute,
    base::string16* value) const {
  return GetData().GetString16Attribute(attribute, value);
}

bool BrowserAccessibility::HasIntListAttribute(
    ax::mojom::IntListAttribute attribute) const {
  return GetData().HasIntListAttribute(attribute);
}

const std::vector<int32_t>& BrowserAccessibility::GetIntListAttribute(
    ax::mojom::IntListAttribute attribute) const {
  return GetData().GetIntListAttribute(attribute);
}

bool BrowserAccessibility::GetIntListAttribute(
    ax::mojom::IntListAttribute attribute,
    std::vector<int32_t>* value) const {
  return GetData().GetIntListAttribute(attribute, value);
}

bool BrowserAccessibility::GetHtmlAttribute(const char* html_attr,
                                            std::string* value) const {
  return GetData().GetHtmlAttribute(html_attr, value);
}

bool BrowserAccessibility::GetHtmlAttribute(const char* html_attr,
                                            base::string16* value) const {
  return GetData().GetHtmlAttribute(html_attr, value);
}

base::string16 BrowserAccessibility::GetText() const {
  return GetInnerText();
}

bool BrowserAccessibility::HasState(ax::mojom::State state_enum) const {
  return GetData().HasState(state_enum);
}

bool BrowserAccessibility::HasAction(ax::mojom::Action action_enum) const {
  return GetData().HasAction(action_enum);
}

bool BrowserAccessibility::HasVisibleCaretOrSelection() const {
  int32_t focus_id = manager()->GetTreeData().sel_focus_object_id;
  BrowserAccessibility* focus_object = manager()->GetFromID(focus_id);
  if (!focus_object)
    return false;

  // Selection or caret will be visible in a focused editable area.
  if (HasState(ax::mojom::State::kEditable)) {
    return IsPlainTextField() ? focus_object == this
                              : focus_object->IsDescendantOf(this);
  }

  // The selection will be visible in non-editable content only if it is not
  // collapsed into a caret.
  return (focus_id != manager()->GetTreeData().sel_anchor_object_id ||
          manager()->GetTreeData().sel_focus_offset !=
              manager()->GetTreeData().sel_anchor_offset) &&
         focus_object->IsDescendantOf(this);
}

bool BrowserAccessibility::IsWebAreaForPresentationalIframe() const {
  if (GetRole() != ax::mojom::Role::kWebArea &&
      GetRole() != ax::mojom::Role::kRootWebArea) {
    return false;
  }

  BrowserAccessibility* parent = PlatformGetParent();
  if (!parent)
    return false;

  return parent->GetRole() == ax::mojom::Role::kIframePresentational;
}

bool BrowserAccessibility::IsClickable() const {
  return ui::IsClickable(GetData());
}

bool BrowserAccessibility::IsPlainTextField() const {
  // We need to check both the role and editable state, because some ARIA text
  // fields may in fact not be editable, whilst some editable fields might not
  // have the role.
  return !HasState(ax::mojom::State::kRichlyEditable) &&
         (GetRole() == ax::mojom::Role::kTextField ||
          GetRole() == ax::mojom::Role::kTextFieldWithComboBox ||
          GetRole() == ax::mojom::Role::kSearchBox ||
          GetBoolAttribute(ax::mojom::BoolAttribute::kEditableRoot));
}

bool BrowserAccessibility::IsRichTextField() const {
  return GetBoolAttribute(ax::mojom::BoolAttribute::kEditableRoot) &&
         HasState(ax::mojom::State::kRichlyEditable);
}

bool BrowserAccessibility::HasExplicitlyEmptyName() const {
  return GetData().GetNameFrom() ==
         ax::mojom::NameFrom::kAttributeExplicitlyEmpty;
}

std::string BrowserAccessibility::ComputeAccessibleNameFromDescendants() const {
  std::string name;
  for (size_t i = 0; i < InternalChildCount(); ++i) {
    BrowserAccessibility* child = InternalGetChild(i);
    std::string child_name;
    if (child->GetStringAttribute(ax::mojom::StringAttribute::kName,
                                  &child_name)) {
      if (!name.empty())
        name += " ";
      name += child_name;
    } else if (!child->HasState(ax::mojom::State::kFocusable)) {
      child_name = child->ComputeAccessibleNameFromDescendants();
      if (!child_name.empty()) {
        if (!name.empty())
          name += " ";
        name += child_name;
      }
    }
  }

  return name;
}

std::string BrowserAccessibility::GetLiveRegionText() const {
  if (GetRole() == ax::mojom::Role::kIgnored)
    return "";

  std::string text = GetStringAttribute(ax::mojom::StringAttribute::kName);
  if (!text.empty())
    return text;

  for (size_t i = 0; i < InternalChildCount(); ++i) {
    BrowserAccessibility* child = InternalGetChild(i);
    if (!child)
      continue;

    text += child->GetLiveRegionText();
  }
  return text;
}

std::vector<int> BrowserAccessibility::GetLineStartOffsets() const {
  if (!instance_active())
    return std::vector<int>();
  return node()->GetOrComputeLineStartOffsets();
}

BrowserAccessibilityPosition::AXPositionInstance
BrowserAccessibility::CreatePositionAt(int offset,
                                       ax::mojom::TextAffinity affinity) const {
  DCHECK(manager_);
  return BrowserAccessibilityPosition::CreateTextPosition(
      manager_->ax_tree_id(), GetId(), offset, affinity);
}

// |offset| could either be a text character or a child index in case of
// non-text objects.
// Currently, to be safe, we convert to text leaf equivalents and we don't use
// tree positions.
// TODO(nektar): Remove this function once selection fixes in Blink are
// thoroughly tested and convert to tree positions.
BrowserAccessibilityPosition::AXPositionInstance
BrowserAccessibility::CreatePositionForSelectionAt(int offset) const {
  BrowserAccessibilityPositionInstance position =
      CreatePositionAt(offset)->AsLeafTextPosition();
  if (position->GetAnchor() &&
      position->GetAnchor()->GetRole() == ax::mojom::Role::kInlineTextBox) {
    return position->CreateParentPosition();
  }
  return position;
}

base::string16 BrowserAccessibility::GetInnerText() const {
  if (IsTextOnlyObject())
    return GetString16Attribute(ax::mojom::StringAttribute::kName);

  base::string16 text;
  for (size_t i = 0; i < InternalChildCount(); ++i)
    text += InternalGetChild(i)->GetInnerText();
  return text;
}

gfx::Rect BrowserAccessibility::RelativeToAbsoluteBounds(
    gfx::RectF bounds,
    const ui::AXCoordinateSystem coordinate_system,
    const ui::AXClippingBehavior clipping_behavior,
    ui::AXOffscreenResult* offscreen_result) const {
  const bool clip_bounds =
      clipping_behavior == ui::AXClippingBehavior::kClipped;
  bool offscreen = false;
  const BrowserAccessibility* node = this;
  while (node) {
    bounds = node->manager()->ax_tree()->RelativeToTreeBounds(
        node->node(), bounds, &offscreen, clip_bounds);

    // On some platforms we need to unapply root scroll offsets.
    const BrowserAccessibility* root = node->manager()->GetRoot();
    if (!node->manager()->UseRootScrollOffsetsWhenComputingBounds() &&
        !root->PlatformGetParent()) {
      int sx = 0;
      int sy = 0;
      if (root->GetIntAttribute(ax::mojom::IntAttribute::kScrollX, &sx) &&
          root->GetIntAttribute(ax::mojom::IntAttribute::kScrollY, &sy)) {
        bounds.Offset(sx, sy);
      }
    }

    if (coordinate_system == ui::AXCoordinateSystem::kFrame)
      break;

    node = root->PlatformGetParent();
  }

  if (coordinate_system == ui::AXCoordinateSystem::kScreen)
    bounds.Offset(manager()->GetViewBounds().OffsetFromOrigin());

  if (offscreen_result) {
    *offscreen_result = offscreen ? ui::AXOffscreenResult::kOffscreen
                                  : ui::AXOffscreenResult::kOnscreen;
  }

  return gfx::ToEnclosingRect(bounds);
}

bool BrowserAccessibility::IsOffscreen() const {
  ui::AXOffscreenResult offscreen_result = ui::AXOffscreenResult::kOnscreen;
  RelativeToAbsoluteBounds(gfx::RectF(), ui::AXCoordinateSystem::kRootFrame,
                           ui::AXClippingBehavior::kClipped, &offscreen_result);
  return offscreen_result == ui::AXOffscreenResult::kOffscreen;
}

bool BrowserAccessibility::IsWebContent() const {
  return true;
}

std::set<ui::AXPlatformNode*> BrowserAccessibility::GetNodesForNodeIdSet(
    const std::set<int32_t>& ids) {
  std::set<ui::AXPlatformNode*> nodes;
  for (int32_t node_id : ids) {
    if (ui::AXPlatformNode* node = GetFromNodeID(node_id)) {
      nodes.insert(node);
    }
  }
  return nodes;
}

ui::AXPlatformNode* BrowserAccessibility::GetTargetNodeForRelation(
    ax::mojom::IntAttribute attr) {
  DCHECK(ui::IsNodeIdIntAttribute(attr));

  if (!node_)
    return nullptr;

  int target_id;
  if (!GetData().GetIntAttribute(attr, &target_id))
    return nullptr;

  return GetFromNodeID(target_id);
}

std::set<ui::AXPlatformNode*> BrowserAccessibility::GetTargetNodesForRelation(
    ax::mojom::IntListAttribute attr) {
  DCHECK(ui::IsNodeIdIntListAttribute(attr));

  std::vector<int32_t> target_ids;
  if (!GetIntListAttribute(attr, &target_ids))
    return std::set<ui::AXPlatformNode*>();

  std::set<int32_t> target_id_set(target_ids.begin(), target_ids.end());
  return GetNodesForNodeIdSet(target_id_set);
}

std::set<ui::AXPlatformNode*> BrowserAccessibility::GetReverseRelations(
    ax::mojom::IntAttribute attr) {
  DCHECK(manager_);
  DCHECK(node_);
  DCHECK(ui::IsNodeIdIntAttribute(attr));
  return GetNodesForNodeIdSet(
      manager_->ax_tree()->GetReverseRelations(attr, GetData().id));
}

std::set<ui::AXPlatformNode*> BrowserAccessibility::GetReverseRelations(
    ax::mojom::IntListAttribute attr) {
  DCHECK(manager_);
  DCHECK(node_);
  DCHECK(ui::IsNodeIdIntListAttribute(attr));
  return GetNodesForNodeIdSet(
      manager_->ax_tree()->GetReverseRelations(attr, GetData().id));
}

const ui::AXUniqueId& BrowserAccessibility::GetUniqueId() const {
  // This is not the same as GetData().id which comes from Blink, because
  // those ids are only unique within the Blink process. We need one that is
  // unique for the browser process.
  return unique_id_;
}

base::Optional<int> BrowserAccessibility::FindTextBoundary(
    ui::TextBoundaryType boundary_type,
    int offset,
    ui::TextBoundaryDirection direction,
    ax::mojom::TextAffinity affinity) const {
  switch (boundary_type) {
    case ui::WORD_BOUNDARY: {
      BrowserAccessibilityPositionInstance position =
          CreatePositionAt(static_cast<int>(offset), affinity);
      switch (direction) {
        case ui::BACKWARDS_DIRECTION:
          return position
              ->CreatePreviousWordStartPosition(
                  ui::AXBoundaryBehavior::StopIfAlreadyAtBoundary)
              ->text_offset();
        case ui::FORWARDS_DIRECTION:
          return position
              ->CreateNextWordStartPosition(
                  ui::AXBoundaryBehavior::StopAtAnchorBoundary)
              ->text_offset();
      }
    }
    case ui::LINE_BOUNDARY: {
      BrowserAccessibilityPositionInstance position =
          CreatePositionAt(static_cast<int>(offset), affinity);
      switch (direction) {
        case ui::BACKWARDS_DIRECTION:
          return position
              ->CreatePreviousLineStartPosition(
                  ui::AXBoundaryBehavior::StopIfAlreadyAtBoundary)
              ->text_offset();
        case ui::FORWARDS_DIRECTION:
          return position
              ->CreateNextLineStartPosition(
                  ui::AXBoundaryBehavior::StopAtAnchorBoundary)
              ->text_offset();
      }
    }
    case ui::CHAR_BOUNDARY:
    case ui::SENTENCE_BOUNDARY:
    case ui::PARAGRAPH_BOUNDARY:
    case ui::ALL_BOUNDARY:
      // TODO(nektar): |AXPosition| can handle other types of boundaries as
      // well.
      return ui::FindAccessibleTextBoundary(GetText(), GetLineStartOffsets(),
                                            boundary_type, offset, direction,
                                            affinity);
  }
}

const std::vector<gfx::NativeViewAccessible>
BrowserAccessibility::GetDescendants() const {
  std::vector<gfx::NativeViewAccessible> descendants;
  if (PlatformChildCount() > 0) {
    BrowserAccessibility* next_sibling_node = GetNextSibling();
    BrowserAccessibility* next_descendant_node =
        BrowserAccessibilityManager::NextInTreeOrder(this);
    while (next_descendant_node && next_descendant_node != next_sibling_node) {
      descendants.emplace_back(next_descendant_node->GetNativeViewAccessible());
      next_descendant_node =
          BrowserAccessibilityManager::NextInTreeOrder(next_descendant_node);
    }
  }
  return descendants;
}

gfx::NativeViewAccessible BrowserAccessibility::GetNativeViewAccessible() {
  // TODO(703369) On Windows, where we have started to migrate to an
  // AXPlatformNode implementation, the BrowserAccessibilityWin subclass has
  // overridden this method. On all other platforms, this method should not be
  // called yet. In the future, when all subclasses have moved over to be
  // implemented by AXPlatformNode, we may make this method completely virtual.
  NOTREACHED();
  return nullptr;
}

//
// AXPlatformNodeDelegate.
//
const ui::AXNodeData& BrowserAccessibility::GetData() const {
  static base::NoDestructor<ui::AXNodeData> empty_data;
  if (node_)
    return node_->data();
  else
    return *empty_data;
}

const ui::AXTreeData& BrowserAccessibility::GetTreeData() const {
  static base::NoDestructor<ui::AXTreeData> empty_data;
  if (manager())
    return manager()->GetTreeData();
  else
    return *empty_data;
}

ui::AXNodePosition::AXPositionInstance
BrowserAccessibility::CreateTextPositionAt(
    int offset,
    ax::mojom::TextAffinity affinity) const {
  DCHECK(manager_);
  return ui::AXNodePosition::CreateTextPosition(manager_->ax_tree_id(), GetId(),
                                                offset, affinity);
}

gfx::NativeViewAccessible BrowserAccessibility::GetNSWindow() {
  NOTREACHED();
  return nullptr;
}

gfx::NativeViewAccessible BrowserAccessibility::GetParent() {
  auto* parent = PlatformGetParent();
  if (parent)
    return parent->GetNativeViewAccessible();

  if (!manager_)
    return nullptr;

  BrowserAccessibilityDelegate* delegate =
      manager_->GetDelegateFromRootManager();
  if (!delegate)
    return nullptr;

  return delegate->AccessibilityGetNativeViewAccessible();
}

int BrowserAccessibility::GetChildCount() {
  return PlatformChildCount();
}

gfx::NativeViewAccessible BrowserAccessibility::ChildAtIndex(int index) {
  auto* child = PlatformGetChild(index);
  if (!child)
    return nullptr;

  return child->GetNativeViewAccessible();
}

gfx::NativeViewAccessible BrowserAccessibility::HitTestSync(int x, int y) {
  auto* accessible = manager_->CachingAsyncHitTest(gfx::Point(x, y));
  if (!accessible)
    return nullptr;

  return accessible->GetNativeViewAccessible();
}

gfx::NativeViewAccessible BrowserAccessibility::GetFocus() {
  auto* focused = manager()->GetFocus();
  if (!focused)
    return nullptr;

  return focused->GetNativeViewAccessible();
}

ui::AXPlatformNode* BrowserAccessibility::GetFromNodeID(int32_t id) {
  // Not all BrowserAccessibility subclasses can return an AXPlatformNode yet.
  // So, here we just return nullptr.
  return nullptr;
}

int BrowserAccessibility::GetIndexInParent() const {
  return node_ ? node_->index_in_parent() : -1;
}

gfx::AcceleratedWidget
BrowserAccessibility::GetTargetForNativeAccessibilityEvent() {
  BrowserAccessibilityDelegate* root_delegate =
      manager()->GetDelegateFromRootManager();
  if (!root_delegate)
    return gfx::kNullAcceleratedWidget;
  return root_delegate->AccessibilityGetAcceleratedWidget();
}

bool BrowserAccessibility::IsTable() const {
  return node()->IsTable();
}

int32_t BrowserAccessibility::GetTableRowCount() const {
  return node()->GetTableRowCount();
}

int32_t BrowserAccessibility::GetTableColCount() const {
  return node()->GetTableColCount();
}

base::Optional<int32_t> BrowserAccessibility::GetTableAriaColCount() const {
  return node()->GetTableAriaColCount();
}

base::Optional<int32_t> BrowserAccessibility::GetTableAriaRowCount() const {
  return node()->GetTableAriaRowCount();
}

int32_t BrowserAccessibility::GetTableCellCount() const {
  return node()->GetTableCellCount();
}

const std::vector<int32_t> BrowserAccessibility::GetColHeaderNodeIds() const {
  std::vector<int32_t> result;
  node()->GetTableCellColHeaderNodeIds(&result);
  return result;
}

const std::vector<int32_t> BrowserAccessibility::GetColHeaderNodeIds(
    int32_t col_index) const {
  std::vector<int32_t> result;
  node()->GetTableColHeaderNodeIds(col_index, &result);
  return result;
}

const std::vector<int32_t> BrowserAccessibility::GetRowHeaderNodeIds() const {
  std::vector<int32_t> result;
  node()->GetTableCellRowHeaderNodeIds(&result);
  return result;
}

const std::vector<int32_t> BrowserAccessibility::GetRowHeaderNodeIds(
    int32_t row_index) const {
  std::vector<int32_t> result;
  node()->GetTableRowHeaderNodeIds(row_index, &result);
  return result;
}

ui::AXPlatformNode* BrowserAccessibility::GetTableCaption() {
  if (ui::AXNode* caption = node()->GetTableCaption())
    return GetFromNodeID(caption->id());

  return nullptr;
}

bool BrowserAccessibility::IsTableRow() const {
  return node()->IsTableRow();
}

int32_t BrowserAccessibility::GetTableRowRowIndex() const {
  return node()->GetTableRowRowIndex();
}

bool BrowserAccessibility::IsTableCellOrHeader() const {
  return node()->IsTableCellOrHeader();
}

int32_t BrowserAccessibility::GetTableCellColIndex() const {
  return node()->GetTableCellColIndex();
}

int32_t BrowserAccessibility::GetTableCellRowIndex() const {
  return node()->GetTableCellRowIndex();
}

int32_t BrowserAccessibility::GetTableCellColSpan() const {
  return node()->GetTableCellColSpan();
}

int32_t BrowserAccessibility::GetTableCellRowSpan() const {
  return node()->GetTableCellRowSpan();
}

int32_t BrowserAccessibility::GetTableCellAriaColIndex() const {
  return node()->GetTableCellAriaColIndex();
}

int32_t BrowserAccessibility::GetTableCellAriaRowIndex() const {
  return node()->GetTableCellAriaRowIndex();
}

int32_t BrowserAccessibility::GetCellId(int32_t row_index,
                                        int32_t col_index) const {
  ui::AXNode* cell = node()->GetTableCellFromCoords(row_index, col_index);
  if (cell)
    return cell->id();

  return -1;
}

int32_t BrowserAccessibility::GetTableCellIndex() const {
  return node()->GetTableCellIndex();
}

int32_t BrowserAccessibility::CellIndexToId(int32_t cell_index) const {
  ui::AXNode* cell = node()->GetTableCellFromIndex(cell_index);
  if (cell)
    return cell->id();
  return -1;
}

bool BrowserAccessibility::IsCellOrHeaderOfARIATable() const {
  return node()->IsCellOrHeaderOfARIATable();
}

bool BrowserAccessibility::IsCellOrHeaderOfARIAGrid() const {
  return node()->IsCellOrHeaderOfARIAGrid();
}

bool BrowserAccessibility::AccessibilityPerformAction(
    const ui::AXActionData& data) {
  switch (data.action) {
    case ax::mojom::Action::kDoDefault:
      manager_->DoDefaultAction(*this);
      return true;
    case ax::mojom::Action::kFocus:
      manager_->SetFocus(*this);
      return true;
    case ax::mojom::Action::kScrollToPoint: {
      // target_point is in screen coordinates.  We need to convert this to
      // frame coordinates because that's what BrowserAccessiblity cares about.
      gfx::Point target =
          data.target_point -
          manager_->GetRootManager()->GetViewBounds().OffsetFromOrigin();

      manager_->ScrollToPoint(*this, target);
      return true;
    }
    case ax::mojom::Action::kScrollToMakeVisible:
      manager_->ScrollToMakeVisible(*this, data.target_rect);
      return true;
    case ax::mojom::Action::kSetScrollOffset:
      manager_->SetScrollOffset(*this, data.target_point);
      return true;
    case ax::mojom::Action::kSetSelection:
      manager_->SetSelection(data);
      return true;
    case ax::mojom::Action::kSetValue:
      manager_->SetValue(*this, data.value);
      return true;
    case ax::mojom::Action::kShowContextMenu:
      manager_->ShowContextMenu(*this);
      return true;
    default:
      return false;
  }
}

base::string16 BrowserAccessibility::GetLocalizedStringForImageAnnotationStatus(
    ax::mojom::ImageAnnotationStatus status) const {
  const ContentClient* content_client = content::GetContentClient();

  int message_id = 0;
  switch (status) {
    case ax::mojom::ImageAnnotationStatus::kEligibleForAnnotation:
      message_id = IDS_AX_IMAGE_ELIGIBLE_FOR_ANNOTATION;
      break;
    case ax::mojom::ImageAnnotationStatus::kAnnotationPending:
      message_id = IDS_AX_IMAGE_ANNOTATION_PENDING;
      break;
    case ax::mojom::ImageAnnotationStatus::kAnnotationAdult:
      message_id = IDS_AX_IMAGE_ANNOTATION_ADULT;
      break;
    case ax::mojom::ImageAnnotationStatus::kAnnotationEmpty:
    case ax::mojom::ImageAnnotationStatus::kAnnotationProcessFailed:
      message_id = IDS_AX_IMAGE_ANNOTATION_NO_DESCRIPTION;
      break;
    case ax::mojom::ImageAnnotationStatus::kNone:
    case ax::mojom::ImageAnnotationStatus::kIneligibleForAnnotation:
    case ax::mojom::ImageAnnotationStatus::kAnnotationSucceeded:
      return base::string16();
  }

  DCHECK(message_id);

  return content_client->GetLocalizedString(message_id);
}

base::string16
BrowserAccessibility::GetLocalizedRoleDescriptionForUnlabeledImage() const {
  const ContentClient* content_client = content::GetContentClient();
  return content_client->GetLocalizedString(
      IDS_AX_UNLABELED_IMAGE_ROLE_DESCRIPTION);
}

base::string16 BrowserAccessibility::GetLocalizedStringForLandmarkType() const {
  const ContentClient* content_client = content::GetContentClient();
  const ui::AXNodeData& data = GetData();

  switch (data.role) {
    case ax::mojom::Role::kBanner:
      return content_client->GetLocalizedString(IDS_AX_ROLE_BANNER);

    case ax::mojom::Role::kComplementary:
      return content_client->GetLocalizedString(IDS_AX_ROLE_COMPLEMENTARY);

    case ax::mojom::Role::kContentInfo:
    case ax::mojom::Role::kFooter:
      return content_client->GetLocalizedString(IDS_AX_ROLE_CONTENT_INFO);

    case ax::mojom::Role::kRegion:
      if (data.HasStringAttribute(ax::mojom::StringAttribute::kName))
        return content_client->GetLocalizedString(IDS_AX_ROLE_REGION);
      FALLTHROUGH;

    default:
      return {};
  }
}

base::string16 BrowserAccessibility::GetStyleNameAttributeAsLocalizedString()
    const {
  const BrowserAccessibility* current_node = this;
  while (current_node) {
    if (current_node->GetData().role == ax::mojom::Role::kMark) {
      const ContentClient* content_client = content::GetContentClient();
      return content_client->GetLocalizedString(IDS_AX_ROLE_MARK);
    }
    current_node = current_node->PlatformGetParent();
  }
  return {};
}

bool BrowserAccessibility::ShouldIgnoreHoveredStateForTesting() {
  BrowserAccessibilityStateImpl* accessibility_state =
      BrowserAccessibilityStateImpl::GetInstance();
  return accessibility_state->disable_hot_tracking_for_testing();
}

bool BrowserAccessibility::IsOrderedSetItem() const {
  return node()->IsOrderedSetItem();
}

bool BrowserAccessibility::IsOrderedSet() const {
  return node()->IsOrderedSet();
}

int32_t BrowserAccessibility::GetPosInSet() const {
  return node()->GetPosInSet();
}

int32_t BrowserAccessibility::GetSetSize() const {
  return node()->GetSetSize();
}

}  // namespace content
