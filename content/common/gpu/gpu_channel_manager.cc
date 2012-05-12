// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/gpu/gpu_channel_manager.h"

#include "base/bind.h"
#include "content/common/child_thread.h"
#include "content/common/gpu/gpu_channel.h"
#include "content/common/gpu/gpu_memory_manager.h"
#include "content/common/gpu/gpu_messages.h"
#include "ui/gl/gl_share_group.h"

GpuChannelManager::GpuChannelManager(ChildThread* gpu_child_thread,
                                     GpuWatchdog* watchdog,
                                     base::MessageLoopProxy* io_message_loop,
                                     base::WaitableEvent* shutdown_event)
    : ALLOW_THIS_IN_INITIALIZER_LIST(weak_factory_(this)),
      io_message_loop_(io_message_loop),
      shutdown_event_(shutdown_event),
      gpu_child_thread_(gpu_child_thread),
      ALLOW_THIS_IN_INITIALIZER_LIST(gpu_memory_manager_(this,
          GpuMemoryManager::kDefaultMaxSurfacesWithFrontbufferSoftLimit)),
      watchdog_(watchdog) {
  DCHECK(gpu_child_thread);
  DCHECK(io_message_loop);
  DCHECK(shutdown_event);
}

GpuChannelManager::~GpuChannelManager() {
  gpu_channels_.clear();
}

void GpuChannelManager::RemoveChannel(int client_id) {
  gpu_channels_.erase(client_id);
}

int GpuChannelManager::GenerateRouteID() {
  static int last_id = 0;
  return ++last_id;
}

void GpuChannelManager::AddRoute(int32 routing_id,
                                 IPC::Channel::Listener* listener) {
  gpu_child_thread_->AddRoute(routing_id, listener);
}

void GpuChannelManager::RemoveRoute(int32 routing_id) {
  gpu_child_thread_->RemoveRoute(routing_id);
}

GpuChannel* GpuChannelManager::LookupChannel(int32 client_id) {
  GpuChannelMap::const_iterator iter = gpu_channels_.find(client_id);
  if (iter == gpu_channels_.end())
    return NULL;
  else
    return iter->second;
}

void GpuChannelManager::AppendAllCommandBufferStubs(
    std::vector<GpuCommandBufferStubBase*>& stubs) {
  for (GpuChannelMap::const_iterator it = gpu_channels_.begin();
      it != gpu_channels_.end(); ++it ) {
    it->second->AppendAllCommandBufferStubs(stubs);
  }

}

bool GpuChannelManager::OnMessageReceived(const IPC::Message& msg) {
  bool msg_is_ok = true;
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP_EX(GpuChannelManager, msg, msg_is_ok)
    IPC_MESSAGE_HANDLER(GpuMsg_EstablishChannel, OnEstablishChannel)
    IPC_MESSAGE_HANDLER(GpuMsg_CloseChannel, OnCloseChannel)
    IPC_MESSAGE_HANDLER(GpuMsg_CreateViewCommandBuffer,
                        OnCreateViewCommandBuffer)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP_EX()
  return handled;
}

bool GpuChannelManager::Send(IPC::Message* msg) {
  return gpu_child_thread_->Send(msg);
}

void GpuChannelManager::OnEstablishChannel(int client_id, bool share_context) {
  IPC::ChannelHandle channel_handle;

  gfx::GLShareGroup* share_group = NULL;
  if (share_context) {
    if (!share_group_)
      share_group_ = new gfx::GLShareGroup;
    share_group = share_group_;
  }

  scoped_refptr<GpuChannel> channel = new GpuChannel(this,
                                                     watchdog_,
                                                     share_group,
                                                     client_id,
                                                     false);
  if (channel->Init(io_message_loop_, shutdown_event_)) {
    gpu_channels_[client_id] = channel;
    channel_handle.name = channel->GetChannelName();

#if defined(OS_POSIX)
    // On POSIX, pass the renderer-side FD. Also mark it as auto-close so
    // that it gets closed after it has been sent.
    int renderer_fd = channel->TakeRendererFileDescriptor();
    DCHECK_NE(-1, renderer_fd);
    channel_handle.socket = base::FileDescriptor(renderer_fd, true);
#endif
  }

  Send(new GpuHostMsg_ChannelEstablished(channel_handle));
}

void GpuChannelManager::OnCloseChannel(
    const IPC::ChannelHandle& channel_handle) {
  for (GpuChannelMap::iterator iter = gpu_channels_.begin();
       iter != gpu_channels_.end(); ++iter) {
    if (iter->second->GetChannelName() == channel_handle.name) {
      gpu_channels_.erase(iter);
      return;
    }
  }
}

void GpuChannelManager::OnCreateViewCommandBuffer(
    const gfx::GLSurfaceHandle& window,
    int32 surface_id,
    int32 client_id,
    const GPUCreateCommandBufferConfig& init_params) {
  DCHECK(surface_id);
  int32 route_id = MSG_ROUTING_NONE;

  GpuChannelMap::const_iterator iter = gpu_channels_.find(client_id);
  if (iter != gpu_channels_.end()) {
    iter->second->CreateViewCommandBuffer(
        window, surface_id, init_params, &route_id);
  }

  Send(new GpuHostMsg_CommandBufferCreated(route_id));
}

void GpuChannelManager::LoseAllContexts() {
  MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&GpuChannelManager::OnLoseAllContexts,
                 weak_factory_.GetWeakPtr()));
}

void GpuChannelManager::OnLoseAllContexts() {
  gpu_channels_.clear();
}
