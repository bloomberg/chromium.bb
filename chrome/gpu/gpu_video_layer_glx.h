// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_GPU_GPU_VIDEO_LAYER_GLX_H_
#define CHROME_GPU_GPU_VIDEO_LAYER_GLX_H_

#include "base/basictypes.h"
#include "base/process.h"
#include "chrome/common/transport_dib.h"
#include "chrome/common/x11_util.h"
#include "ipc/ipc_channel.h"

class GpuViewX;
class GpuThread;

class GpuVideoLayerGLX : public IPC::Channel::Listener {
 public:
  GpuVideoLayerGLX(GpuViewX* view,
                   GpuThread* gpu_thread,
                   int32 routing_id,
                   const gfx::Size& size);
  virtual ~GpuVideoLayerGLX();

  const gfx::Size& size() const { return size_; }

  // IPC::Channel::Listener implementation.
  virtual void OnMessageReceived(const IPC::Message& message);
  virtual void OnChannelConnected(int32 peer_pid);
  virtual void OnChannelError();

 private:
  // Message handlers.
  void OnPaintToVideoLayer(base::ProcessId source_process_id,
                           TransportDIB::Id id,
                           const gfx::Rect& bitmap_rect);

  GpuViewX* view_;
  GpuThread* gpu_thread_;
  int32 routing_id_;
  gfx::Size size_;

  DISALLOW_COPY_AND_ASSIGN(GpuVideoLayerGLX);
};

#endif  // CHROME_GPU_GPU_VIDEO_LAYER_GLX_H_
