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

class GpuCommandBufferStub;

namespace gfx {
class GLSurface;
}

namespace gpu {
class GpuScheduler;
}

class ImageTransportSurface : public IPC::Channel::Listener,
                              public IPC::Message::Sender {
 public:
  // Creates the appropriate surface depending on the GL implementation
  static scoped_refptr<gfx::GLSurface>
      CreateSurface(GpuCommandBufferStub* stub);

  bool Initialize();
  void Destroy();

  // IPC::Channel::Listener implementation:
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

  // IPC::Message::Sender implementation:
  virtual bool Send(IPC::Message* msg) OVERRIDE;

 protected:
  explicit ImageTransportSurface(GpuCommandBufferStub* stub);
  ~ImageTransportSurface();

  // IPC::Message handlers
  virtual void OnSetSurfaceACK(uint64 surface_id) = 0;
  virtual void OnBuffersSwappedACK() = 0;

  // Resize the backbuffer
  virtual void Resize(gfx::Size size) = 0;

  GpuCommandBufferStub* stub() { return stub_; }
  gpu::GpuScheduler* scheduler();
  int32 route_id() { return route_id_; }

 private:
  // Weak pointer. The stub outlives this surface.
  GpuCommandBufferStub* stub_;
  int32 route_id_;

  DISALLOW_COPY_AND_ASSIGN(ImageTransportSurface);
};

#endif  // defined(ENABLE_GPU)

#endif  // CONTENT_COMMON_GPU_EGL_IMAGE_TRANSPORT_SURFACE_LINUX_H_
