// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/gpu/client/gpu_channel_host.h"

#include "base/bind.h"
#include "base/debug/trace_event.h"
#include "base/message_loop.h"
#include "base/message_loop_proxy.h"
#include "base/threading/thread_restrictions.h"
#include "content/common/gpu/client/command_buffer_proxy_impl.h"
#include "content/common/gpu/gpu_messages.h"
#include "googleurl/src/gurl.h"
#include "ipc/ipc_sync_message_filter.h"

#if defined(OS_WIN)
#include "content/public/common/sandbox_init.h"
#endif

using base::AutoLock;
using base::MessageLoopProxy;

namespace content {

GpuListenerInfo::GpuListenerInfo() {}

GpuListenerInfo::~GpuListenerInfo() {}

GpuChannelHost::GpuChannelHost(
    GpuChannelHostFactory* factory, int gpu_host_id, int client_id)
    : factory_(factory),
      client_id_(client_id),
      gpu_host_id_(gpu_host_id),
      state_(kUnconnected) {
  next_transfer_buffer_id_.GetNext();
}

void GpuChannelHost::Connect(
    const IPC::ChannelHandle& channel_handle) {
  DCHECK(factory_->IsMainThread());
  // Open a channel to the GPU process. We pass NULL as the main listener here
  // since we need to filter everything to route it to the right thread.
  scoped_refptr<base::MessageLoopProxy> io_loop = factory_->GetIOLoopProxy();
  channel_.reset(new IPC::SyncChannel(
      channel_handle, IPC::Channel::MODE_CLIENT, NULL,
      io_loop, true,
      factory_->GetShutDownEvent()));

  sync_filter_ = new IPC::SyncMessageFilter(
      factory_->GetShutDownEvent());

  channel_->AddFilter(sync_filter_.get());

  channel_filter_ = new MessageFilter(this);

  // Install the filter last, because we intercept all leftover
  // messages.
  channel_->AddFilter(channel_filter_.get());

  // It is safe to send IPC messages before the channel completes the connection
  // and receives the hello message from the GPU process. The messages get
  // cached.
  state_ = kConnected;
}

void GpuChannelHost::set_gpu_info(const GPUInfo& gpu_info) {
  gpu_info_ = gpu_info;
}

void GpuChannelHost::SetStateLost() {
  state_ = kLost;
}

const GPUInfo& GpuChannelHost::gpu_info() const {
  return gpu_info_;
}

void GpuChannelHost::OnMessageReceived(const IPC::Message& message) {
    bool handled = true;

    IPC_BEGIN_MESSAGE_MAP(GpuChannelHost, message)
      IPC_MESSAGE_HANDLER(GpuChannelMsg_GenerateMailboxNamesReply,
                          OnGenerateMailboxNamesReply)
    IPC_MESSAGE_UNHANDLED(handled = false)
    IPC_END_MESSAGE_MAP()

    DCHECK(handled);
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
  // IsMainThread() since it might return false during shutdown,
  // impl we are actually calling from the main thread (discard message then).
  //
  // TODO: Can we just always use sync_filter_ since we setup the channel
  //       without a main listener?
  if (factory_->IsMainThread()) {
    if (channel_.get()) {
      // http://crbug.com/125264
      base::ThreadRestrictions::ScopedAllowWait allow_wait;
      return channel_->Send(message);
    }
  } else if (MessageLoop::current()) {
    return sync_filter_->Send(message);
  }

  // Callee takes ownership of message, regardless of whether Send is
  // successful. See IPC::Sender.
  delete message;
  return false;
}

CommandBufferProxyImpl* GpuChannelHost::CreateViewCommandBuffer(
    int32 surface_id,
    CommandBufferProxyImpl* share_group,
    const std::string& allowed_extensions,
    const std::vector<int32>& attribs,
    const GURL& active_url,
    gfx::GpuPreference gpu_preference) {
  TRACE_EVENT1("gpu",
               "GpuChannelHost::CreateViewCommandBuffer",
               "surface_id",
               surface_id);

  AutoLock lock(context_lock_);
  // An error occurred. Need to get the host again to reinitialize it.
  if (!channel_.get())
    return NULL;

  GPUCreateCommandBufferConfig init_params;
  init_params.share_group_id =
      share_group ? share_group->GetRouteID() : MSG_ROUTING_NONE;
  init_params.allowed_extensions = allowed_extensions;
  init_params.attribs = attribs;
  init_params.active_url = active_url;
  init_params.gpu_preference = gpu_preference;
  int32 route_id = factory_->CreateViewCommandBuffer(surface_id, init_params);
  if (route_id == MSG_ROUTING_NONE)
    return NULL;

  CommandBufferProxyImpl* command_buffer =
      new CommandBufferProxyImpl(this, route_id);
  AddRoute(route_id, command_buffer->AsWeakPtr());
  proxies_[route_id] = command_buffer;
  return command_buffer;
}

CommandBufferProxyImpl* GpuChannelHost::CreateOffscreenCommandBuffer(
    const gfx::Size& size,
    CommandBufferProxyImpl* share_group,
    const std::string& allowed_extensions,
    const std::vector<int32>& attribs,
    const GURL& active_url,
    gfx::GpuPreference gpu_preference) {
  TRACE_EVENT0("gpu", "GpuChannelHost::CreateOffscreenCommandBuffer");

  AutoLock lock(context_lock_);
  // An error occurred. Need to get the host again to reinitialize it.
  if (!channel_.get())
    return NULL;

  GPUCreateCommandBufferConfig init_params;
  init_params.share_group_id =
      share_group ? share_group->GetRouteID() : MSG_ROUTING_NONE;
  init_params.allowed_extensions = allowed_extensions;
  init_params.attribs = attribs;
  init_params.active_url = active_url;
  init_params.gpu_preference = gpu_preference;
  int32 route_id;
  if (!Send(new GpuChannelMsg_CreateOffscreenCommandBuffer(size,
                                                           init_params,
                                                           &route_id))) {
    return NULL;
  }

  if (route_id == MSG_ROUTING_NONE)
    return NULL;

  CommandBufferProxyImpl* command_buffer =
      new CommandBufferProxyImpl(this, route_id);
  AddRoute(route_id, command_buffer->AsWeakPtr());
  proxies_[route_id] = command_buffer;
  return command_buffer;
}

GpuVideoDecodeAcceleratorHost* GpuChannelHost::CreateVideoDecoder(
    int command_buffer_route_id,
    media::VideoCodecProfile profile,
    media::VideoDecodeAccelerator::Client* client) {
  AutoLock lock(context_lock_);
  ProxyMap::iterator it = proxies_.find(command_buffer_route_id);
  DCHECK(it != proxies_.end());
  CommandBufferProxyImpl* proxy = it->second;
  return proxy->CreateVideoDecoder(profile, client);
}

void GpuChannelHost::DestroyCommandBuffer(
    CommandBufferProxyImpl* command_buffer) {
  TRACE_EVENT0("gpu", "GpuChannelHost::DestroyCommandBuffer");

  AutoLock lock(context_lock_);
  int route_id = command_buffer->GetRouteID();
  Send(new GpuChannelMsg_DestroyCommandBuffer(route_id));
  // Check the proxy has not already been removed after a channel error.
  if (proxies_.find(route_id) != proxies_.end())
    proxies_.erase(route_id);
  RemoveRoute(route_id);
  delete command_buffer;
}

bool GpuChannelHost::CollectRenderingStatsForSurface(
    int surface_id, GpuRenderingStats* stats) {
  TRACE_EVENT0("gpu", "GpuChannelHost::CollectRenderingStats");

  return Send(new GpuChannelMsg_CollectRenderingStatsForSurface(surface_id,
                                                                stats));
}

void GpuChannelHost::AddRoute(
    int route_id, base::WeakPtr<IPC::Listener> listener) {
  DCHECK(MessageLoopProxy::current());

  scoped_refptr<base::MessageLoopProxy> io_loop = factory_->GetIOLoopProxy();
  io_loop->PostTask(FROM_HERE,
                    base::Bind(&GpuChannelHost::MessageFilter::AddRoute,
                               channel_filter_.get(), route_id, listener,
                               MessageLoopProxy::current()));
}

void GpuChannelHost::RemoveRoute(int route_id) {
  scoped_refptr<base::MessageLoopProxy> io_loop = factory_->GetIOLoopProxy();
  io_loop->PostTask(FROM_HERE,
                    base::Bind(&GpuChannelHost::MessageFilter::RemoveRoute,
                               channel_filter_.get(), route_id));
}

base::SharedMemoryHandle GpuChannelHost::ShareToGpuProcess(
    base::SharedMemory* shared_memory) {
  AutoLock lock(context_lock_);

  if (!channel_.get())
    return base::SharedMemory::NULLHandle();

  base::SharedMemoryHandle handle;
#if defined(OS_WIN)
  // Windows needs to explicitly duplicate the handle out to another process.
  if (!BrokerDuplicateHandle(shared_memory->handle(),
                             channel_->peer_pid(),
                             &handle,
                             FILE_MAP_WRITE,
                             0)) {
    return base::SharedMemory::NULLHandle();
  }
#else
  if (!shared_memory->ShareToProcess(channel_->peer_pid(), &handle))
    return base::SharedMemory::NULLHandle();
#endif

  return handle;
}

bool GpuChannelHost::GenerateMailboxNames(unsigned num,
                                          std::vector<std::string>* names) {
  TRACE_EVENT0("gpu", "GenerateMailboxName");
  AutoLock lock(context_lock_);

  if (num > mailbox_name_pool_.size()) {
    if (!Send(new GpuChannelMsg_GenerateMailboxNames(num, names)))
      return false;
  } else {
    names->insert(names->begin(),
                  mailbox_name_pool_.end() - num,
                  mailbox_name_pool_.end());
    mailbox_name_pool_.erase(mailbox_name_pool_.end() - num,
                             mailbox_name_pool_.end());
  }

  const unsigned ideal_mailbox_pool_size = 100;
  if (mailbox_name_pool_.size() < ideal_mailbox_pool_size / 2) {
    Send(new GpuChannelMsg_GenerateMailboxNamesAsync(
        ideal_mailbox_pool_size - mailbox_name_pool_.size()));
  }

  return true;
}

void GpuChannelHost::OnGenerateMailboxNamesReply(
    const std::vector<std::string>& names) {
  TRACE_EVENT0("gpu", "OnGenerateMailboxNamesReply");
  AutoLock lock(context_lock_);

  mailbox_name_pool_.insert(mailbox_name_pool_.end(),
                            names.begin(),
                            names.end());
}

int32 GpuChannelHost::ReserveTransferBufferId() {
  return next_transfer_buffer_id_.GetNext();
}

GpuChannelHost::~GpuChannelHost() {}


GpuChannelHost::MessageFilter::MessageFilter(GpuChannelHost* parent)
    : parent_(parent) {
}

GpuChannelHost::MessageFilter::~MessageFilter() {}

void GpuChannelHost::MessageFilter::AddRoute(
    int route_id,
    base::WeakPtr<IPC::Listener> listener,
    scoped_refptr<MessageLoopProxy> loop) {
  DCHECK(parent_->factory_->IsIOThread());
  DCHECK(listeners_.find(route_id) == listeners_.end());
  GpuListenerInfo info;
  info.listener = listener;
  info.loop = loop;
  listeners_[route_id] = info;
}

void GpuChannelHost::MessageFilter::RemoveRoute(int route_id) {
  DCHECK(parent_->factory_->IsIOThread());
  ListenerMap::iterator it = listeners_.find(route_id);
  if (it != listeners_.end())
    listeners_.erase(it);
}

bool GpuChannelHost::MessageFilter::OnMessageReceived(
    const IPC::Message& message) {
  DCHECK(parent_->factory_->IsIOThread());

  // Never handle sync message replies or we will deadlock here.
  if (message.is_reply())
    return false;

  if (message.routing_id() == MSG_ROUTING_CONTROL) {
    MessageLoop* main_loop = parent_->factory_->GetMainLoop();
    main_loop->PostTask(FROM_HERE,
                        base::Bind(&GpuChannelHost::OnMessageReceived,
                                   parent_,
                                   message));
    return true;
  }

  ListenerMap::iterator it = listeners_.find(message.routing_id());

  if (it != listeners_.end()) {
    const GpuListenerInfo& info = it->second;
    info.loop->PostTask(
        FROM_HERE,
        base::Bind(
            base::IgnoreResult(&IPC::Listener::OnMessageReceived),
            info.listener,
            message));
  }

  return true;
}

void GpuChannelHost::MessageFilter::OnChannelError() {
  DCHECK(parent_->factory_->IsIOThread());

  // Post the task to signal the GpuChannelHost before the proxies. That way, if
  // they themselves post a task to recreate the context, they will not try to
  // re-use this channel host before it has a chance to mark itself lost.
  MessageLoop* main_loop = parent_->factory_->GetMainLoop();
  main_loop->PostTask(FROM_HERE,
                      base::Bind(&GpuChannelHost::OnChannelError, parent_));
  // Inform all the proxies that an error has occurred. This will be reported
  // via OpenGL as a lost context.
  for (ListenerMap::iterator it = listeners_.begin();
       it != listeners_.end();
       it++) {
    const GpuListenerInfo& info = it->second;
    info.loop->PostTask(
        FROM_HERE,
        base::Bind(&IPC::Listener::OnChannelError, info.listener));
  }

  listeners_.clear();
}

}  // namespace content
