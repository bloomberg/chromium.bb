// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/frame_host/frame_tree.h"

#include <queue>

#include "base/bind.h"
#include "base/callback.h"
#include "content/browser/frame_host/frame_tree_node.h"
#include "content/browser/frame_host/navigator.h"
#include "content/browser/frame_host/render_frame_host_factory.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/browser/renderer_host/render_view_host_factory.h"
#include "content/browser/renderer_host/render_view_host_impl.h"

namespace content {

namespace {
// Used with FrameTree::ForEach() to search for the FrameTreeNode
// corresponding to |frame_tree_node_id|.
bool FrameTreeNodeForId(int64 frame_tree_node_id,
                        FrameTreeNode** out_node,
                        FrameTreeNode* node) {
  if (node->frame_tree_node_id() == frame_tree_node_id) {
    *out_node = node;
    // Terminate iteration once the node has been found.
    return false;
  }
  return true;
}

bool FrameTreeNodeForRoutingId(int routing_id,
                               int process_id,
                               FrameTreeNode** out_node,
                               FrameTreeNode* node) {
  // TODO(creis): Look through the swapped out RFHs as well.
  if (node->current_frame_host()->GetProcess()->GetID() == process_id &&
      node->current_frame_host()->GetRoutingID() == routing_id) {
    *out_node = node;
    // Terminate iteration once the node has been found.
    return false;
  }
  return true;
}

// Iterate over the FrameTree to reset any node affected by the loss of the
// given RenderViewHost's process.
bool ResetNodesForNewProcess(RenderViewHost* render_view_host,
                             FrameTreeNode* node) {
  if (render_view_host == node->current_frame_host()->render_view_host())
    node->ResetForNewProcess();
  return true;
}

}  // namespace

FrameTree::FrameTree(Navigator* navigator,
                     RenderFrameHostDelegate* render_frame_delegate,
                     RenderViewHostDelegate* render_view_delegate,
                     RenderWidgetHostDelegate* render_widget_delegate,
                     RenderFrameHostManager::Delegate* manager_delegate)
    : render_frame_delegate_(render_frame_delegate),
      render_view_delegate_(render_view_delegate),
      render_widget_delegate_(render_widget_delegate),
      manager_delegate_(manager_delegate),
      root_(new FrameTreeNode(this,
                              navigator,
                              render_frame_delegate,
                              render_view_delegate,
                              render_widget_delegate,
                              manager_delegate,
                              std::string())) {
}

FrameTree::~FrameTree() {
}

FrameTreeNode* FrameTree::FindByID(int64 frame_tree_node_id) {
  FrameTreeNode* node = NULL;
  ForEach(base::Bind(&FrameTreeNodeForId, frame_tree_node_id, &node));
  return node;
}

FrameTreeNode* FrameTree::FindByRoutingID(int routing_id, int process_id) {
  FrameTreeNode* node = NULL;
  ForEach(
      base::Bind(&FrameTreeNodeForRoutingId, routing_id, process_id, &node));
  return node;
}

void FrameTree::ForEach(
    const base::Callback<bool(FrameTreeNode*)>& on_node) const {
  std::queue<FrameTreeNode*> queue;
  queue.push(root_.get());

  while (!queue.empty()) {
    FrameTreeNode* node = queue.front();
    queue.pop();
    if (!on_node.Run(node))
      break;

    for (size_t i = 0; i < node->child_count(); ++i)
      queue.push(node->child_at(i));
  }
}

RenderFrameHostImpl* FrameTree::AddFrame(FrameTreeNode* parent,
                                         int new_routing_id,
                                         const std::string& frame_name) {
  scoped_ptr<FrameTreeNode> node(new FrameTreeNode(
      this, parent->navigator(), render_frame_delegate_, render_view_delegate_,
      render_widget_delegate_, manager_delegate_, frame_name));
  FrameTreeNode* node_ptr = node.get();
  // AddChild is what creates the RenderFrameHost.
  parent->AddChild(node.Pass(), new_routing_id);
  return node_ptr->current_frame_host();
}

void FrameTree::RemoveFrame(FrameTreeNode* child) {
  FrameTreeNode* parent = child->parent();
  if (!parent) {
    NOTREACHED() << "Unexpected RemoveFrame call for main frame.";
    return;
  }

  // Notify observers of the frame removal.
  RenderFrameHostImpl* render_frame_host = child->current_frame_host();
  if (!on_frame_removed_.is_null()) {
    on_frame_removed_.Run(
        render_frame_host->render_view_host(),
        render_frame_host->GetRoutingID());
  }

  parent->RemoveChild(child);
}

void FrameTree::ResetForMainFrameSwap() {
  root_->ResetForNewProcess();
}

void FrameTree::RenderProcessGone(RenderViewHost* render_view_host) {
  // Walk the full tree looking for nodes that may be affected.  Once a frame
  // crashes, all of its child FrameTreeNodes go away.
  // Note that the helper function may call ResetForNewProcess on a node, which
  // clears its children before we iterate over them.  That's ok, because
  // ForEach does not add a node's children to the queue until after visiting
  // the node itself.
  ForEach(base::Bind(&ResetNodesForNewProcess, render_view_host));
}

RenderFrameHostImpl* FrameTree::GetMainFrame() const {
  return root_->current_frame_host();
}

void FrameTree::SetFrameRemoveListener(
    const base::Callback<void(RenderViewHostImpl*, int)>& on_frame_removed) {
  on_frame_removed_ = on_frame_removed;
}

RenderViewHostImpl* FrameTree::CreateRenderViewHostForMainFrame(
    SiteInstance* site_instance,
    int routing_id,
    int main_frame_routing_id,
    bool swapped_out,
    bool hidden) {
  DCHECK(main_frame_routing_id != MSG_ROUTING_NONE);
  RenderViewHostMap::iterator iter =
      render_view_host_map_.find(site_instance->GetId());
  CHECK(iter == render_view_host_map_.end());
  RenderViewHostImpl* rvh = static_cast<RenderViewHostImpl*>(
      RenderViewHostFactory::Create(site_instance,
                                    render_view_delegate_,
                                    render_widget_delegate_,
                                    routing_id,
                                    main_frame_routing_id,
                                    swapped_out,
                                    hidden));

  render_view_host_map_[site_instance->GetId()] =
      RenderViewHostRefCount(rvh, 0);
  return rvh;
}

RenderViewHostImpl* FrameTree::GetRenderViewHostForSubFrame(
    SiteInstance* site_instance) {
  RenderViewHostMap::iterator iter =
      render_view_host_map_.find(site_instance->GetId());
  // TODO(creis): Mirror the frame tree so this check can't fail.
  if (iter == render_view_host_map_.end())
    return NULL;
  RenderViewHostRefCount rvh_refcount = iter->second;
  return rvh_refcount.first;
}

void FrameTree::RegisterRenderFrameHost(
    RenderFrameHostImpl* render_frame_host) {
  SiteInstance* site_instance =
      render_frame_host->render_view_host()->GetSiteInstance();
  RenderViewHostMap::iterator iter =
      render_view_host_map_.find(site_instance->GetId());
  CHECK(iter != render_view_host_map_.end());

  // Increment the refcount.
  CHECK_GE(iter->second.second, 0);
  iter->second.second++;
}

void FrameTree::UnregisterRenderFrameHost(
    RenderFrameHostImpl* render_frame_host) {
  SiteInstance* site_instance =
      render_frame_host->render_view_host()->GetSiteInstance();
  RenderViewHostMap::iterator iter =
      render_view_host_map_.find(site_instance->GetId());
  CHECK(iter != render_view_host_map_.end());

  // Decrement the refcount and shutdown the RenderViewHost if no one else is
  // using it.
  CHECK_GT(iter->second.second, 0);
  iter->second.second--;
  if (iter->second.second == 0) {
    iter->second.first->Shutdown();
    render_view_host_map_.erase(iter);
  }
}

}  // namespace content
