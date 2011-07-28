// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/gpu/gpu_channel_host.h"

#include "content/common/child_process.h"
#include "content/common/gpu/gpu_messages.h"
#include "content/renderer/gpu/command_buffer_proxy.h"
#include "content/renderer/gpu/gpu_surface_proxy.h"
#include "content/renderer/gpu/transport_texture_service.h"
#include "content/renderer/render_thread.h"
#include "googleurl/src/gurl.h"

GpuChannelHost::GpuChannelHost()
    : state_(kUnconnected),
      transport_texture_service_(new TransportTextureService()) {
}

GpuChannelHost::~GpuChannelHost() {
}

void GpuChannelHost::Connect(
    const IPC::ChannelHandle& channel_handle,
    base::ProcessHandle renderer_process_for_gpu) {
  // Open a channel to the GPU process.
  channel_.reset(new IPC::SyncChannel(
      channel_handle, IPC::Channel::MODE_CLIENT, this,
      ChildProcess::current()->io_message_loop_proxy(), true,
      ChildProcess::current()->GetShutDownEvent()));

  // It is safe to send IPC messages before the channel completes the connection
  // and receives the hello message from the GPU process. The messages get
  // cached.
  state_ = kConnected;

  // Notify the GPU process of our process handle. This gives it the ability
  // to map renderer handles into the GPU process.
  Send(new GpuChannelMsg_Initialize(renderer_process_for_gpu));
}

void GpuChannelHost::set_gpu_info(const GPUInfo& gpu_info) {
  gpu_info_ = gpu_info;
}

const GPUInfo& GpuChannelHost::gpu_info() const {
  return gpu_info_;
}

void GpuChannelHost::SetStateLost() {
  state_ = kLost;
}

bool GpuChannelHost::OnMessageReceived(const IPC::Message& message) {
  DCHECK(message.routing_id() != MSG_ROUTING_CONTROL);

  return router_.RouteMessage(message);
}

void GpuChannelHost::OnChannelConnected(int32 peer_pid) {
  channel_->AddFilter(transport_texture_service_.get());
}

void GpuChannelHost::OnChannelError() {
  state_ = kLost;

  // Channel is invalid and will be reinitialized if this host is requested
  // again.
  channel_.reset();

  // Inform all the proxies that an error has occured. This will be reported via
  // OpenGL as a lost context.
  for (ProxyMap::iterator iter = proxies_.begin();
       iter != proxies_.end(); iter++) {
    router_.RemoveRoute(iter->first);
    iter->second->OnChannelError();
  }

  // The proxies are reference counted so this will not result in their
  // destruction if the client still holds a reference. The proxy will report
  // a lost context, indicating to the client that it needs to be recreated.
  proxies_.clear();
}

bool GpuChannelHost::Send(IPC::Message* message) {
  // The GPU process never sends synchronous IPCs so clear the unblock flag to
  // preserve order.
  message->set_unblock(false);
  if (channel_.get())
    return channel_->Send(message);

  // Callee takes ownership of message, regardless of whether Send is
  // successful. See IPC::Message::Sender.
  delete message;
  return false;
}

CommandBufferProxy* GpuChannelHost::CreateViewCommandBuffer(
    int render_view_id,
    const std::string& allowed_extensions,
    const std::vector<int32>& attribs,
    const GURL& active_url) {
#if defined(ENABLE_GPU)
  // An error occurred. Need to get the host again to reinitialize it.
  if (!channel_.get())
    return NULL;

  GPUCreateCommandBufferConfig init_params;
  init_params.allowed_extensions = allowed_extensions;
  init_params.attribs = attribs;
  init_params.active_url = active_url;
  int32 route_id;
  if (!RenderThread::current()->Send(
      new GpuHostMsg_CreateViewCommandBuffer(
          render_view_id, init_params, &route_id))) {
    return NULL;
  }

  if (route_id == MSG_ROUTING_NONE)
    return NULL;

  CommandBufferProxy* command_buffer = new CommandBufferProxy(this, route_id);
  router_.AddRoute(route_id, command_buffer);
  proxies_[route_id] = command_buffer;
  return command_buffer;
#else
  return NULL;
#endif
}

GpuVideoDecodeAcceleratorHost* GpuChannelHost::CreateVideoDecoder(
    int command_buffer_route_id,
    const std::vector<uint32>& configs,
    media::VideoDecodeAccelerator::Client* client) {
  ProxyMap::iterator it = proxies_.find(command_buffer_route_id);
  DCHECK(it != proxies_.end());
  CommandBufferProxy* proxy = it->second;
  return proxy->CreateVideoDecoder(configs, client);
}

CommandBufferProxy* GpuChannelHost::CreateOffscreenCommandBuffer(
    const gfx::Size& size,
    const std::string& allowed_extensions,
    const std::vector<int32>& attribs,
    const GURL& active_url) {
#if defined(ENABLE_GPU)
  // An error occurred. Need to get the host again to reinitialize it.
  if (!channel_.get())
    return NULL;

  GPUCreateCommandBufferConfig init_params;
  init_params.allowed_extensions = allowed_extensions;
  init_params.attribs = attribs;
  init_params.active_url = active_url;
  int32 route_id;
  if (!Send(new GpuChannelMsg_CreateOffscreenCommandBuffer(size,
                                                           init_params,
                                                           &route_id))) {
    return NULL;
  }

  if (route_id == MSG_ROUTING_NONE)
    return NULL;

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

GpuSurfaceProxy* GpuChannelHost::CreateOffscreenSurface(const gfx::Size& size) {
#if defined(ENABLE_GPU)
  int route_id;
  if (!Send(new GpuChannelMsg_CreateOffscreenSurface(size, &route_id)))
    return NULL;

  scoped_ptr<GpuSurfaceProxy> surface(new GpuSurfaceProxy(this, route_id));
  router_.AddRoute(route_id, surface.get());

  return surface.release();
#endif
}

void GpuChannelHost::DestroySurface(GpuSurfaceProxy* surface) {
#if defined(ENABLE_GPU)
  Send(new GpuChannelMsg_DestroySurface(surface->route_id()));
  if (router_.ResolveRoute(surface->route_id()))
    router_.RemoveRoute(surface->route_id());

  delete surface;
#endif
}
