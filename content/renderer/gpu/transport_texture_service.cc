// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/gpu/transport_texture_service.h"

#include "base/bind.h"
#include "base/stl_util.h"
#include "content/common/child_process.h"
#include "content/common/gpu/gpu_messages.h"
#include "content/renderer/gpu/transport_texture_host.h"

TransportTextureService::TransportTextureService()
    : channel_(NULL),
      next_host_id_(1) {
}

TransportTextureService::~TransportTextureService() {
  STLDeleteElements(&pending_messages_);
}

void TransportTextureService::OnFilterAdded(IPC::Channel* channel) {
  channel_ = channel;
  router_.reset(new MessageRouter());

  // Add all pending routes.
  for (size_t i = 0; i < pending_routes_.size(); ++i)
    router_->AddRoute(pending_routes_[i].first, pending_routes_[i].second);
  pending_routes_.clear();

  // Submit all pending messages.
  for (size_t i = 0; i < pending_messages_.size(); ++i)
    channel_->Send(pending_messages_[i]);
  pending_messages_.clear();
}

void TransportTextureService::OnFilterRemoved() {
  // TODO(hclam): Implement.
}

void TransportTextureService::OnChannelClosing() {
  channel_ = NULL;
}

bool TransportTextureService::OnMessageReceived(const IPC::Message& msg) {
  switch (msg.type()) {
    case GpuTransportTextureHostMsg_TransportTextureCreated::ID:
    case GpuTransportTextureHostMsg_CreateTextures::ID:
    case GpuTransportTextureHostMsg_ReleaseTextures::ID:
    case GpuTransportTextureHostMsg_TextureUpdated::ID:
      if (!router_->RouteMessage(msg)) {
        LOG(ERROR) << "GpuTransportTextureHostMsg cannot be dispatched.";
        return false;
      }
      return true;
    default:
      return false;
  }
}

scoped_refptr<TransportTextureHost>
TransportTextureService::CreateTransportTextureHost(
    RendererGLContext* context, int context_route_id) {
#if !defined(OS_MACOSX)
  // TODO(hclam): Check this is on Render Thread.

  // Create the object and then add the route.
  scoped_refptr<TransportTextureHost> host = new TransportTextureHost(
      ChildProcess::current()->io_message_loop(), MessageLoop::current(),
      this, this, context, context_route_id, next_host_id_);

  // Add route in the IO thread.
  ChildProcess::current()->io_message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&TransportTextureService::AddRouteInternal, this,
                 next_host_id_, host));

  // Increment host ID for next object.
  ++next_host_id_;
  return host;
#else
  // TransportTextureHost has problem compiling on Mac so skip it.
  return NULL;
#endif
}

void TransportTextureService::RemoveRoute(int32 host_id) {
  ChildProcess::current()->io_message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&TransportTextureService::RemoveRouteInternal, this, host_id));
}

bool TransportTextureService::Send(IPC::Message* msg) {
  ChildProcess::current()->io_message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&TransportTextureService::SendInternal, this, msg));
  return true;
}

void TransportTextureService::AddRouteInternal(
    int32 host_id, IPC::Channel::Listener* listener) {
  DCHECK_EQ(ChildProcess::current()->io_message_loop(),
            MessageLoop::current());

  // If router is there then just add the route, or we have to save it.
  if (router_.get())
    router_->AddRoute(host_id, listener);
  else
    pending_routes_.push_back(std::make_pair(host_id, listener));
}

void TransportTextureService::RemoveRouteInternal(int32 host_id) {
  DCHECK_EQ(ChildProcess::current()->io_message_loop(),
            MessageLoop::current());

  if (router_.get())
    router_->RemoveRoute(host_id);
}

void TransportTextureService::SendInternal(IPC::Message* msg) {
  DCHECK_EQ(ChildProcess::current()->io_message_loop(),
            MessageLoop::current());

  if (channel_)
    channel_->Send(msg);
  else
    pending_messages_.push_back(msg);
}
