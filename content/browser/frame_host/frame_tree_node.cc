// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/frame_host/frame_tree_node.h"

#include <queue>

#include "base/stl_util.h"
#include "content/browser/frame_host/navigator.h"
#include "content/browser/frame_host/render_frame_host_impl.h"

namespace content {

const int64 FrameTreeNode::kInvalidFrameId = -1;
int64 FrameTreeNode::next_frame_tree_node_id_ = 1;

FrameTreeNode::FrameTreeNode(Navigator* navigator,
                             RenderFrameHostDelegate* render_frame_delegate,
                             RenderViewHostDelegate* render_view_delegate,
                             RenderWidgetHostDelegate* render_widget_delegate,
                             RenderFrameHostManager::Delegate* manager_delegate,
                             int64 frame_id,
                             const std::string& name)
  : navigator_(navigator),
    render_manager_(render_frame_delegate,
                    render_view_delegate,
                    render_widget_delegate,
                    manager_delegate),
    frame_tree_node_id_(next_frame_tree_node_id_++),
    frame_id_(frame_id),
    frame_name_(name),
    owns_render_frame_host_(true),
    render_frame_host_(NULL) {
}

FrameTreeNode::~FrameTreeNode() {
  if (owns_render_frame_host_)
    delete render_frame_host_;
}

void FrameTreeNode::AddChild(scoped_ptr<FrameTreeNode> child) {
  children_.push_back(child.release());
}

void FrameTreeNode::RemoveChild(FrameTreeNode* child) {
  std::vector<FrameTreeNode*>::iterator iter;

  for (iter = children_.begin(); iter != children_.end(); ++iter) {
    if ((*iter) == child)
      break;
  }

  if (iter != children_.end())
    children_.erase(iter);
}

void FrameTreeNode::ResetForMainFrame(
    RenderFrameHostImpl* new_render_frame_host) {
  owns_render_frame_host_ = false;
  frame_id_ = kInvalidFrameId;
  current_url_ = GURL();

  // The children may not have been cleared if a cross-process navigation
  // commits before the old process cleans everything up.  Make sure the child
  // nodes get deleted.
  children_.clear();

  render_frame_host_ = new_render_frame_host;
}

}  // namespace content
