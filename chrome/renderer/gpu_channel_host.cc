// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/gpu_channel_host.h"

#include "chrome/common/child_process.h"
#include "chrome/common/gpu_messages.h"
#include "chrome/renderer/command_buffer_proxy.h"

GpuChannelHost::GpuChannelHost() : state_(UNCONNECTED) {
}

GpuChannelHost::~GpuChannelHost() {
}

void GpuChannelHost::Connect(const std::string& channel_name) {
  // Open a channel to the GPU process.
  channel_.reset(new IPC::SyncChannel(
      channel_name, IPC::Channel::MODE_CLIENT, this, NULL,
      ChildProcess::current()->io_message_loop(), true,
      ChildProcess::current()->GetShutDownEvent()));
}

void GpuChannelHost::OnMessageReceived(const IPC::Message& message) {
  DCHECK(message.routing_id() != MSG_ROUTING_CONTROL);
  if (!router_.RouteMessage(message)) {
    NOTREACHED() << "GpuChannelHost failed to route message";
  }
}

void GpuChannelHost::OnChannelConnected(int32 peer_pid) {
  state_ = CONNECTED;
}

void GpuChannelHost::OnChannelError() {
  state_ = LOST;

  // Channel is invalid and will be reinitialized if this host is requested
  // again.
  channel_.reset();

  // Inform all the proxies that an error has occured. This will be reported via
  // OpenGL as a lost context.
  for (ProxyMap::iterator iter = proxies_.begin();
       iter != proxies_.end(); iter++) {
    proxies_.erase(iter->first);
    router_.RemoveRoute(iter->first);
    iter->second->OnChannelError();
  }

  // The proxies are reference counted so this will not result in their
  // destruction if the client still holds a reference. The proxy will report
  // a lost context, indicating to the client that it needs to be recreated.
  proxies_.clear();
}

bool GpuChannelHost::Send(IPC::Message* message) {
  if (!channel_.get()) {
    delete message;
    return false;
  }

  return channel_->Send(message);
}

CommandBufferProxy* GpuChannelHost::CreateCommandBuffer() {
#if defined(ENABLE_GPU)
  // An error occurred. Need to get the host again to reinitialize it.
  if (!channel_.get())
    return NULL;

  int32 route_id;
  if (!Send(new GpuChannelMsg_CreateCommandBuffer(&route_id)) &&
      route_id != MSG_ROUTING_NONE) {
    return NULL;
  }

  CommandBufferProxy* command_buffer = new CommandBufferProxy(this, route_id);
  router_.AddRoute(route_id, command_buffer);
  proxies_[route_id] = command_buffer;
  return command_buffer;
#else
  return NULL;
#endif
}

void GpuChannelHost::DestroyCommandBuffer(CommandBufferProxy* command_buffer) {
#if defined(ENABLE_GPU)
  Send(new GpuChannelMsg_DestroyCommandBuffer(command_buffer->route_id()));

  // Check the proxy has not already been removed after a channel error.
  int route_id = command_buffer->route_id();
  if (proxies_.find(command_buffer->route_id()) != proxies_.end()) {
    proxies_.erase(route_id);
    router_.RemoveRoute(route_id);
  }

  delete command_buffer;
#endif
}
