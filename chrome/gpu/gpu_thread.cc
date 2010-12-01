// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/gpu/gpu_thread.h"

#include <string>
#include <vector>

#include "app/gfx/gl/gl_context.h"
#include "base/command_line.h"
#include "base/worker_pool.h"
#include "build/build_config.h"
#include "chrome/common/child_process.h"
#include "chrome/common/child_process_logging.h"
#include "chrome/common/gpu_messages.h"
#include "chrome/gpu/gpu_info_collector.h"
#include "ipc/ipc_channel_handle.h"

#if defined(OS_WIN)
#include "app/win_util.h"
#endif

GpuThread::GpuThread() {
}

GpuThread::~GpuThread() {
}

void GpuThread::Init(const base::Time& process_start_time) {
  gpu_info_collector::CollectGraphicsInfo(&gpu_info_);
  child_process_logging::SetGpuInfo(gpu_info_);

#if defined(OS_WIN)
  // Asynchronously collect the DirectX diagnostics because this can take a
  // couple of seconds.
  if (!WorkerPool::PostTask(
      FROM_HERE,
      NewRunnableFunction(&GpuThread::CollectDxDiagnostics, this),
      true)) {
    // Flag GPU info as complete if the DirectX diagnostics cannot be collected.
    gpu_info_.SetProgress(GPUInfo::kComplete);
  }
#endif

  // Record initialization only after collecting the GPU info because that can
  // take a significant amount of time.
  gpu_info_.SetInitializationTime(base::Time::Now() - process_start_time);
}

void GpuThread::RemoveChannel(int renderer_id) {
  gpu_channels_.erase(renderer_id);
}

void GpuThread::OnControlMessageReceived(const IPC::Message& msg) {
  bool msg_is_ok = true;
  IPC_BEGIN_MESSAGE_MAP_EX(GpuThread, msg, msg_is_ok)
    IPC_MESSAGE_HANDLER(GpuMsg_EstablishChannel,
                        OnEstablishChannel)
    IPC_MESSAGE_HANDLER(GpuMsg_Synchronize,
                        OnSynchronize)
    IPC_MESSAGE_HANDLER(GpuMsg_CollectGraphicsInfo,
                        OnCollectGraphicsInfo)
#if defined(OS_MACOSX)
    IPC_MESSAGE_HANDLER(GpuMsg_AcceleratedSurfaceBuffersSwappedACK,
                        OnAcceleratedSurfaceBuffersSwappedACK)
#endif
    IPC_MESSAGE_HANDLER(GpuMsg_Crash,
                        OnCrash)
    IPC_MESSAGE_HANDLER(GpuMsg_Hang,
                        OnHang)
  IPC_END_MESSAGE_MAP_EX()
}

void GpuThread::OnEstablishChannel(int renderer_id) {
  scoped_refptr<GpuChannel> channel;
  IPC::ChannelHandle channel_handle;
  GPUInfo gpu_info;

  GpuChannelMap::const_iterator iter = gpu_channels_.find(renderer_id);
  if (iter == gpu_channels_.end())
    channel = new GpuChannel(renderer_id);
  else
    channel = iter->second;

  DCHECK(channel != NULL);

  if (channel->Init())
    gpu_channels_[renderer_id] = channel;
  else
    channel = NULL;

  if (channel.get()) {
    channel_handle.name = channel->GetChannelName();
#if defined(OS_POSIX)
    // On POSIX, pass the renderer-side FD. Also mark it as auto-close so
    // that it gets closed after it has been sent.
    int renderer_fd = channel->DisownRendererFd();
    channel_handle.socket = base::FileDescriptor(renderer_fd, true);
#endif
  }

  Send(new GpuHostMsg_ChannelEstablished(channel_handle, gpu_info_));
}

void GpuThread::OnSynchronize() {
  Send(new GpuHostMsg_SynchronizeReply());
}

void GpuThread::OnCollectGraphicsInfo() {
  Send(new GpuHostMsg_GraphicsInfoCollected(gpu_info_));
}

#if defined(OS_MACOSX)
void GpuThread::OnAcceleratedSurfaceBuffersSwappedACK(
    int renderer_id, int32 route_id, uint64 swap_buffers_count) {
  GpuChannelMap::const_iterator iter = gpu_channels_.find(renderer_id);
  if (iter == gpu_channels_.end())
    return;
  scoped_refptr<GpuChannel> channel = iter->second;
  channel->AcceleratedSurfaceBuffersSwapped(route_id, swap_buffers_count);
}
#endif

void GpuThread::OnCrash() {
  // Good bye, cruel world.
  volatile int* it_s_the_end_of_the_world_as_we_know_it = NULL;
  *it_s_the_end_of_the_world_as_we_know_it = 0xdead;
}

void GpuThread::OnHang() {
  for (;;) {
    // Do not sleep here. The GPU watchdog timer tracks the amount of user
    // time this thread is using and it doesn't use much while calling Sleep.
  }
}

#if defined(OS_WIN)

// Runs on a worker thread. The GpuThread never terminates voluntarily so it is
// safe to assume that its message loop is valid.
void GpuThread::CollectDxDiagnostics(GpuThread* thread) {
  win_util::ScopedCOMInitializer com_initializer;

  DxDiagNode node;
  gpu_info_collector::GetDxDiagnostics(&node);

  thread->message_loop()->PostTask(
      FROM_HERE,
      NewRunnableFunction(&GpuThread::SetDxDiagnostics, thread, node));
}

// Runs on the GPU thread.
void GpuThread::SetDxDiagnostics(GpuThread* thread, const DxDiagNode& node) {
  thread->gpu_info_.SetDxDiagnostics(node);
  thread->gpu_info_.SetProgress(GPUInfo::kComplete);
}

#endif
