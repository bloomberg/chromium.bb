// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_GPU_IMAGE_TRANSPORT_SURFACE_LINUX_H_
#define CONTENT_COMMON_GPU_IMAGE_TRANSPORT_SURFACE_LINUX_H_
#pragma once

#if defined(ENABLE_GPU)

#include "base/memory/ref_counted.h"
#include "ipc/ipc_channel.h"
#include "ipc/ipc_message.h"
#include "ui/gfx/size.h"

class GpuChannelManager;

struct GpuHostMsg_AcceleratedSurfaceRelease_Params;
struct GpuHostMsg_AcceleratedSurfaceSetIOSurface_Params;
struct GpuHostMsg_AcceleratedSurfaceBuffersSwapped_Params;

namespace gfx {
class GLSurface;
}

namespace gpu {
class GpuScheduler;
}

class ImageTransportSurface {
 public:
  virtual void OnSetSurfaceACK(uint64 surface_id) = 0;
  virtual void OnBuffersSwappedACK() = 0;
  virtual void OnResize(gfx::Size size) = 0;

  // Creates the appropriate surface depending on the GL implementation.
  static scoped_refptr<gfx::GLSurface>
      CreateSurface(GpuChannelManager* manager,
                    int32 render_view_id,
                    int32 renderer_id,
                    int32 command_buffer_id);
};

class ImageTransportHelper : public IPC::Channel::Listener {
 public:
  // Takes weak pointers to objects that outlive the helper.
  ImageTransportHelper(ImageTransportSurface* surface,
                       GpuChannelManager* manager,
                       int32 render_view_id,
                       int32 renderer_id,
                       int32 command_buffer_id);
  ~ImageTransportHelper();

  bool Initialize();
  void Destroy();

  // IPC::Channel::Listener implementation:
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

  // Helper send functions. Caller fills in the surface specific params
  // like size and surface id. The helper fills in the rest.
  void SendAcceleratedSurfaceRelease(
      GpuHostMsg_AcceleratedSurfaceRelease_Params params);
  void SendAcceleratedSurfaceSetIOSurface(
      GpuHostMsg_AcceleratedSurfaceSetIOSurface_Params params);
  void SendAcceleratedSurfaceBuffersSwapped(
      GpuHostMsg_AcceleratedSurfaceBuffersSwapped_Params params);

  // Whether or not we should execute more commands.
  void SetScheduled(bool is_scheduled);

 private:
  gpu::GpuScheduler* Scheduler();

  // IPC::Message handlers.
  void OnSetSurfaceACK(uint64 surface_id);
  void OnBuffersSwappedACK();

  // Backbuffer resize callback.
  void Resize(gfx::Size size);

  // Weak pointers that to objects that outlive this helper.
  ImageTransportSurface* surface_;
  GpuChannelManager* manager_;

  int32 render_view_id_;
  int32 renderer_id_;
  int32 command_buffer_id_;
  int32 route_id_;

  DISALLOW_COPY_AND_ASSIGN(ImageTransportHelper);
};

#endif  // defined(ENABLE_GPU)

#endif  // CONTENT_COMMON_GPU_IMAGE_TRANSPORT_SURFACE_LINUX_H_
