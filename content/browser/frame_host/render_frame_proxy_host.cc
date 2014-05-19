// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/frame_host/render_frame_proxy_host.h"

#include "content/browser/frame_host/frame_tree_node.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/browser/site_instance_impl.h"
#include "content/common/frame_messages.h"
#include "ipc/ipc_message.h"

namespace content {

RenderFrameProxyHost::RenderFrameProxyHost(SiteInstance* site_instance,
                                           FrameTreeNode* frame_tree_node)
    : routing_id_(site_instance->GetProcess()->GetNextRoutingID()),
      site_instance_(site_instance),
      frame_tree_node_(frame_tree_node) {
  GetProcess()->AddRoute(routing_id_, this);
}

RenderFrameProxyHost::~RenderFrameProxyHost() {
  if (GetProcess()->HasConnection())
    Send(new FrameMsg_DeleteProxy(routing_id_));

  GetProcess()->RemoveRoute(routing_id_);
}

RenderViewHostImpl* RenderFrameProxyHost::GetRenderViewHost() {
  if (render_frame_host_.get())
    return render_frame_host_->render_view_host();
  return NULL;
}

scoped_ptr<RenderFrameHostImpl> RenderFrameProxyHost::PassFrameHostOwnership() {
  render_frame_host_->set_render_frame_proxy_host(NULL);
  return render_frame_host_.Pass();
}

bool RenderFrameProxyHost::Send(IPC::Message *msg) {
  // TODO(nasko): For now, RenderFrameHost uses this object to send IPC messages
  // while swapped out. This can be removed once we don't have a swapped out
  // state on RenderFrameHosts. See https://crbug.com/357747.
  msg->set_routing_id(routing_id_);
  return GetProcess()->Send(msg);
}

bool RenderFrameProxyHost::OnMessageReceived(const IPC::Message& msg) {
  // TODO(nasko): This can be removed once we don't have a swapped out state on
  // RenderFrameHosts. See https://crbug.com/357747.
  if (render_frame_host_.get())
    return render_frame_host_->OnMessageReceived(msg);

  return false;
}

}  // namespace content
