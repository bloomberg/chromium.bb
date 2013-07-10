// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_contents/frame_tree_node.h"

#include <queue>

#include "base/stl_util.h"

namespace content {

FrameTreeNode::FrameTreeNode(int64 frame_id, const std::string& name)
  : frame_id_(frame_id),
    frame_name_(name) {
}

FrameTreeNode::~FrameTreeNode() {
}

void FrameTreeNode::AddChild(FrameTreeNode* child) {
  children_.push_back(child);
}

void FrameTreeNode::RemoveChild(int64 child_id) {
  std::vector<FrameTreeNode*>::iterator iter;

  for (iter = children_.begin(); iter != children_.end(); ++iter) {
    if ((*iter)->frame_id() == child_id)
      break;
  }

  if (iter != children_.end())
    children_.erase(iter);
}

}  // namespace content
