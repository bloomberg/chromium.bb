// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_GPU_IMAGE_TRANSPORT_SURFACE_H_
#define CONTENT_COMMON_GPU_IMAGE_TRANSPORT_SURFACE_H_

#include <stdint.h>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "build/build_config.h"
#include "content/common/content_export.h"
#include "ui/gl/gl_surface.h"

namespace content {
class GpuChannelManager;
class GpuCommandBufferStub;

// The GPU process is agnostic as to how it displays results. On some platforms
// it renders directly to window. On others it renders offscreen and transports
// the results to the browser process to display. This file provides a simple
// framework for making the offscreen path seem more like the onscreen path.
//
// The ImageTransportSurface class defines an simple interface for events that
// should be responded to. The factory returns an offscreen surface that looks
// a lot like an onscreen surface to the GPU process.
//
// The ImageTransportSurfaceHelper provides some glue to the outside world:
// making sure outside events reach the ImageTransportSurface and
// allowing the ImageTransportSurface to send events to the outside world.

class ImageTransportSurface {
 public:
  // Creates a surface with the given attributes.
  static scoped_refptr<gfx::GLSurface> CreateSurface(
      GpuChannelManager* manager,
      GpuCommandBufferStub* stub,
      const gfx::GLSurfaceHandle& handle,
      gfx::GLSurface::Format format);

#if defined(OS_MACOSX)
  CONTENT_EXPORT static void SetAllowOSMesaForTesting(bool allow);
#endif

 private:
  // Creates the appropriate native surface depending on the GL implementation.
  // This will be implemented separately by each platform.
  //
  // This will not be called for texture transport surfaces which are
  // cross-platform. The platform implementation should only create the
  // surface and should not initialize it. On failure, a null scoped_refptr
  // should be returned.
  static scoped_refptr<gfx::GLSurface> CreateNativeSurface(
      GpuChannelManager* manager,
      GpuCommandBufferStub* stub,
      const gfx::GLSurfaceHandle& handle,
      gfx::GLSurface::Format format);

  DISALLOW_COPY_AND_ASSIGN(ImageTransportSurface);
};

}  // namespace content

#endif  // CONTENT_COMMON_GPU_IMAGE_TRANSPORT_SURFACE_H_
