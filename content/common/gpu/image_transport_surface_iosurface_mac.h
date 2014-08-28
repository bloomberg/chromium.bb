// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_GPU_IMAGE_TRANSPORT_SURFACE_IOSURFACE_MAC_H_
#define CONTENT_COMMON_GPU_IMAGE_TRANSPORT_SURFACE_IOSURFACE_MAC_H_

#include <list>

#include "content/common/gpu/image_transport_surface_fbo_mac.h"
#include "ui/gl/gl_bindings.h"

// Note that this must be included after gl_bindings.h to avoid conflicts.
#include <OpenGL/CGLIOSurface.h>

namespace content {

// Allocate IOSurface-backed storage for an FBO image transport surface.
class IOSurfaceStorageProvider
    : public ImageTransportSurfaceFBO::StorageProvider {
 public:
  IOSurfaceStorageProvider(ImageTransportSurfaceFBO* transport_surface);
  virtual ~IOSurfaceStorageProvider();

  // ImageTransportSurfaceFBO::StorageProvider implementation:
  virtual gfx::Size GetRoundedSize(gfx::Size size) OVERRIDE;
  virtual bool AllocateColorBufferStorage(
      CGLContextObj context, GLuint texture,
      gfx::Size pixel_size, float scale_factor) OVERRIDE;
  virtual void FreeColorBufferStorage() OVERRIDE;
  virtual void SwapBuffers(const gfx::Size& size, float scale_factor) OVERRIDE;
  virtual void WillWriteToBackbuffer() OVERRIDE;
  virtual void DiscardBackbuffer() OVERRIDE;
  virtual void SwapBuffersAckedByBrowser() OVERRIDE;

 private:
  ImageTransportSurfaceFBO* transport_surface_;

  base::ScopedCFTypeRef<IOSurfaceRef> io_surface_;

  // The list of IOSurfaces that have been sent to the browser process but have
  // not been opened in the browser process yet. This list should never have
  // more than one entry.
  std::list<base::ScopedCFTypeRef<IOSurfaceRef>> pending_swapped_surfaces_;

  // The id of |io_surface_| or 0 if that's NULL.
  IOSurfaceID io_surface_id_;

  DISALLOW_COPY_AND_ASSIGN(IOSurfaceStorageProvider);
};

}  // namespace content

#endif  //  CONTENT_COMMON_GPU_IMAGE_TRANSPORT_SURFACE_MAC_H_
