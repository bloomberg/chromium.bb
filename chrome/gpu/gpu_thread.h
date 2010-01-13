// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_GPU_GPU_THREAD_H_
#define CHROME_GPU_GPU_THREAD_H_

#include "app/gfx/native_widget_types.h"
#include "base/basictypes.h"
#include "chrome/common/child_thread.h"

class GpuThread : public ChildThread {
 public:
  GpuThread();
  ~GpuThread();

 private:
  // ChildThread overrides.
  virtual void OnControlMessageReceived(const IPC::Message& msg);

  // Message handlers.
  void OnNewRenderWidgetHostView(gfx::NativeViewId parent_window,
                                 int32 routing_id);

  DISALLOW_COPY_AND_ASSIGN(GpuThread);
};

#endif  // CHROME_GPU_GPU_THREAD_H_
