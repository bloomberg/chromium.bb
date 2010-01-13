// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/gpu/gpu_thread.h"

#include "build/build_config.h"
#include "chrome/common/gpu_messages.h"

#if defined(OS_WIN)
#include "chrome/gpu/gpu_view_win.h"
#endif

GpuThread::GpuThread() {
}

GpuThread::~GpuThread() {
}

void GpuThread::OnControlMessageReceived(const IPC::Message& msg) {
  bool msg_is_ok = true;
  IPC_BEGIN_MESSAGE_MAP_EX(GpuThread, msg, msg_is_ok)
    IPC_MESSAGE_HANDLER(GpuMsg_NewRenderWidgetHostView,
                        OnNewRenderWidgetHostView)
  IPC_END_MESSAGE_MAP_EX()
}

void GpuThread::OnNewRenderWidgetHostView(gfx::NativeViewId parent_window,
                                          int32 routing_id) {
#if defined(OS_WIN)
  // The class' lifetime is controlled by the host, which will send a message to
  // destroy the GpuRWHView when necessary. So we don't manage the lifetime
  // of this object.
  new GpuViewWin(this, parent_window, routing_id);
#else
  NOTIMPLEMENTED();
#endif
}
