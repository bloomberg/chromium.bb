// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/frame_host/frame_tree_node.h"

#include <queue>

#include "base/command_line.h"
#include "base/stl_util.h"
#include "content/browser/frame_host/frame_tree.h"
#include "content/browser/frame_host/navigator.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/public/common/content_switches.h"

namespace content {

int64 FrameTreeNode::next_frame_tree_node_id_ = 1;

FrameTreeNode::FrameTreeNode(FrameTree* frame_tree,
                             Navigator* navigator,
                             RenderFrameHostDelegate* render_frame_delegate,
                             RenderViewHostDelegate* render_view_delegate,
                             RenderWidgetHostDelegate* render_widget_delegate,
                             RenderFrameHostManager::Delegate* manager_delegate,
                             const std::string& name)
    : frame_tree_(frame_tree),
      navigator_(navigator),
      render_manager_(this,
                      render_frame_delegate,
                      render_view_delegate,
                      render_widget_delegate,
                      manager_delegate),
      frame_tree_node_id_(next_frame_tree_node_id_++),
      parent_(NULL),
      replication_state_(name),
      effective_sandbox_flags_(SandboxFlags::NONE) {
}

FrameTreeNode::~FrameTreeNode() {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableBrowserSideNavigation)) {
    navigator_->CancelNavigation(this);
  }
}

bool FrameTreeNode::IsMainFrame() const {
  return frame_tree_->root() == this;
}

void FrameTreeNode::AddChild(scoped_ptr<FrameTreeNode> child,
                             int process_id,
                             int frame_routing_id) {
  // Child frame must always be created in the same process as the parent.
  CHECK_EQ(process_id, render_manager_.current_host()->GetProcess()->GetID());

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
    // Subtle: we need to make sure the node is gone from the tree before
    // observers are notified of its deletion.
    scoped_ptr<FrameTreeNode> node_to_delete(*iter);
    children_.weak_erase(iter);
    node_to_delete->set_parent(NULL);
    node_to_delete.reset();
  }
}

void FrameTreeNode::ResetForNewProcess() {
  current_url_ = GURL();

  // The children may not have been cleared if a cross-process navigation
  // commits before the old process cleans everything up.  Make sure the child
  // nodes get deleted before swapping to a new process.
  ScopedVector<FrameTreeNode> old_children = children_.Pass();

  // Loop over all children removing them from the FrameTree. This will ensure
  // that nodes are properly removed from the tree and notifications are sent.
  // Note: since the |children_| vector is now empty, the calls into RemoveChild
  // will be a noop and will not result in repeatedly traversing the list.
  for (const auto& child : old_children)
    frame_tree_->RemoveFrame(child);

  old_children.clear();  // May notify observers.
}

void FrameTreeNode::SetFrameName(const std::string& name) {
  replication_state_.name = name;

  // Notify this frame's proxies about the updated name.
  render_manager_.OnDidUpdateName(name);
}

bool FrameTreeNode::IsDescendantOf(FrameTreeNode* other) const {
  if (!other || !other->child_count())
    return false;

  for (FrameTreeNode* node = parent(); node; node = node->parent()) {
    if (node == other)
      return true;
  }

  return false;
}

bool FrameTreeNode::IsLoading() const {
  RenderFrameHostImpl* current_frame_host =
      render_manager_.current_frame_host();
  RenderFrameHostImpl* pending_frame_host =
      render_manager_.pending_frame_host();

  DCHECK(current_frame_host);
  // TODO(fdegans): Change the implementation logic for PlzNavigate once
  // DidStartLoading and DidStopLoading are properly called.
  if (pending_frame_host && pending_frame_host->is_loading())
    return true;
  return current_frame_host->is_loading();
}

double FrameTreeNode::GetLoadingProgress() const {
  RenderFrameHostImpl* current_frame_host =
      render_manager_.current_frame_host();

  DCHECK(current_frame_host);
  return current_frame_host->loading_progress();
}

bool FrameTreeNode::CommitPendingSandboxFlags() {
  bool did_change_flags =
      effective_sandbox_flags_ != replication_state_.sandbox_flags;
  effective_sandbox_flags_ = replication_state_.sandbox_flags;
  return did_change_flags;
}

}  // namespace content
