// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/gpu/gpu_channel_host.h"

#include "base/message_loop.h"
#include "base/message_loop_proxy.h"
#include "content/common/child_process.h"
#include "content/common/gpu/gpu_messages.h"
#include "content/renderer/gpu/command_buffer_proxy.h"
#include "content/renderer/gpu/transport_texture_service.h"
#include "content/renderer/render_process.h"
#include "content/renderer/render_thread.h"
#include "googleurl/src/gurl.h"
#include "ipc/ipc_sync_message_filter.h"

using base::AutoLock;
using base::MessageLoopProxy;

GpuChannelHost::Listener::Listener(
    base::WeakPtr<IPC::Channel::Listener> listener,
    scoped_refptr<base::MessageLoopProxy> loop)
    : listener_(listener),
      loop_(loop) {

}

GpuChannelHost::Listener::~Listener() {

}

void GpuChannelHost::Listener::DispatchMessage(const IPC::Message& msg) {
  if (listener_.get())
    listener_->OnMessageReceived(msg);
}

void GpuChannelHost::Listener::DispatchError() {
  if (listener_.get())
    listener_->OnChannelError();
}

GpuChannelHost::MessageFilter::MessageFilter(GpuChannelHost* parent)
    : parent_(parent) {
}

GpuChannelHost::MessageFilter::~MessageFilter() {

}

void GpuChannelHost::MessageFilter::AddRoute(
    int route_id,
    base::WeakPtr<IPC::Channel::Listener> listener,
    scoped_refptr<MessageLoopProxy> loop) {
  DCHECK(MessageLoop::current() == ChildProcess::current()->io_message_loop());
  DCHECK(listeners_.find(route_id) == listeners_.end());
  listeners_[route_id] = new GpuChannelHost::Listener(listener, loop);
}

void GpuChannelHost::MessageFilter::RemoveRoute(int route_id) {
  DCHECK(MessageLoop::current() == ChildProcess::current()->io_message_loop());
  ListenerMap::iterator it = listeners_.find(route_id);
  if (it != listeners_.end())
    listeners_.erase(it);
}

bool GpuChannelHost::MessageFilter::OnMessageReceived(
    const IPC::Message& message) {
  DCHECK(MessageLoop::current() == ChildProcess::current()->io_message_loop());
  // Never handle sync message replies or we will deadlock here.
  if (message.is_reply())
    return false;

  DCHECK(message.routing_id() != MSG_ROUTING_CONTROL);

  ListenerMap::iterator it = listeners_.find(message.routing_id());

  if (it != listeners_.end()) {
    const scoped_refptr<GpuChannelHost::Listener>& listener = it->second;
    listener->loop()->PostTask(
        FROM_HERE,
        NewRunnableMethod(
            listener.get(),
            &GpuChannelHost::Listener::DispatchMessage,
            message));
  }

  return true;
}

void GpuChannelHost::MessageFilter::OnChannelError() {
  DCHECK(MessageLoop::current() == ChildProcess::current()->io_message_loop());
  // Inform all the proxies that an error has occurred. This will be reported
  // via OpenGL as a lost context.
  for (ListenerMap::iterator it = listeners_.begin();
       it != listeners_.end();
       it++) {
    const scoped_refptr<GpuChannelHost::Listener>& listener = it->second;
    listener->loop()->PostTask(
        FROM_HERE,
        NewRunnableMethod(
            listener.get(),
            &GpuChannelHost::Listener::DispatchError));
  }

  listeners_.clear();

  ChildThread* main_thread = RenderProcess::current()->main_thread();
  MessageLoop* main_loop = main_thread->message_loop();
  main_loop->PostTask(FROM_HERE,
                      NewRunnableMethod(parent_,
                                        &GpuChannelHost::OnChannelError));
}

GpuChannelHost::GpuChannelHost()
    : state_(kUnconnected),
      transport_texture_service_(new TransportTextureService()) {
}

GpuChannelHost::~GpuChannelHost() {
}

