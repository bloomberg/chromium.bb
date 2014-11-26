// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_ACCESSIBILITY_BLINK_AX_TREE_SOURCE_H_
#define CONTENT_RENDERER_ACCESSIBILITY_BLINK_AX_TREE_SOURCE_H_

#include "third_party/WebKit/public/web/WebAXObject.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/accessibility/ax_tree_source.h"

namespace content {

class RenderFrameImpl;

class BlinkAXTreeSource
    : public ui::AXTreeSource<blink::WebAXObject> {
 public:
  BlinkAXTreeSource(RenderFrameImpl* render_frame);
  ~BlinkAXTreeSource() override;

  // Call this to have BlinkAXTreeSource collect a mapping from
  // node ids to the frame routing id for an out-of-process iframe during
  // calls to SerializeNode.
  void CollectChildFrameIdMapping(
      std::map<int32, int>* node_to_frame_routing_id_map,
      std::map<int32, int>* node_to_browser_plugin_instance_id_map);

  // Walks up the ancestor chain to see if this is a descendant of the root.
  bool IsInTree(blink::WebAXObject node) const;

  // Set the id of the node with accessibility focus. The node with
  // accessibility focus will force loading inline text box children,
  // which aren't always loaded by default on all platforms.
  int accessibility_focus_id() { return accessibility_focus_id_; }
  void set_accessiblity_focus_id(int id) { accessibility_focus_id_ = id; }

  // AXTreeSource implementation.
  blink::WebAXObject GetRoot() const override;
  blink::WebAXObject GetFromId(int32 id) const override;
  int32 GetId(blink::WebAXObject node) const override;
  void GetChildren(
      blink::WebAXObject node,
      std::vector<blink::WebAXObject>* out_children) const override;
  blink::WebAXObject GetParent(blink::WebAXObject node) const override;
  void SerializeNode(blink::WebAXObject node,
                     ui::AXNodeData* out_data) const override;
  bool IsValid(blink::WebAXObject node) const override;
  bool IsEqual(blink::WebAXObject node1,
               blink::WebAXObject node2) const override;
  blink::WebAXObject GetNull() const override;

  blink::WebDocument GetMainDocument() const;

 private:
  RenderFrameImpl* render_frame_;
  std::map<int32, int>* node_to_frame_routing_id_map_;
  std::map<int32, int>* node_to_browser_plugin_instance_id_map_;
  int accessibility_focus_id_;
};

}  // namespace content

#endif  // CONTENT_RENDERER_ACCESSIBILITY_BLINK_AX_TREE_SOURCE_H_
