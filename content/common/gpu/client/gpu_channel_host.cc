// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/gpu/client/gpu_channel_host.h"

#include <algorithm>

#include "base/bind.h"
#include "base/debug/trace_event.h"
#include "base/message_loop/message_loop.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/posix/eintr_wrapper.h"
#include "base/threading/thread_restrictions.h"
#include "content/common/gpu/client/command_buffer_proxy_impl.h"
#include "content/common/gpu/gpu_messages.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "ipc/ipc_sync_message_filter.h"
#include "url/gurl.h"

#if defined(OS_WIN)
#include "content/public/common/sandbox_init.h"
#endif

using base::AutoLock;
using base::MessageLoopProxy;

namespace content {

GpuListenerInfo::GpuListenerInfo() {}

GpuListenerInfo::~GpuListenerInfo() {}

// static
scoped_refptr<GpuChannelHost> GpuChannelHost::Create(
    GpuChannelHostFactory* factory,
    int gpu_host_id,
    int client_id,
    const gpu::GPUInfo& gpu_info,
    const IPC::ChannelHandle& channel_handle) {
  DCHECK(factory->IsMainThread());
  scoped_refptr<GpuChannelHost> host = new GpuChannelHost(
      factory, gpu_host_id, client_id, gpu_info);
  host->Connect(channel_handle);
  return host;
}

GpuChannelHost::GpuChannelHost(GpuChannelHostFactory* factory,
                               int gpu_host_id,
                               int client_id,
                               const gpu::GPUInfo& gpu_info)
    : factory_(factory),
      client_id_(client_id),
      gpu_host_id_(gpu_host_id),
      gpu_info_(gpu_info) {
  next_transfer_buffer_id_.GetNext();
}

void GpuChannelHost::Connect(const IPC::ChannelHandle& channel_handle) {
  // Open a channel to the GPU process. We pass NULL as the main listener here
  // since we need to filter everything to route it to the right thread.
  scoped_refptr<base::MessageLoopProxy> io_loop = factory_->GetIOLoopProxy();
  channel_.reset(new IPC::SyncChannel(channel_handle,
                                      IPC::Channel::MODE_CLIENT,
                                      NULL,
                                      io_loop.get(),
                                      true,
                                      factory_->GetShutDownEvent()));

  sync_filter_ = new IPC::SyncMessageFilter(
      factory_->GetShutDownEvent());

  channel_->AddFilter(sync_filter_.get());

  channel_filter_ = new MessageFilter();

  // Install the filter last, because we intercept all leftover
  // messages.
  channel_->AddFilter(channel_filter_.get());
}

bool GpuChannelHost::Send(IPC::Message* msg) {
  // Callee takes ownership of message, regardless of whether Send is
  // successful. See IPC::Sender.
  scoped_ptr<IPC::Message> message(msg);
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
    // http://crbug.com/125264
    base::ThreadRestrictions::ScopedAllowWait allow_wait;
    return channel_->Send(message.release());
  } else if (base::MessageLoop::current()) {
    return sync_filter_->Send(message.release());
  }

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

  AutoLock lock(context_lock_);
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

  AutoLock lock(context_lock_);
  proxies_[route_id] = command_buffer;
  return command_buffer;
}

scoped_ptr<media::VideoDecodeAccelerator> GpuChannelHost::CreateVideoDecoder(
    int command_buffer_route_id,
    media::VideoCodecProfile profile,
    media::VideoDecodeAccelerator::Client* client) {
  AutoLock lock(context_lock_);
  ProxyMap::iterator it = proxies_.find(command_buffer_route_id);
  DCHECK(it != proxies_.end());
  CommandBufferProxyImpl* proxy = it->second;
  return proxy->CreateVideoDecoder(profile, client).Pass();
}

void GpuChannelHost::DestroyCommandBuffer(
    CommandBufferProxyImpl* command_buffer) {
  TRACE_EVENT0("gpu", "GpuChannelHost::DestroyCommandBuffer");

  int route_id = command_buffer->GetRouteID();
  Send(new GpuChannelMsg_DestroyCommandBuffer(route_id));
  RemoveRoute(route_id);

  AutoLock lock(context_lock_);
  proxies_.erase(route_id);
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
  DCHECK(MessageLoopProxy::current().get());

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
    base::SharedMemoryHandle source_handle) {
  if (IsLost())
    return base::SharedMemory::NULLHandle();

#if defined(OS_WIN)
  // Windows needs to explicitly duplicate the handle out to another process.
  base::SharedMemoryHandle target_handle;
  if (!BrokerDuplicateHandle(source_handle,
                             channel_->peer_pid(),
                             &target_handle,
                             0,
                             DUPLICATE_SAME_ACCESS)) {
    return base::SharedMemory::NULLHandle();
  }

  return target_handle;
#else
  int duped_handle = HANDLE_EINTR(dup(source_handle.fd));
  if (duped_handle < 0)
    return base::SharedMemory::NULLHandle();

  return base::FileDescriptor(duped_handle, true);
#endif
}

