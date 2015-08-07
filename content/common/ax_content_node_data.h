// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_AX_CONTENT_NODE_DATA_H_
#define CONTENT_COMMON_AX_CONTENT_NODE_DATA_H_

#include "content/common/content_export.h"
#include "ui/accessibility/ax_node_data.h"

namespace content {

enum AXContentIntAttribute {
  // The routing ID of this root node.
  AX_CONTENT_ATTR_ROUTING_ID,

  // The routing ID of this tree's parent.
  AX_CONTENT_ATTR_PARENT_ROUTING_ID,

  // The routing ID of this node's child tree.
  AX_CONTENT_ATTR_CHILD_ROUTING_ID,

  // The browser plugin instance ID of this node's child tree.
  AX_CONTENT_ATTR_CHILD_BROWSER_PLUGIN_INSTANCE_ID,

  AX_CONTENT_INT_ATTRIBUTE_LAST
};

// A subclass of AXNodeData that contains extra fields for
// content-layer-specific AX attributes.
struct CONTENT_EXPORT AXContentNodeData : public ui::AXNodeData {
  AXContentNodeData();
  ~AXContentNodeData() override;

  bool HasContentIntAttribute(AXContentIntAttribute attribute) const;
  int GetContentIntAttribute(AXContentIntAttribute attribute) const;
  bool GetContentIntAttribute(AXContentIntAttribute attribute,
                              int* value) const;
  void AddContentIntAttribute(AXContentIntAttribute attribute, int value);

  // Return a string representation of this data, for debugging.
  std::string ToString() const override;

  // This is a simple serializable struct. All member variables should be
  // public and copyable.
  std::vector<std::pair<AXContentIntAttribute, int32> > content_int_attributes;
};

}  // namespace content

#endif  // CONTENT_COMMON_AX_CONTENT_NODE_DATA_H_
