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
#include "content/browser/frame_host/render_frame_proxy_host.h"
#include "content/browser/renderer_host/render_view_host_factory.h"
#include "content/browser/renderer_host/render_view_host_impl.h"

namespace content {

namespace {

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

bool CreateProxyForSiteInstance(const scoped_refptr<SiteInstance>& instance,
                                FrameTreeNode* node) {
  // If a new frame is created in the current SiteInstance, other frames in
  // that SiteInstance don't need a proxy for the new frame.
  SiteInstance* current_instance =
      node->render_manager()->current_frame_host()->GetSiteInstance();
  if (current_instance != instance.get())
    node->render_manager()->CreateRenderFrameProxy(instance.get());
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
}

FrameTree::~FrameTree() {
}

FrameTreeNode* FrameTree::FindByID(int64 frame_tree_node_id) {
  FrameTreeNode* node = NULL;
  ForEach(base::Bind(&FrameTreeNodeForId, frame_tree_node_id, &node));
  return node;
}

FrameTreeNode* FrameTree::FindByRoutingID(int process_id, int routing_id) {
  RenderFrameHostImpl* render_frame_host =
      RenderFrameHostImpl::FromID(process_id, routing_id);
  if (render_frame_host) {
    FrameTreeNode* result = render_frame_host->frame_tree_node();
    if (this == result->frame_tree())
      return result;
  }

  RenderFrameProxyHost* render_frame_proxy_host =
      RenderFrameProxyHost::FromID(process_id, routing_id);
  if (render_frame_proxy_host) {
    FrameTreeNode* result = render_frame_proxy_host->frame_tree_node();
    if (this == result->frame_tree())
      return result;
  }

  return NULL;
}

void FrameTree::ForEach(
    const base::Callback<bool(FrameTreeNode*)>& on_node) const {
  ForEach(on_node, NULL);
}

void FrameTree::ForEach(
    const base::Callback<bool(FrameTreeNode*)>& on_node,
    FrameTreeNode* skip_this_subtree) const {
  std::queue<FrameTreeNode*> queue;
  queue.push(root_.get());

  while (!queue.empty()) {
    FrameTreeNode* node = queue.front();
    queue.pop();
    if (skip_this_subtree == node)
      continue;

    if (!on_node.Run(node))
      break;

    for (size_t i = 0; i < node->child_count(); ++i)
      queue.push(node->child_at(i));
  }
}

RenderFrameHostImpl* FrameTree::AddFrame(FrameTreeNode* parent,
                                         int process_id,
                                         int new_routing_id,
                                         const std::string& frame_name) {
  // A child frame always starts with an initial empty document, which means
  // it is in the same SiteInstance as the parent frame. Ensure that the process
  // which requested a child frame to be added is the same as the process of the
  // parent node.
  // We return nullptr if this is not the case, which can happen in a race if an
  // old RFH sends a CreateChildFrame message as we're swapping to a new RFH.
  if (parent->current_frame_host()->GetProcess()->GetID() != process_id)
    return nullptr;

  scoped_ptr<FrameTreeNode> node(new FrameTreeNode(
      this, parent->navigator(), render_frame_delegate_, render_view_delegate_,
      render_widget_delegate_, manager_delegate_, frame_name));
  FrameTreeNode* node_ptr = node.get();
  // AddChild is what creates the RenderFrameHost.
  parent->AddChild(node.Pass(), process_id, new_routing_id);
  return node_ptr->current_frame_host();
}

void FrameTree::RemoveFrame(FrameTreeNode* child) {
  FrameTreeNode* parent = child->parent();
  if (!parent) {
    NOTREACHED() << "Unexpected RemoveFrame call for main frame.";
    return;
  }

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
      root()->render_manager()->CreateRenderFrame(
          site_instance, nullptr, MSG_ROUTING_NONE,
          CREATE_RF_SWAPPED_OUT | CREATE_RF_HIDDEN, nullptr);
    } else {
      root()->render_manager()->EnsureRenderViewInitialized(
          source, render_view_host, site_instance);
    }
  }

  scoped_refptr<SiteInstance> instance(site_instance);

  // Proxies are created in the FrameTree in response to a node navigating to a
  // new SiteInstance. Since |source|'s navigation will replace the currently
  // loaded document, the entire subtree under |source| will be removed.
  ForEach(base::Bind(&CreateProxyForSiteInstance, instance), source);
}

void FrameTree::ResetForMainFrameSwap() {
  root_->ResetForNewProcess();
  focused_frame_tree_node_id_ = -1;
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
    // If a RenderViewHost's main frame is pending deletion for this
    // |site_instance|, put it in the map of RenderViewHosts pending shutdown.
    // Otherwise return the existing RenderViewHost for the SiteInstance.
    RenderFrameHostImpl* main_frame = static_cast<RenderFrameHostImpl*>(
        iter->second->GetMainFrame());
    if (main_frame->frame_tree_node()->render_manager()->IsPendingDeletion(
            main_frame)) {
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
  SiteInstance* site_instance = render_frame_host->GetSiteInstance();
  RenderViewHostMap::iterator iter =
      render_view_host_map_.find(site_instance->GetId());
  CHECK(iter != render_view_host_map_.end());

  iter->second->increment_ref_count();
}

void FrameTree::UnregisterRenderFrameHost(
    RenderFrameHostImpl* render_frame_host) {
  SiteInstance* site_instance = render_frame_host->GetSiteInstance();
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

void FrameTree::FrameRemoved(FrameTreeNode* frame) {
  // No notification for the root frame.
  if (frame == root_.get())
    return;

  // Notify observers of the frame removal.
  if (!on_frame_removed_.is_null())
    on_frame_removed_.Run(frame->current_frame_host());
}

}  // namespace content
