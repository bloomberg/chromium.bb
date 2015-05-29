// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_GPU_NULL_TRANSPORT_SURFACE_H_
#define CONTENT_COMMON_GPU_NULL_TRANSPORT_SURFACE_H_

#include "base/basictypes.h"
#include "content/common/gpu/image_transport_surface.h"
#include "ui/gfx/native_widget_types.h"

namespace content {

// Used when the GPU process is not involved in frame transport, but frame
// resources are exchanged between client and target directly (such as in
// delegated rendering).
class NullTransportSurface : public PassThroughImageTransportSurface {
 public:
  NullTransportSurface(GpuChannelManager* manager,
                       GpuCommandBufferStub* stub,
                       const gfx::GLSurfaceHandle& handle);

  // gfx::GLSurfaceAdapter implementation.
  bool Initialize() override;
  void Destroy() override;
  bool SwapBuffers() override;
  bool PostSubBuffer(int x, int y, int width, int height) override;
  bool OnMakeCurrent(gfx::GLContext* context) override;

 protected:
  ~NullTransportSurface() override;

  void SendVSyncUpdateIfAvailable() override;

 private:
  uint32 parent_client_id_;

  DISALLOW_COPY_AND_ASSIGN(NullTransportSurface);
};

}  // namespace content

#endif  // CONTENT_COMMON_GPU_NULL_TRANSPORT_SURFACE_H_
