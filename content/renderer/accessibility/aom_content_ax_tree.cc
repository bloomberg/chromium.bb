// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/accessibility/aom_content_ax_tree.h"

#include <string>

#include "content/common/ax_content_node_data.h"
#include "content/renderer/accessibility/render_accessibility_impl.h"
#include "ui/accessibility/ax_enum_util.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node.h"

namespace {

ax::mojom::IntAttribute GetCorrespondingAXAttribute(
    blink::WebAOMIntAttribute attr) {
  switch (attr) {
    case blink::WebAOMIntAttribute::AOM_ATTR_COLUMN_COUNT:
      return ax::mojom::IntAttribute::kTableColumnCount;
    case blink::WebAOMIntAttribute::AOM_ATTR_COLUMN_INDEX:
      return ax::mojom::IntAttribute::kTableColumnIndex;
    case blink::WebAOMIntAttribute::AOM_ATTR_COLUMN_SPAN:
      return ax::mojom::IntAttribute::kTableCellColumnSpan;
    case blink::WebAOMIntAttribute::AOM_ATTR_HIERARCHICAL_LEVEL:
      return ax::mojom::IntAttribute::kHierarchicalLevel;
    case blink::WebAOMIntAttribute::AOM_ATTR_POS_IN_SET:
      return ax::mojom::IntAttribute::kPosInSet;
    case blink::WebAOMIntAttribute::AOM_ATTR_ROW_COUNT:
      return ax::mojom::IntAttribute::kTableRowCount;
    case blink::WebAOMIntAttribute::AOM_ATTR_ROW_INDEX:
      return ax::mojom::IntAttribute::kTableRowIndex;
    case blink::WebAOMIntAttribute::AOM_ATTR_ROW_SPAN:
      return ax::mojom::IntAttribute::kTableCellRowSpan;
    case blink::WebAOMIntAttribute::AOM_ATTR_SET_SIZE:
      return ax::mojom::IntAttribute::kSetSize;
    default:
      return ax::mojom::IntAttribute::kNone;
  }
}

ax::mojom::StringAttribute GetCorrespondingAXAttribute(
    blink::WebAOMStringAttribute attr) {
  switch (attr) {
    case blink::WebAOMStringAttribute::AOM_ATTR_KEY_SHORTCUTS:
      return ax::mojom::StringAttribute::kKeyShortcuts;
    case blink::WebAOMStringAttribute::AOM_ATTR_NAME:
      return ax::mojom::StringAttribute::kName;
    case blink::WebAOMStringAttribute::AOM_ATTR_PLACEHOLDER:
      return ax::mojom::StringAttribute::kPlaceholder;
    case blink::WebAOMStringAttribute::AOM_ATTR_ROLE_DESCRIPTION:
      return ax::mojom::StringAttribute::kRoleDescription;
    case blink::WebAOMStringAttribute::AOM_ATTR_VALUE_TEXT:
      return ax::mojom::StringAttribute::kValue;
    default:
      return ax::mojom::StringAttribute::kNone;
  }
}

}  // namespace

namespace content {

AomContentAxTree::AomContentAxTree(RenderFrameImpl* render_frame)
    : render_frame_(render_frame) {}

bool AomContentAxTree::ComputeAccessibilityTree() {
  AXContentTreeUpdate content_tree_update;
  RenderAccessibilityImpl::SnapshotAccessibilityTree(render_frame_,
                                                     &content_tree_update);

  // Hack to convert between AXContentNodeData and AXContentTreeData to just
  // AXNodeData and AXTreeData to preserve content specific attributes while
  // still being able to use AXTree's Unserialize method.
  ui::AXTreeUpdate tree_update;
  tree_update.has_tree_data = content_tree_update.has_tree_data;
  ui::AXTreeData* tree_data = &(content_tree_update.tree_data);
  tree_update.tree_data = *tree_data;
  tree_update.node_id_to_clear = content_tree_update.node_id_to_clear;
  tree_update.root_id = content_tree_update.root_id;
  tree_update.nodes.assign(content_tree_update.nodes.begin(),
                           content_tree_update.nodes.end());
  return tree_.Unserialize(tree_update);
}

bool AomContentAxTree::GetRoleForAXNode(int32_t ax_id,
                                        blink::WebString* out_param) {
  ui::AXNode* node = tree_.GetFromId(ax_id);
  if (!node)
    return false;
  *out_param = blink::WebString::FromUTF8(ui::ToString(node->data().role));
  return true;
}

bool AomContentAxTree::GetStringAttributeForAXNode(
    int32_t ax_id,
    blink::WebAOMStringAttribute attr,
    blink::WebString* out_param) {
  ui::AXNode* node = tree_.GetFromId(ax_id);
  std::string out_string;

  if (node && node->data().GetStringAttribute(GetCorrespondingAXAttribute(attr),
                                              &out_string)) {
    *out_param = blink::WebString::FromUTF8(out_string.c_str());
    return true;
  }

  return false;
}

bool AomContentAxTree::GetIntAttributeForAXNode(int32_t ax_id,
                                                blink::WebAOMIntAttribute attr,
                                                int32_t* out_param) {
  ui::AXNode* node = tree_.GetFromId(ax_id);
  if (!node)
    return false;
  ax::mojom::IntAttribute ax_attr = GetCorrespondingAXAttribute(attr);
  return node->data().GetIntAttribute(ax_attr, out_param);
}

bool AomContentAxTree::GetParentIdForAXNode(int32_t ax_id, int32_t* out_param) {
  ui::AXNode* node = tree_.GetFromId(ax_id);
  if (!node || !node->parent())
    return false;
  *out_param = node->parent()->id();
  return true;
}

bool AomContentAxTree::GetFirstChildIdForAXNode(int32_t ax_id,
                                                int32_t* out_param) {
  ui::AXNode* node = tree_.GetFromId(ax_id);
  if (!node || !node->child_count())
    return false;

  ui::AXNode* child = node->ChildAtIndex(0);
  DCHECK(child);
  *out_param = child->id();
  return true;
}

bool AomContentAxTree::GetLastChildIdForAXNode(int32_t ax_id,
                                               int32_t* out_param) {
  ui::AXNode* node = tree_.GetFromId(ax_id);
  if (!node || !node->child_count())
    return false;

  ui::AXNode* child = node->ChildAtIndex(node->child_count() - 1);
  DCHECK(child);
  *out_param = child->id();
  return true;
}

bool AomContentAxTree::GetPreviousSiblingIdForAXNode(int32_t ax_id,
                                                     int32_t* out_param) {
  ui::AXNode* node = tree_.GetFromId(ax_id);
  if (!node)
    return false;
  int index_in_parent = node->index_in_parent();

  // Assumption: only when this node is the first child, does it not have a
  // previous sibling.
  if (index_in_parent == 0)
    return false;

  ui::AXNode* sibling = node->parent()->ChildAtIndex(index_in_parent - 1);
  DCHECK(sibling);
  *out_param = sibling->id();
  return true;
}

bool AomContentAxTree::GetNextSiblingIdForAXNode(int32_t ax_id,
                                                 int32_t* out_param) {
  ui::AXNode* node = tree_.GetFromId(ax_id);
  if (!node)
    return false;
  int index_in_parent = node->index_in_parent();

  // Assumption: When this node is the last child, it does not have a next
  // sibling.
  if (index_in_parent == (node->parent()->child_count() - 1))
    return false;

  ui::AXNode* sibling = node->parent()->ChildAtIndex(index_in_parent + 1);
  DCHECK(sibling);
  *out_param = sibling->id();
  return true;
}

}  // namespace content
