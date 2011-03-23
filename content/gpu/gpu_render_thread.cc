// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/gpu/gpu_render_thread.h"

#include <string>
#include <vector>

#include "app/gfx/gl/gl_context.h"
#include "app/gfx/gl/gl_implementation.h"
#include "app/win/scoped_com_initializer.h"
#include "base/command_line.h"
#include "base/threading/worker_pool.h"
#include "build/build_config.h"
#include "chrome/common/child_process_logging.h"
#include "chrome/common/chrome_switches.h"
#include "content/common/child_process.h"
#include "content/common/gpu_messages.h"
#include "content/gpu/gpu_child_thread.h"
#include "content/gpu/gpu_info_collector.h"
#include "content/gpu/gpu_watchdog_thread.h"
#include "ipc/ipc_channel_handle.h"

GpuRenderThread::GpuRenderThread(IPC::Message::Sender* browser_channel,
                                 GpuWatchdogThread* gpu_watchdog_thread,
                                 MessageLoop* io_message_loop,
                                 base::WaitableEvent* shutdown_event)
    : io_message_loop_(io_message_loop),
      shutdown_event_(shutdown_event),
      browser_channel_(browser_channel),
      watchdog_thread_(gpu_watchdog_thread) {
  DCHECK(browser_channel);
  DCHECK(io_message_loop);
  DCHECK(shutdown_event);
}

GpuRenderThread::~GpuRenderThread() {
  gpu_channels_.clear();
}

void GpuRenderThread::RemoveChannel(int renderer_id) {
  gpu_channels_.erase(renderer_id);
}

bool GpuRenderThread::OnMessageReceived(const IPC::Message& msg) {
  bool msg_is_ok = true;
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP_EX(GpuRenderThread, msg, msg_is_ok)
    IPC_MESSAGE_HANDLER(GpuMsg_EstablishChannel, OnEstablishChannel)
    IPC_MESSAGE_HANDLER(GpuMsg_CloseChannel, OnCloseChannel)
    IPC_MESSAGE_HANDLER(GpuMsg_CreateViewCommandBuffer,
                        OnCreateViewCommandBuffer);
    IPC_MESSAGE_HANDLER(GpuMsg_Synchronize, OnSynchronize)
#if defined(OS_MACOSX)
    IPC_MESSAGE_HANDLER(GpuMsg_AcceleratedSurfaceBuffersSwappedACK,
                        OnAcceleratedSurfaceBuffersSwappedACK)
    IPC_MESSAGE_HANDLER(GpuMsg_DestroyCommandBuffer,
                        OnDestroyCommandBuffer)
#endif
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP_EX()
  return handled;
}

bool GpuRenderThread::Send(IPC::Message* msg) {
  return browser_channel_->Send(msg);
}

void GpuRenderThread::OnEstablishChannel(int renderer_id) {
  scoped_refptr<GpuChannel> channel;
  IPC::ChannelHandle channel_handle;
  GPUInfo gpu_info;

  GpuChannelMap::const_iterator iter = gpu_channels_.find(renderer_id);
  if (iter == gpu_channels_.end())
    channel = new GpuChannel(this, watchdog_thread_, renderer_id);
  else
    channel = iter->second;

  DCHECK(channel != NULL);

  if (channel->Init(io_message_loop_, shutdown_event_))
    gpu_channels_[renderer_id] = channel;
  else
    channel = NULL;

  if (channel.get()) {
    channel_handle.name = channel->GetChannelName();
#if defined(OS_POSIX)
    // On POSIX, pass the renderer-side FD. Also mark it as auto-close so
    // that it gets closed after it has been sent.
    int renderer_fd = channel->GetRendererFileDescriptor();
    channel_handle.socket = base::FileDescriptor(dup(renderer_fd), true);
#endif
  }

  Send(new GpuHostMsg_ChannelEstablished(channel_handle));
}

void GpuRenderThread::OnCloseChannel(const IPC::ChannelHandle& channel_handle) {
  for (GpuChannelMap::iterator iter = gpu_channels_.begin();
       iter != gpu_channels_.end(); ++iter) {
    if (iter->second->GetChannelName() == channel_handle.name) {
      gpu_channels_.erase(iter);
      return;
    }
  }
}

void GpuRenderThread::OnSynchronize() {
  Send(new GpuHostMsg_SynchronizeReply());
}

void GpuRenderThread::OnCreateViewCommandBuffer(
    gfx::PluginWindowHandle window,
    int32 render_view_id,
    int32 renderer_id,
    const GPUCreateCommandBufferConfig& init_params) {
  int32 route_id = MSG_ROUTING_NONE;

  GpuChannelMap::const_iterator iter = gpu_channels_.find(renderer_id);
  if (iter != gpu_channels_.end()) {
    iter->second->CreateViewCommandBuffer(
        window, render_view_id, init_params, &route_id);
  }

  Send(new GpuHostMsg_CommandBufferCreated(route_id));
}

#if defined(OS_MACOSX)
void GpuRenderThread::OnAcceleratedSurfaceBuffersSwappedACK(
    int renderer_id, int32 route_id, uint64 swap_buffers_count) {
  GpuChannelMap::const_iterator iter = gpu_channels_.find(renderer_id);
  if (iter == gpu_channels_.end())
    return;
  scoped_refptr<GpuChannel> channel = iter->second;
  channel->AcceleratedSurfaceBuffersSwapped(route_id, swap_buffers_count);
}
void GpuRenderThread::OnDestroyCommandBuffer(
    int renderer_id, int32 renderer_view_id) {
  GpuChannelMap::const_iterator iter = gpu_channels_.find(renderer_id);
  if (iter == gpu_channels_.end())
    return;
  scoped_refptr<GpuChannel> channel = iter->second;
  channel->DestroyCommandBufferByViewId(renderer_view_id);
}
#endif
