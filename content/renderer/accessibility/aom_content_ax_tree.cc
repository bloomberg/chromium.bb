// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/accessibility/aom_content_ax_tree.h"

#include "content/common/ax_content_node_data.h"
#include "content/renderer/accessibility/render_accessibility_impl.h"
#include "ui/accessibility/ax_enums.h"
#include "ui/accessibility/ax_node.h"

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
  return (node) ? blink::WebString::FromUTF8(
                      node->data().GetStringAttribute(ui::AX_ATTR_NAME))
                : blink::WebString();
}

blink::WebString AomContentAxTree::GetRoleForAXNode(int32_t axID) {
  ui::AXNode* node = tree_.GetFromId(axID);
  // TODO(meredithl): Change to blink_ax_conversion.cc method once available.
  return (node) ? blink::WebString::FromUTF8(ui::ToString(node->data().role))
                : blink::WebString();
}

}  // namespace content
