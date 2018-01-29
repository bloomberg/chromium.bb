// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/accessibility/aom_content_ax_tree.h"

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

blink::WebString AomContentAxTree::GetNameForAXNode(int32_t axID) {
  ui::AXNode* node = tree_.GetFromId(axID);
  return (node) ? blink::WebString::FromUTF8(node->data().GetStringAttribute(
                      ax::mojom::StringAttribute::kName))
                : blink::WebString();
}

blink::WebString AomContentAxTree::GetRoleForAXNode(int32_t axID) {
  ui::AXNode* node = tree_.GetFromId(axID);
  // TODO(meredithl): Change to blink_ax_conversion.cc method once available.
  return (node) ? blink::WebString::FromUTF8(ui::ToString(node->data().role))
                : blink::WebString();
}

bool AomContentAxTree::GetIntAttributeForAXNode(int32_t axID,
                                                blink::WebAOMIntAttribute attr,
                                                int32_t* out_param) {
  ui::AXNode* node = tree_.GetFromId(axID);
  if (!node)
    return false;
  ax::mojom::IntAttribute ax_attr = GetCorrespondingAXAttribute(attr);
  return node->data().GetIntAttribute(ax_attr, out_param);
}

}  // namespace content