bool GpuChannelHost::GenerateMailboxNames(unsigned num,
                                          std::vector<gpu::Mailbox>* names) {
  DCHECK(names->empty());
  TRACE_EVENT0("gpu", "GenerateMailboxName");
  size_t generate_count = channel_filter_->GetMailboxNames(num, names);

  if (names->size() < num) {
    std::vector<gpu::Mailbox> new_names;
    if (!Send(new GpuChannelMsg_GenerateMailboxNames(num - names->size(),
                                                     &new_names)))
      return false;
    names->insert(names->end(), new_names.begin(), new_names.end());
  }

  if (generate_count > 0)
    Send(new GpuChannelMsg_GenerateMailboxNamesAsync(generate_count));

  return true;
}

int32 GpuChannelHost::ReserveTransferBufferId() {
  return next_transfer_buffer_id_.GetNext();
}

GpuChannelHost::~GpuChannelHost() {
  // channel_ must be destroyed on the main thread.
  if (!factory_->IsMainThread())
    factory_->GetMainLoop()->DeleteSoon(FROM_HERE, channel_.release());
}


GpuChannelHost::MessageFilter::MessageFilter()
    : lost_(false),
      requested_mailboxes_(0) {
}

GpuChannelHost::MessageFilter::~MessageFilter() {}

void GpuChannelHost::MessageFilter::AddRoute(
    int route_id,
    base::WeakPtr<IPC::Listener> listener,
    scoped_refptr<MessageLoopProxy> loop) {
  DCHECK(listeners_.find(route_id) == listeners_.end());
  GpuListenerInfo info;
  info.listener = listener;
  info.loop = loop;
  listeners_[route_id] = info;
}

void GpuChannelHost::MessageFilter::RemoveRoute(int route_id) {
  ListenerMap::iterator it = listeners_.find(route_id);
  if (it != listeners_.end())
    listeners_.erase(it);
}

bool GpuChannelHost::MessageFilter::OnMessageReceived(
    const IPC::Message& message) {
  // Never handle sync message replies or we will deadlock here.
  if (message.is_reply())
    return false;

  if (message.routing_id() == MSG_ROUTING_CONTROL)
    return OnControlMessageReceived(message);

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
  // Set the lost state before signalling the proxies. That way, if they
  // themselves post a task to recreate the context, they will not try to re-use
  // this channel host.
  {
    AutoLock lock(lock_);
    lost_ = true;
  }

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

bool GpuChannelHost::MessageFilter::IsLost() const {
  AutoLock lock(lock_);
  return lost_;
}

size_t GpuChannelHost::MessageFilter::GetMailboxNames(
    size_t num, std::vector<gpu::Mailbox>* names) {
  AutoLock lock(lock_);
  size_t count = std::min(num, mailbox_name_pool_.size());
  names->insert(names->begin(),
                mailbox_name_pool_.end() - count,
                mailbox_name_pool_.end());
  mailbox_name_pool_.erase(mailbox_name_pool_.end() - count,
                           mailbox_name_pool_.end());

  const size_t ideal_mailbox_pool_size = 100;
  size_t total = mailbox_name_pool_.size() + requested_mailboxes_;
  DCHECK_LE(total, ideal_mailbox_pool_size);
  if (total >= ideal_mailbox_pool_size / 2)
    return 0;
  size_t request = ideal_mailbox_pool_size - total;
  requested_mailboxes_ += request;
  return request;
}

bool GpuChannelHost::MessageFilter::OnControlMessageReceived(
    const IPC::Message& message) {
  bool handled = true;

  IPC_BEGIN_MESSAGE_MAP(GpuChannelHost::MessageFilter, message)
  IPC_MESSAGE_HANDLER(GpuChannelMsg_GenerateMailboxNamesReply,
                      OnGenerateMailboxNamesReply)
  IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  DCHECK(handled);
  return handled;
}

void GpuChannelHost::MessageFilter::OnGenerateMailboxNamesReply(
    const std::vector<gpu::Mailbox>& names) {
  TRACE_EVENT0("gpu", "OnGenerateMailboxNamesReply");
  AutoLock lock(lock_);
  DCHECK_LE(names.size(), requested_mailboxes_);
  requested_mailboxes_ -= names.size();
  mailbox_name_pool_.insert(mailbox_name_pool_.end(),
                            names.begin(),
                            names.end());
}


}  // namespace content
