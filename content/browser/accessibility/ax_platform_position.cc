// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/accessibility/ax_platform_position.h"

#include "content/browser/accessibility/browser_accessibility.h"
#include "content/browser/accessibility/browser_accessibility_manager.h"
#include "ui/accessibility/ax_enums.h"

namespace content {

AXPlatformPosition::AXPlatformPosition() {}

AXPlatformPosition::~AXPlatformPosition() {}

AXPlatformPosition::AXPositionInstance AXPlatformPosition::Clone() const {
  return AXPositionInstance(new AXPlatformPosition(*this));
}

base::string16 AXPlatformPosition::GetInnerText() const {
  if (IsNullPosition())
    return base::string16();
  DCHECK(GetAnchor());
  return GetAnchor()->GetText();
}

void AXPlatformPosition::AnchorChild(int child_index,
                                     AXTreeID* tree_id,
                                     int32_t* child_id) const {
  DCHECK(tree_id);
  DCHECK(child_id);

  if (!GetAnchor() || child_index < 0 || child_index >= AnchorChildCount()) {
    *tree_id = INVALID_TREE_ID;
    *child_id = INVALID_ANCHOR_ID;
    return;
  }

  BrowserAccessibility* child = nullptr;
  if (GetAnchor()->PlatformIsLeaf()) {
    child = GetAnchor()->InternalGetChild(child_index);
  } else {
    child = GetAnchor()->PlatformGetChild(child_index);
  }
  DCHECK(child);
  *tree_id = child->manager()->ax_tree_id();
  *child_id = child->GetId();
}

int AXPlatformPosition::AnchorChildCount() const {
  if (!GetAnchor())
    return 0;

  if (GetAnchor()->PlatformIsLeaf()) {
    return static_cast<int>(GetAnchor()->InternalChildCount());
  } else {
    return static_cast<int>(GetAnchor()->PlatformChildCount());
  }
}

int AXPlatformPosition::AnchorIndexInParent() const {
  return GetAnchor() ? static_cast<int>(GetAnchor()->GetIndexInParent())
                     : AXPosition::INVALID_INDEX;
}

void AXPlatformPosition::AnchorParent(AXTreeID* tree_id,
                                      int32_t* parent_id) const {
  DCHECK(tree_id);
  DCHECK(parent_id);

  if (!GetAnchor() || !GetAnchor()->GetParent()) {
    *tree_id = AXPosition::INVALID_TREE_ID;
    *parent_id = AXPosition::INVALID_ANCHOR_ID;
    return;
  }

  BrowserAccessibility* parent = GetAnchor()->GetParent();
  *tree_id = parent->manager()->ax_tree_id();
  *parent_id = parent->GetId();
}

BrowserAccessibility* AXPlatformPosition::GetNodeInTree(AXTreeID tree_id,
                                                        int32_t node_id) const {
  if (tree_id == AXPosition::INVALID_TREE_ID ||
      node_id == AXPosition::INVALID_ANCHOR_ID) {
    return nullptr;
  }

  auto* manager = BrowserAccessibilityManager::FromID(tree_id);
  if (!manager)
    return nullptr;
  return manager->GetFromID(node_id);
}

int AXPlatformPosition::MaxTextOffset() const {
  if (IsNullPosition())
    return INVALID_OFFSET;
  return static_cast<int>(GetInnerText().length());
}

bool AXPlatformPosition::IsInLineBreak() const {
  if (IsNullPosition())
    return false;

  DCHECK(GetAnchor());
  return GetAnchor()->IsLineBreakObject();
}

std::vector<int32_t> AXPlatformPosition::GetWordStartOffsets() const {
  if (IsNullPosition())
    return std::vector<int32_t>();
  DCHECK(GetAnchor());
  return GetAnchor()->GetIntListAttribute(ui::AX_ATTR_WORD_STARTS);
}

std::vector<int32_t> AXPlatformPosition::GetWordEndOffsets() const {
  if (IsNullPosition())
    return std::vector<int32_t>();
  DCHECK(GetAnchor());
  return GetAnchor()->GetIntListAttribute(ui::AX_ATTR_WORD_ENDS);
}

int32_t AXPlatformPosition::GetNextOnLineID(int32_t node_id) const {
  if (IsNullPosition())
    return INVALID_ANCHOR_ID;
  BrowserAccessibility* node = GetNodeInTree(tree_id(), node_id);
  int next_on_line_id;
  if (!node ||
      !node->GetIntAttribute(ui::AX_ATTR_NEXT_ON_LINE_ID, &next_on_line_id)) {
    return INVALID_ANCHOR_ID;
  }
  return static_cast<int32_t>(next_on_line_id);
}

int32_t AXPlatformPosition::GetPreviousOnLineID(int32_t node_id) const {
  if (IsNullPosition())
    return INVALID_ANCHOR_ID;
  BrowserAccessibility* node = GetNodeInTree(tree_id(), node_id);
  int previous_on_line_id;
  if (!node ||
      !node->GetIntAttribute(ui::AX_ATTR_PREVIOUS_ON_LINE_ID,
                             &previous_on_line_id)) {
    return INVALID_ANCHOR_ID;
  }
  return static_cast<int32_t>(previous_on_line_id);
}

}  // namespace content
