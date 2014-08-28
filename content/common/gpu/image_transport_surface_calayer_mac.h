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

@class ImageTransportLayer;

namespace content {

// Allocate CAOpenGLLayer-backed storage for an FBO image transport surface.
class CALayerStorageProvider
    : public ImageTransportSurfaceFBO::StorageProvider {
 public:
  CALayerStorageProvider(ImageTransportSurfaceFBO* transport_surface);
  virtual ~CALayerStorageProvider();

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

  // Interface to ImageTransportLayer:
  CGLContextObj LayerShareGroupContext();
  bool LayerCanDraw();
  void LayerDoDraw();
  void LayerResetStorageProvider();

 private:
  void DrawWithVsyncDisabled();
  void SendPendingSwapToBrowserAfterFrameDrawn();

  ImageTransportSurfaceFBO* transport_surface_;

  // Used to determine if we should use setNeedsDisplay or setAsynchronous to
  // animate.
  const bool gpu_vsync_disabled_;

  // Set when a new swap occurs, and un-set when |layer_| draws that frame.
  bool has_pending_draw_;

  // A counter that is incremented whenever LayerCanDraw returns false. If this
  // reaches a threshold, then |layer_| is switched to synchronous drawing to
  // save CPU work.
  uint32 can_draw_returned_false_count_;

  // The texture with the pixels to draw, and the share group it is allocated
  // in.
  base::ScopedTypeRef<CGLContextObj> share_group_context_;
  GLuint fbo_texture_;
  gfx::Size fbo_pixel_size_;
  float fbo_scale_factor_;

  // The CALayer that the current frame is being drawn into.
  base::scoped_nsobject<CAContext> context_;
  base::scoped_nsobject<ImageTransportLayer> layer_;

  base::WeakPtrFactory<CALayerStorageProvider> weak_factory_;
  DISALLOW_COPY_AND_ASSIGN(CALayerStorageProvider);
};

}  // namespace content

#endif  //  CONTENT_COMMON_GPU_IMAGE_TRANSPORT_CALAYER_MAC_H_
