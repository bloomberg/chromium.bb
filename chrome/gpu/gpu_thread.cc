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
#include "chrome/common/gpu_info.h"
#include "chrome/common/gpu_messages.h"
#include "chrome/gpu/gpu_info_collector.h"
#include "ipc/ipc_channel_handle.h"

#if defined(OS_WIN)
#include "chrome/gpu/gpu_view_win.h"
#elif defined(GPU_USE_GLX)
#include "chrome/gpu/gpu_backing_store_glx_context.h"
#include "chrome/gpu/gpu_view_x.h"

#include <X11/Xutil.h>  // Must be last.
#endif

#if defined(OS_LINUX)
#include <gtk/gtk.h>
#endif

GpuThread::GpuThread() {
#if defined(GPU_USE_GLX)
  display_ = ::XOpenDisplay(NULL);
#endif
#if defined(OS_LINUX)
  {
    // The X11 port of the command buffer code assumes it can access the X
    // display via the macro GDK_DISPLAY(), which implies that Gtk has been
    // initialized. This code was taken from PluginThread. TODO(kbr):
    // rethink whether initializing Gtk is really necessary or whether we
    // should just send the display connection down to the GPUProcessor.
    g_thread_init(NULL);
    const std::vector<std::string>& args =
        CommandLine::ForCurrentProcess()->argv();
    int argc = args.size();
    scoped_array<char *> argv(new char *[argc + 1]);
    for (size_t i = 0; i < args.size(); ++i) {
      // TODO(piman@google.com): can gtk_init modify argv? Just being safe
      // here.
      argv[i] = strdup(args[i].c_str());
    }
    argv[argc] = NULL;
    char **argv_pointer = argv.get();

    gtk_init(&argc, &argv_pointer);
    for (size_t i = 0; i < args.size(); ++i) {
      free(argv[i]);
    }
    x11_util::SetX11ErrorHandlers();
  }
#endif
}

GpuThread::~GpuThread() {
}

#if defined(GPU_USE_GLX)
GpuBackingStoreGLXContext* GpuThread::GetGLXContext() {
  if (!glx_context_.get())
    glx_context_.reset(new GpuBackingStoreGLXContext(this));
  return glx_context_.get();
}
#endif

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
    IPC_MESSAGE_HANDLER(GpuMsg_NewRenderWidgetHostView,
                        OnNewRenderWidgetHostView)
    IPC_MESSAGE_HANDLER(GpuMsg_CollectGraphicsInfo,
                        OnCollectGraphicsInfo)
  IPC_END_MESSAGE_MAP_EX()
}

void GpuThread::OnEstablishChannel(int renderer_id) {
  scoped_refptr<GpuChannel> channel;
  IPC::ChannelHandle channel_handle;
  GPUInfo gpu_info;

  // Fail to establish a channel if some implementation of GL cannot be
  // initialized.
  if (gfx::GLContext::InitializeOneOff()) {
    // Fail to establish channel if GPU stats cannot be retreived.
    if (gpu_info_collector::CollectGraphicsInfo(&gpu_info)) {
      GpuChannelMap::const_iterator iter = gpu_channels_.find(renderer_id);
      if (iter == gpu_channels_.end()) {
        channel = new GpuChannel(renderer_id);
      } else {
        channel = iter->second;
      }

      DCHECK(channel != NULL);

      if (channel->Init()) {
        gpu_channels_[renderer_id] = channel;
      } else {
        channel = NULL;
      }

      if (channel.get()) {
        channel_handle.name = channel->GetChannelName();
#if defined(OS_POSIX)
        // On POSIX, pass the renderer-side FD. Also mark it as auto-close so
        // that it gets closed after it has been sent.
        int renderer_fd = channel->DisownRendererFd();
        channel_handle.socket = base::FileDescriptor(renderer_fd, true);
#endif
      }
    }
  }

  Send(new GpuHostMsg_ChannelEstablished(channel_handle, gpu_info));
}

void GpuThread::OnSynchronize() {
  Send(new GpuHostMsg_SynchronizeReply());
}

void GpuThread::OnNewRenderWidgetHostView(GpuNativeWindowHandle parent_window,
                                          int32 routing_id) {
  // The GPUView class' lifetime is controlled by the host, which will send a
  // message to destroy the GpuRWHView when necessary. So we don't manage the
  // lifetime of this object.
#if defined(OS_WIN)
  new GpuViewWin(this, parent_window, routing_id);
#elif defined(GPU_USE_GLX)
  new GpuViewX(this, parent_window, routing_id);
#else
  NOTIMPLEMENTED();
#endif
}

void GpuThread::OnCollectGraphicsInfo() {
  // Fail to establish a channel if some implementation of GL cannot be
  // initialized.
  GPUInfo gpu_info;
  if (gfx::GLContext::InitializeOneOff()) {
    gpu_info_collector::CollectGraphicsInfo(&gpu_info);
  }

  Send(new GpuHostMsg_GraphicsInfoCollected(gpu_info));
}
