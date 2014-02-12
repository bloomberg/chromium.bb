// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/frame_host/frame_tree_node.h"

#include <queue>

#include "base/stl_util.h"
#include "content/browser/frame_host/frame_tree.h"
#include "content/browser/frame_host/navigator.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/browser/renderer_host/render_view_host_impl.h"

namespace content {

const int64 FrameTreeNode::kInvalidFrameId = -1;
int64 FrameTreeNode::next_frame_tree_node_id_ = 1;

FrameTreeNode::FrameTreeNode(FrameTree* frame_tree,
                             Navigator* navigator,
                             RenderFrameHostDelegate* render_frame_delegate,
                             RenderViewHostDelegate* render_view_delegate,
                             RenderWidgetHostDelegate* render_widget_delegate,
                             RenderFrameHostManager::Delegate* manager_delegate,
                             int64 frame_id,
                             const std::string& name)
    : frame_tree_(frame_tree),
      navigator_(navigator),
      render_manager_(this,
                      render_frame_delegate,
                      render_view_delegate,
                      render_widget_delegate,
                      manager_delegate),
      frame_tree_node_id_(next_frame_tree_node_id_++),
      frame_id_(frame_id),
      frame_name_(name),
      parent_(NULL) {}

FrameTreeNode::~FrameTreeNode() {
}

bool FrameTreeNode::IsMainFrame() const {
  return frame_tree_->root() == this;
}

void FrameTreeNode::AddChild(scoped_ptr<FrameTreeNode> child,
                             int frame_routing_id) {
  // Initialize the RenderFrameHost for the new node.  We always create child
  // frames in the same SiteInstance as the current frame, and they can swap to
  // a different one if they navigate away.
  child->render_manager()->Init(
      render_manager_.current_host()->GetSiteInstance()->GetBrowserContext(),
      render_manager_.current_host()->GetSiteInstance(),
      render_manager_.current_host()->GetRoutingID(),
      frame_routing_id);
  child->set_parent(this);
  children_.push_back(child.release());
}

void FrameTreeNode::RemoveChild(FrameTreeNode* child) {
  std::vector<FrameTreeNode*>::iterator iter;

  for (iter = children_.begin(); iter != children_.end(); ++iter) {
    if ((*iter) == child)
      break;
  }

  if (iter != children_.end()) {
    (*iter)->set_parent(NULL);
    children_.erase(iter);
  }
}

void FrameTreeNode::ResetForNewProcess() {
  frame_id_ = kInvalidFrameId;
  current_url_ = GURL();

  // The children may not have been cleared if a cross-process navigation
  // commits before the old process cleans everything up.  Make sure the child
  // nodes get deleted before swapping to a new process.
  children_.clear();
}

}  // namespace content
