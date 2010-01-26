// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/gpu/gpu_thread.h"

#include "build/build_config.h"
#include "chrome/common/gpu_messages.h"

#if defined(OS_WIN)
#include "chrome/gpu/gpu_view_win.h"
#elif defined(OS_LINUX)
#include "chrome/gpu/gpu_backing_store_glx_context.h"
#include "chrome/gpu/gpu_view_x.h"

#include <X11/Xutil.h>  // Must be last.
#endif

GpuThread::GpuThread() {
#if defined(OS_LINUX)
  display_ = ::XOpenDisplay(NULL);
#endif
}

GpuThread::~GpuThread() {
}

#if defined(OS_LINUX)
GpuBackingStoreGLXContext* GpuThread::GetGLXContext() {
  if (!glx_context_.get())
    glx_context_.reset(new GpuBackingStoreGLXContext(this));
  return glx_context_.get();
}
#endif

void GpuThread::OnControlMessageReceived(const IPC::Message& msg) {
  bool msg_is_ok = true;
  IPC_BEGIN_MESSAGE_MAP_EX(GpuThread, msg, msg_is_ok)
    IPC_MESSAGE_HANDLER(GpuMsg_NewRenderWidgetHostView,
                        OnNewRenderWidgetHostView)
  IPC_END_MESSAGE_MAP_EX()
}

void GpuThread::OnNewRenderWidgetHostView(GpuNativeWindowHandle parent_window,
                                          int32 routing_id) {
  // The GPUView class' lifetime is controlled by the host, which will send a
  // message to destroy the GpuRWHView when necessary. So we don't manage the
  // lifetime of this object.
#if defined(OS_WIN)
  new GpuViewWin(this, parent_window, routing_id);
#elif defined(OS_LINUX)
  new GpuViewX(this, parent_window, routing_id);
#else
  NOTIMPLEMENTED();
#endif
}