void GpuChannelHost::Connect(
    const IPC::ChannelHandle& channel_handle,
    base::ProcessHandle renderer_process_for_gpu) {
  DCHECK(RenderThread::current());
  // Open a channel to the GPU process. We pass NULL as the main listener here
  // since we need to filter everything to route it to the right thread.
  channel_.reset(new IPC::SyncChannel(
      channel_handle, IPC::Channel::MODE_CLIENT, NULL,
      ChildProcess::current()->io_message_loop_proxy(), true,
      ChildProcess::current()->GetShutDownEvent()));

  sync_filter_ = new IPC::SyncMessageFilter(
      ChildProcess::current()->GetShutDownEvent());

  channel_->AddFilter(sync_filter_.get());

  channel_->AddFilter(transport_texture_service_.get());

  channel_filter_ = new MessageFilter(this);

  // Install the filter last, because we intercept all leftover
  // messages.
  channel_->AddFilter(channel_filter_.get());

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

void GpuChannelHost::OnChannelError() {
  state_ = kLost;

  // Channel is invalid and will be reinitialized if this host is requested
  // again.
  channel_.reset();
}

bool GpuChannelHost::Send(IPC::Message* message) {
  // The GPU process never sends synchronous IPCs so clear the unblock flag to
  // preserve order.
  message->set_unblock(false);

  // Currently we need to choose between two different mechanisms for sending.
  // On the main thread we use the regular channel Send() method, on another
  // thread we use SyncMessageFilter. We also have to be careful interpreting
  // RenderThread::current() since it might return NULL during shutdown, while
  // we are actually calling from the main thread (discard message then).
  //
  // TODO: Can we just always use sync_filter_ since we setup the channel
  //       without a main listener?
  if (RenderThread::current()) {
    if (channel_.get())
      return channel_->Send(message);
  } else if (MessageLoop::current()) {
    return sync_filter_->Send(message);
  }

  // Callee takes ownership of message, regardless of whether Send is
  // successful. See IPC::Message::Sender.
  delete message;
  return false;
}

CommandBufferProxy* GpuChannelHost::CreateViewCommandBuffer(
    int render_view_id,
    CommandBufferProxy* share_group,
    const std::string& allowed_extensions,
    const std::vector<int32>& attribs,
    const GURL& active_url) {
  DCHECK(ChildThread::current());
#if defined(ENABLE_GPU)
  AutoLock lock(context_lock_);
  // An error occurred. Need to get the host again to reinitialize it.
  if (!channel_.get())
    return NULL;

  GPUCreateCommandBufferConfig init_params;
  init_params.share_group_id =
      share_group ? share_group->route_id() : MSG_ROUTING_NONE;
  init_params.allowed_extensions = allowed_extensions;
  init_params.attribs = attribs;
  init_params.active_url = active_url;
  int32 route_id;
  if (!ChildThread::current()->Send(
      new GpuHostMsg_CreateViewCommandBuffer(
          render_view_id,
          init_params,
          &route_id))) {
    return NULL;
  }

  if (route_id == MSG_ROUTING_NONE)
    return NULL;

  CommandBufferProxy* command_buffer = new CommandBufferProxy(this, route_id);
  AddRoute(route_id, command_buffer->AsWeakPtr());
  proxies_[route_id] = command_buffer;
  return command_buffer;
#else
  return NULL;
#endif
}

GpuVideoDecodeAcceleratorHost* GpuChannelHost::CreateVideoDecoder(
    int command_buffer_route_id,
    media::VideoDecodeAccelerator::Profile profile,
    media::VideoDecodeAccelerator::Client* client) {
  AutoLock lock(context_lock_);
  ProxyMap::iterator it = proxies_.find(command_buffer_route_id);
  DCHECK(it != proxies_.end());
  CommandBufferProxy* proxy = it->second;
  return proxy->CreateVideoDecoder(profile, client);
}

CommandBufferProxy* GpuChannelHost::CreateOffscreenCommandBuffer(
    const gfx::Size& size,
    CommandBufferProxy* share_group,
    const std::string& allowed_extensions,
    const std::vector<int32>& attribs,
    const GURL& active_url) {
#if defined(ENABLE_GPU)
  AutoLock lock(context_lock_);
  // An error occurred. Need to get the host again to reinitialize it.
  if (!channel_.get())
    return NULL;

  GPUCreateCommandBufferConfig init_params;
  init_params.share_group_id =
      share_group ? share_group->route_id() : MSG_ROUTING_NONE;
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
  AddRoute(route_id, command_buffer->AsWeakPtr());
  proxies_[route_id] = command_buffer;
  return command_buffer;
#else
  return NULL;
#endif
}

void GpuChannelHost::DestroyCommandBuffer(CommandBufferProxy* command_buffer) {
#if defined(ENABLE_GPU)
  AutoLock lock(context_lock_);
  Send(new GpuChannelMsg_DestroyCommandBuffer(command_buffer->route_id()));

  // Check the proxy has not already been removed after a channel error.
  int route_id = command_buffer->route_id();
  if (proxies_.find(command_buffer->route_id()) != proxies_.end())
    proxies_.erase(route_id);
  RemoveRoute(route_id);
  delete command_buffer;
#endif
}

void GpuChannelHost::AddRoute(
    int route_id, base::WeakPtr<IPC::Channel::Listener> listener) {
  DCHECK(MessageLoopProxy::current());

  MessageLoopProxy* io_loop = RenderProcess::current()->io_message_loop_proxy();
  io_loop->PostTask(FROM_HERE,
                    NewRunnableMethod(
                        channel_filter_.get(),
                        &GpuChannelHost::MessageFilter::AddRoute,
                        route_id, listener, MessageLoopProxy::current()));
}

void GpuChannelHost::RemoveRoute(int route_id) {
  MessageLoopProxy* io_loop = RenderProcess::current()->io_message_loop_proxy();
  io_loop->PostTask(FROM_HERE,
                    NewRunnableMethod(
                        channel_filter_.get(),
                        &GpuChannelHost::MessageFilter::RemoveRoute,
                        route_id));
}
