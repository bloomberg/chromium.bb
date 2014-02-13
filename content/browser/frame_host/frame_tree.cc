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

// TODO(creis): Remove this version along with FrameTreeNode::frame_id().
bool FrameTreeNodeForFrameId(int64 frame_id,
                             FrameTreeNode** out_node,
                             FrameTreeNode* node) {
  if (node->frame_id() == frame_id) {
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
                              FrameTreeNode::kInvalidFrameId,
                              std::string())) {
}

FrameTree::~FrameTree() {
}

FrameTreeNode* FrameTree::FindByID(int64 frame_tree_node_id) {
  FrameTreeNode* node = NULL;
  ForEach(base::Bind(&FrameTreeNodeForId, frame_tree_node_id, &node));
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

bool FrameTree::IsFirstNavigationAfterSwap() const {
  return root_->frame_id() == FrameTreeNode::kInvalidFrameId;
}

void FrameTree::OnFirstNavigationAfterSwap(int main_frame_id) {
  root_->set_frame_id(main_frame_id);
}

RenderFrameHostImpl* FrameTree::AddFrame(int frame_routing_id,
                                         int64 parent_frame_id,
                                         int64 frame_id,
                                         const std::string& frame_name) {
  FrameTreeNode* parent = FindByFrameID(parent_frame_id);
  // TODO(ajwong): Should the renderer be killed here? Would there be a race on
  // shutdown that might make this case possible?
  if (!parent)
    return NULL;

  scoped_ptr<FrameTreeNode> node(new FrameTreeNode(
      this, parent->navigator(), render_frame_delegate_, render_view_delegate_,
      render_widget_delegate_, manager_delegate_, frame_id, frame_name));
  FrameTreeNode* node_ptr = node.get();
  // AddChild is what creates the RenderFrameHost.
  parent->AddChild(node.Pass(), frame_routing_id);
  return node_ptr->current_frame_host();
}

void FrameTree::RemoveFrame(RenderFrameHostImpl* render_frame_host,
                            int64 parent_frame_id,
                            int64 frame_id) {
  // If switches::kSitePerProcess is not specified, then the FrameTree only
  // contains a node for the root element. However, even in this case
  // frame detachments need to be broadcast outwards.
  //
  // TODO(ajwong): Move this below the |parent| check after the FrameTree is
  // guaranteed to be correctly populated even without the
  // switches::kSitePerProcess flag.
  FrameTreeNode* parent = FindByFrameID(parent_frame_id);
  FrameTreeNode* child = FindByFrameID(frame_id);
  if (!on_frame_removed_.is_null()) {
    on_frame_removed_.Run(
        render_frame_host->render_view_host(), frame_id);
  }

  // TODO(ajwong): Should the renderer be killed here? Would there be a race on
  // shutdown that might make this case possible?
  if (!parent || !child)
    return;

  parent->RemoveChild(child);
}

void FrameTree::SetFrameUrl(int64 frame_id, const GURL& url) {
  FrameTreeNode* node = FindByFrameID(frame_id);
  // TODO(ajwong): Should the renderer be killed here? Would there be a race on
  // shutdown that might make this case possible?
  if (!node)
    return;

  if (node)
    node->set_current_url(url);
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
    const base::Callback<void(RenderViewHostImpl*, int64)>& on_frame_removed) {
  on_frame_removed_ = on_frame_removed;
}

void FrameTree::ClearFrameRemoveListenerForTesting() {
  on_frame_removed_.Reset();
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
  if (iter != render_view_host_map_.end()) {
    // If a RenderViewHost is pending shutdown for this |site_instance|, put it
    // in the map of RenderViewHosts pending shutdown. Otherwise there should
    // not be a RenderViewHost for the SiteInstance.
    CHECK_EQ(RenderViewHostImpl::STATE_PENDING_SHUTDOWN,
             iter->second->rvh_state());
    render_view_host_pending_shutdown_map_.insert(
        std::pair<int, RenderViewHostImpl*>(site_instance->GetId(),
                                            iter->second));
    render_view_host_map_.erase(iter);
  }
  RenderViewHostImpl* rvh = static_cast<RenderViewHostImpl*>(
      RenderViewHostFactory::Create(site_instance,
                                    render_view_delegate_,
                                    render_widget_delegate_,
                                    routing_id,
                                    main_frame_routing_id,
                                    swapped_out,
                                    hidden));

  render_view_host_map_[site_instance->GetId()] = rvh;
  return rvh;
}

RenderViewHostImpl* FrameTree::GetRenderViewHostForSubFrame(
    SiteInstance* site_instance) {
  RenderViewHostMap::iterator iter =
      render_view_host_map_.find(site_instance->GetId());
  // TODO(creis): Mirror the frame tree so this check can't fail.
  if (iter == render_view_host_map_.end())
    return NULL;
  return iter->second;
}

void FrameTree::RegisterRenderFrameHost(
    RenderFrameHostImpl* render_frame_host) {
  SiteInstance* site_instance =
      render_frame_host->render_view_host()->GetSiteInstance();
  RenderViewHostMap::iterator iter =
      render_view_host_map_.find(site_instance->GetId());
  CHECK(iter != render_view_host_map_.end());

  iter->second->increment_ref_count();
}

void FrameTree::UnregisterRenderFrameHost(
    RenderFrameHostImpl* render_frame_host) {
  SiteInstance* site_instance =
      render_frame_host->render_view_host()->GetSiteInstance();
  int32 site_instance_id = site_instance->GetId();
  RenderViewHostMap::iterator iter =
      render_view_host_map_.find(site_instance_id);
  if (iter != render_view_host_map_.end() &&
      iter->second == render_frame_host->render_view_host()) {
    // Decrement the refcount and shutdown the RenderViewHost if no one else is
    // using it.
    CHECK_GT(iter->second->ref_count(), 0);
    iter->second->decrement_ref_count();
    if (iter->second->ref_count() == 0) {
      iter->second->Shutdown();
      render_view_host_map_.erase(iter);
    }
  } else {
    // The RenderViewHost should be in the list of RenderViewHosts pending
    // shutdown.
    bool render_view_host_found = false;
    std::pair<RenderViewHostMultiMap::iterator,
              RenderViewHostMultiMap::iterator> result =
        render_view_host_pending_shutdown_map_.equal_range(site_instance_id);
    for (RenderViewHostMultiMap::iterator multi_iter = result.first;
         multi_iter != result.second;
         ++multi_iter) {
      if (multi_iter->second != render_frame_host->render_view_host())
        continue;
      render_view_host_found = true;
      RenderViewHostImpl* rvh = multi_iter->second;
      // Decrement the refcount and shutdown the RenderViewHost if no one else
      // is using it.
      CHECK_GT(rvh->ref_count(), 0);
      rvh->decrement_ref_count();
      if (rvh->ref_count() == 0) {
        rvh->Shutdown();
        render_view_host_pending_shutdown_map_.erase(multi_iter);
      }
      break;
    }
    CHECK(render_view_host_found);
  }
}

FrameTreeNode* FrameTree::FindByFrameID(int64 frame_id) {
  FrameTreeNode* node = NULL;
  ForEach(base::Bind(&FrameTreeNodeForFrameId, frame_id, &node));
  return node;
}

}  // namespace content
