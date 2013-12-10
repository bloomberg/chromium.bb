// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/frame_host/render_frame_host_impl.h"

#include "base/containers/hash_tables.h"
#include "base/lazy_instance.h"
#include "content/browser/frame_host/frame_tree.h"
#include "content/browser/frame_host/frame_tree_node.h"
#include "content/browser/frame_host/navigator.h"
#include "content/browser/frame_host/render_frame_host_delegate.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/common/frame_messages.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/user_metrics.h"
#include "url/gurl.h"

namespace content {

// The (process id, routing id) pair that identifies one RenderFrame.
typedef std::pair<int32, int32> RenderFrameHostID;
typedef base::hash_map<RenderFrameHostID, RenderFrameHostImpl*>
    RoutingIDFrameMap;
static base::LazyInstance<RoutingIDFrameMap> g_routing_id_frame_map =
    LAZY_INSTANCE_INITIALIZER;

// static
RenderFrameHostImpl* RenderFrameHostImpl::FromID(
    int process_id, int routing_id) {
  RoutingIDFrameMap* frames = g_routing_id_frame_map.Pointer();
  RoutingIDFrameMap::iterator it = frames->find(
      RenderFrameHostID(process_id, routing_id));
  return it == frames->end() ? NULL : it->second;
}

RenderFrameHostImpl::RenderFrameHostImpl(
    RenderViewHostImpl* render_view_host,
    RenderFrameHostDelegate* delegate,
    FrameTree* frame_tree,
    FrameTreeNode* frame_tree_node,
    int routing_id,
    bool is_swapped_out)
    : render_view_host_(render_view_host),
      delegate_(delegate),
      frame_tree_(frame_tree),
      frame_tree_node_(frame_tree_node),
      routing_id_(routing_id),
      is_swapped_out_(is_swapped_out) {
  GetProcess()->AddRoute(routing_id_, this);
  g_routing_id_frame_map.Get().insert(std::make_pair(
      RenderFrameHostID(GetProcess()->GetID(), routing_id_),
      this));
}

RenderFrameHostImpl::~RenderFrameHostImpl() {
  GetProcess()->RemoveRoute(routing_id_);
  g_routing_id_frame_map.Get().erase(
      RenderFrameHostID(GetProcess()->GetID(), routing_id_));
  if (delegate_)
    delegate_->RenderFrameDeleted(this);
}

int RenderFrameHostImpl::GetRoutingID() {
  return routing_id_;
}

bool RenderFrameHostImpl::Send(IPC::Message* message) {
  return GetProcess()->Send(message);
}

bool RenderFrameHostImpl::OnMessageReceived(const IPC::Message &msg) {
  if (delegate_->OnMessageReceived(this, msg))
    return true;

  bool handled = true;
  bool msg_is_ok = true;
  IPC_BEGIN_MESSAGE_MAP_EX(RenderFrameHostImpl, msg, msg_is_ok)
    IPC_MESSAGE_HANDLER(FrameHostMsg_Detach, OnDetach)
    IPC_MESSAGE_HANDLER(FrameHostMsg_DidStartProvisionalLoadForFrame,
                        OnDidStartProvisionalLoadForFrame)
  IPC_END_MESSAGE_MAP_EX()

  if (!msg_is_ok) {
    // The message had a handler, but its de-serialization failed.
    // Kill the renderer.
    RecordAction(UserMetricsAction("BadMessageTerminate_RFH"));
    GetProcess()->ReceivedBadMessage();
  }

  return handled;
}

void RenderFrameHostImpl::Init() {
  GetProcess()->ResumeRequestsForView(routing_id());
}

RenderProcessHost* RenderFrameHostImpl::GetProcess() const {
  // TODO(nasko): This should return its own process, once we have working
  // cross-process navigation for subframes.
  return render_view_host_->GetProcess();
}

void RenderFrameHostImpl::OnCreateChildFrame(int new_frame_routing_id,
                                             int64 parent_frame_id,
                                             int64 frame_id,
                                             const std::string& frame_name) {
  RenderFrameHostImpl* new_frame = frame_tree_->AddFrame(
      new_frame_routing_id, parent_frame_id, frame_id, frame_name);
  if (delegate_)
    delegate_->RenderFrameCreated(new_frame);
}

void RenderFrameHostImpl::OnDetach(int64 parent_frame_id, int64 frame_id) {
  frame_tree_->RemoveFrame(this, parent_frame_id, frame_id);
}

void RenderFrameHostImpl::OnDidStartProvisionalLoadForFrame(
    int64 frame_id,
    int64 parent_frame_id,
    bool is_main_frame,
    const GURL& url) {
  frame_tree_node_->navigator()->DidStartProvisionalLoad(
      this, frame_id, parent_frame_id, is_main_frame, url);
}

}  // namespace content
