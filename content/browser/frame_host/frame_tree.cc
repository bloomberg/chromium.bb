// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/frame_host/frame_tree.h"

#include <queue>

#include "base/bind.h"
#include "base/callback.h"
#include "base/containers/hash_tables.h"
#include "base/lazy_instance.h"
#include "content/browser/frame_host/frame_tree_node.h"
#include "content/browser/frame_host/navigator.h"
#include "content/browser/frame_host/render_frame_host_factory.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/browser/renderer_host/render_view_host_factory.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/public/browser/browser_thread.h"

namespace content {

namespace {

// This is a global map between frame_tree_node_ids and pointer to
// FrameTreeNodes.
typedef base::hash_map<int64, FrameTreeNode*> FrameTreeNodeIDMap;

base::LazyInstance<FrameTreeNodeIDMap> g_frame_tree_node_id_map =
    LAZY_INSTANCE_INITIALIZER;

// Used with FrameTree::ForEach() to search for the FrameTreeNode
// corresponding to |frame_tree_node_id| whithin a specific FrameTree.
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

bool CreateProxyForSiteInstance(FrameTreeNode* source_node,
                                const scoped_refptr<SiteInstance>& instance,
                                FrameTreeNode* node) {
  // Skip the node that initiated the creation.
  if (source_node == node)
    return true;

  node->render_manager()->CreateRenderFrameProxy(instance);
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
                              std::string())),
      focused_frame_tree_node_id_(-1) {
    std::pair<FrameTreeNodeIDMap::iterator, bool> result =
        g_frame_tree_node_id_map.Get().insert(
            std::make_pair(root_->frame_tree_node_id(), root_.get()));
    CHECK(result.second);
}

FrameTree::~FrameTree() {
  g_frame_tree_node_id_map.Get().erase(root_->frame_tree_node_id());
}

// static
FrameTreeNode* FrameTree::GloballyFindByID(int64 frame_tree_node_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  FrameTreeNodeIDMap* nodes = g_frame_tree_node_id_map.Pointer();
  FrameTreeNodeIDMap::iterator it = nodes->find(frame_tree_node_id);
  return it == nodes->end() ? NULL : it->second;
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
  std::pair<FrameTreeNodeIDMap::iterator, bool> result =
      g_frame_tree_node_id_map.Get().insert(
          std::make_pair(node->frame_tree_node_id(), node.get()));
  CHECK(result.second);
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
    on_frame_removed_.Run(render_frame_host);
  }
  g_frame_tree_node_id_map.Get().erase(child->frame_tree_node_id());
  parent->RemoveChild(child);
}

void FrameTree::CreateProxiesForSiteInstance(
    FrameTreeNode* source,
    SiteInstance* site_instance) {
  // Create the swapped out RVH for the new SiteInstance. This will create
  // a top-level swapped out RFH as well, which will then be wrapped by a
  // RenderFrameProxyHost.
  if (!source->IsMainFrame()) {
    RenderViewHostImpl* render_view_host =
        source->frame_tree()->GetRenderViewHost(site_instance);
    if (!render_view_host) {
      root()->render_manager()->CreateRenderFrame(site_instance,
                                                  MSG_ROUTING_NONE,
                                                  true,
                                                  false,
                                                  true);
    }
  }

  scoped_refptr<SiteInstance> instance(site_instance);
  ForEach(base::Bind(&CreateProxyForSiteInstance, source, instance));
}

void FrameTree::ResetForMainFrameSwap() {
  root_->ResetForNewProcess();
  focused_frame_tree_node_id_ = -1;
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

FrameTreeNode* FrameTree::GetFocusedFrame() {
  return FindByID(focused_frame_tree_node_id_);
}

void FrameTree::SetFocusedFrame(FrameTreeNode* node) {
  focused_frame_tree_node_id_ = node->frame_tree_node_id();
}

void FrameTree::SetFrameRemoveListener(
    const base::Callback<void(RenderFrameHost*)>& on_frame_removed) {
  on_frame_removed_ = on_frame_removed;
}

RenderViewHostImpl* FrameTree::CreateRenderViewHost(SiteInstance* site_instance,
                                                    int routing_id,
                                                    int main_frame_routing_id,
                                                    bool swapped_out,
                                                    bool hidden) {
  DCHECK(main_frame_routing_id != MSG_ROUTING_NONE);
  RenderViewHostMap::iterator iter =
      render_view_host_map_.find(site_instance->GetId());
  if (iter != render_view_host_map_.end()) {
    // If a RenderViewHost is pending shutdown for this |site_instance|, put it
    // in the map of RenderViewHosts pending shutdown. Otherwise return the
    // existing RenderViewHost for the SiteInstance.
    if (iter->second->rvh_state() ==
        RenderViewHostImpl::STATE_PENDING_SHUTDOWN) {
      render_view_host_pending_shutdown_map_.insert(
          std::pair<int, RenderViewHostImpl*>(site_instance->GetId(),
                                              iter->second));
      render_view_host_map_.erase(iter);
    } else {
      return iter->second;
    }
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

RenderViewHostImpl* FrameTree::GetRenderViewHost(SiteInstance* site_instance) {
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

}  // namespace content
