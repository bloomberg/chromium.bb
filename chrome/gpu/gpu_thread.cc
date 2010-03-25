// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/gpu/gpu_thread.h"

#include "build/build_config.h"
#include "chrome/common/child_process.h"
#include "chrome/common/gpu_messages.h"
#include "chrome/gpu/gpu_config.h"

#if defined(OS_WIN)
#include "chrome/gpu/gpu_view_win.h"
#elif defined(GPU_USE_GLX)
#include "chrome/gpu/gpu_backing_store_glx_context.h"
#include "chrome/gpu/gpu_view_x.h"

#include <X11/Xutil.h>  // Must be last.
#endif

GpuThread::GpuThread() {
#if defined(GPU_USE_GLX)
  display_ = ::XOpenDisplay(NULL);
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

void GpuThread::OnControlMessageReceived(const IPC::Message& msg) {
  bool msg_is_ok = true;
  IPC_BEGIN_MESSAGE_MAP_EX(GpuThread, msg, msg_is_ok)
    IPC_MESSAGE_HANDLER(GpuMsg_EstablishChannel,
                        OnEstablishChannel)
    IPC_MESSAGE_HANDLER(GpuMsg_NewRenderWidgetHostView,
                        OnNewRenderWidgetHostView)
  IPC_END_MESSAGE_MAP_EX()
}

void GpuThread::OnEstablishChannel(int renderer_id) {
  scoped_refptr<GpuChannel> channel;

  GpuChannelMap::const_iterator iter = gpu_channels_.find(renderer_id);
  if (iter == gpu_channels_.end()) {
    channel = new GpuChannel(renderer_id);
  } else {
    channel = iter->second;
  }

  DCHECK(channel != NULL);

  if (channel->Init()) {
    // TODO(apatrick): figure out when to remove channels from the map. They
    // will never be destroyed otherwise.
    gpu_channels_[renderer_id] = channel;
  } else {
    channel = NULL;
  }

  IPC::ChannelHandle channel_handle;
  if (channel.get()) {
    channel_handle.name = channel->GetChannelName();
#if defined(OS_POSIX)
    // On POSIX, pass the renderer-side FD. Also mark it as auto-close so that
    // it gets closed after it has been sent.
    int renderer_fd = channel->DisownRendererFd();
    channel_handle.socket = base::FileDescriptor(renderer_fd, true);
#endif
  }

  Send(new GpuHostMsg_ChannelEstablished(channel_handle));
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
