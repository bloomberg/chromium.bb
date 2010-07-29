// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_GPU_GPU_VIDEO_LAYER_GLX_H_
#define CHROME_GPU_GPU_VIDEO_LAYER_GLX_H_
#pragma once

#include "app/surface/transport_dib.h"
#include "base/basictypes.h"
#include "base/process.h"
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

  // Renders the video layer using the current GL context with respect to the
  // given |viewport_size|.
  //
  // TODO(scherkus): we also need scrolling information to determine where
  // exactly to place our quad.
  void Render(const gfx::Size& viewport_size);

  // IPC::Channel::Listener implementation.
  virtual void OnMessageReceived(const IPC::Message& message);
  virtual void OnChannelConnected(int32 peer_pid);
  virtual void OnChannelError();

 private:
  // Message handlers.
  void OnPaintToVideoLayer(base::ProcessId source_process_id,
                           TransportDIB::Id id,
                           const gfx::Rect& bitmap_rect);

  // Calculates vertices for |object| relative to |world|, where |world| is
  // assumed to represent a full-screen quad.  |vertices| should be an array of
  // 8 floats.
  //
  // TODO(scherkus): not sure how to describe what this does.
  static void CalculateVertices(const gfx::Size& world,
                                const gfx::Rect& object,
                                float* vertices);

  // GPU process related.
  GpuViewX* view_;
  GpuThread* gpu_thread_;
  int32 routing_id_;

  // The native size of the incoming YUV frames.
  gfx::Size native_size_;

  // The target absolute position and size of the RGB frames.
  gfx::Rect target_rect_;

  // The target absolute position and size expressed as quad vertices.
  float target_vertices_[8];

  // 3 textures, one for each plane.
  unsigned int textures_[3];

  // Shader program for YUV->RGB conversion.
  unsigned int program_;

  DISALLOW_COPY_AND_ASSIGN(GpuVideoLayerGLX);
};

#endif  // CHROME_GPU_GPU_VIDEO_LAYER_GLX_H_
