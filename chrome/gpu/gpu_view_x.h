// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_GPU_GPU_VIEW_X_H_
#define CHROME_GPU_GPU_VIEW_X_H_

#include "app/gfx/native_widget_types.h"
#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "chrome/gpu/x_util.h"
#include "ipc/ipc_channel.h"

class GpuBackingStoreGLX;
class GpuThread;

namespace gfx {
class Rect;
class Size;
}

class GpuViewX
    : public IPC::Channel::Listener {
 public:
  GpuViewX(GpuThread* gpu_thread,
           XID parent,
           int32 routing_id);
  ~GpuViewX();

  GpuThread* gpu_thread() const { return gpu_thread_; }
  XID window() const { return window_; }

  // Wrapper around GPUBackingStoreGLXContext using our current window.
  GLXContext BindContext();

  // IPC::Channel::Listener implementation.
  virtual void OnMessageReceived(const IPC::Message& message);
  virtual void OnChannelConnected(int32 peer_pid);
  virtual void OnChannelError();

  void DidScrollBackingStoreRect(int dx, int dy, const gfx::Rect& rect);

  void Repaint();

 private:
  // IPC message handlers.
  void OnNewBackingStore(int32 routing_id, const gfx::Size& size);
  void OnWindowPainted();

  GpuThread* gpu_thread_;
  int32 routing_id_;

  XID window_;

  scoped_ptr<GpuBackingStoreGLX> backing_store_;

  DISALLOW_COPY_AND_ASSIGN(GpuViewX);
};

#endif  // CHROME_GPU_GPU_VIEW_X_H_
