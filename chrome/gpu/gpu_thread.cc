// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/gpu/gpu_thread.h"

#include <string>
#include <vector>

#include "app/gfx/gl/gl_context.h"
#include "base/command_line.h"
#include "build/build_config.h"
#include "chrome/common/child_process.h"
#include "chrome/common/child_process_logging.h"
#include "chrome/common/gpu_messages.h"
#include "chrome/gpu/gpu_info_collector.h"
#include "ipc/ipc_channel_handle.h"

#if defined(TOOLKIT_USES_GTK)
#include <gtk/gtk.h>
#include "app/x11_util.h"
#include "gfx/gtk_util.h"
#endif

GpuThread::GpuThread() {
#if defined(OS_LINUX)
  {
    // The X11 port of the command buffer code assumes it can access the X
    // display via the macro GDK_DISPLAY(), which implies that Gtk has been
    // initialized. This code was taken from PluginThread. TODO(kbr):
    // rethink whether initializing Gtk is really necessary or whether we
    // should just send a raw display connection down to the GPUProcessor.
    g_thread_init(NULL);
    gfx::GtkInitFromCommandLine(*CommandLine::ForCurrentProcess());
    x11_util::SetDefaultX11ErrorHandlers();
  }
#endif
}

GpuThread::~GpuThread() {
}

void GpuThread::Init(const base::Time& process_start_time) {
  gpu_info_collector::CollectGraphicsInfo(&gpu_info_);
  child_process_logging::SetGpuInfo(gpu_info_);

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

void GpuThread::OnCrash() {
  // Good bye, cruel world.
  volatile int* it_s_the_end_of_the_world_as_we_know_it = NULL;
  *it_s_the_end_of_the_world_as_we_know_it = 0xdead;
}

void GpuThread::OnHang() {
  for (;;)
    PlatformThread::Sleep(1000);
}
