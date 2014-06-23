// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_GPU_IMAGE_TRANSPORT_SURFACE_CALAYER_MAC_H_
#define CONTENT_COMMON_GPU_IMAGE_TRANSPORT_SURFACE_CALAYER_MAC_H_

#include "base/mac/scoped_nsobject.h"
#include "content/common/gpu/image_transport_surface_fbo_mac.h"
#include "ui/base/cocoa/remote_layer_api.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/scoped_cgl.h"

@interface ImageTransportLayer : CAOpenGLLayer {
  base::ScopedTypeRef<CGLContextObj> shareContext_;
  GLuint texture_;
  gfx::Size pixelSize_;
}
@end

namespace content {

// Allocate CAOpenGLLayer-backed storage for an FBO image transport surface.
class CALayerStorageProvider
    : public ImageTransportSurfaceFBO::StorageProvider {
 public:
  CALayerStorageProvider();
  virtual ~CALayerStorageProvider();

  // ImageTransportSurfaceFBO::StorageProvider implementation:
  virtual gfx::Size GetRoundedSize(gfx::Size size) OVERRIDE;
  virtual bool AllocateColorBufferStorage(
      CGLContextObj context, GLuint texture,
      gfx::Size pixel_size, float scale_factor) OVERRIDE;
  virtual void FreeColorBufferStorage() OVERRIDE;
  virtual uint64 GetSurfaceHandle() const OVERRIDE;
  virtual void WillSwapBuffers() OVERRIDE;

 private:
  base::scoped_nsobject<CAContext> context_;
  base::scoped_nsobject<ImageTransportLayer> layer_;

  DISALLOW_COPY_AND_ASSIGN(CALayerStorageProvider);
};

}  // namespace content

#endif  //  CONTENT_COMMON_GPU_IMAGE_TRANSPORT_CALAYER_MAC_H_
