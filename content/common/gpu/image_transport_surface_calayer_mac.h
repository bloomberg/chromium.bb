// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_GPU_IMAGE_TRANSPORT_SURFACE_CALAYER_MAC_H_
#define CONTENT_COMMON_GPU_IMAGE_TRANSPORT_SURFACE_CALAYER_MAC_H_

#include "base/mac/scoped_nsobject.h"
#include "content/common/gpu/image_transport_surface_fbo_mac.h"
#include "ui/base/cocoa/remote_layer_api.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gpu_switching_observer.h"
#include "ui/gl/scoped_cgl.h"

@class ImageTransportLayer;

namespace content {

// Allocate CAOpenGLLayer-backed storage for an FBO image transport surface.
class CALayerStorageProvider
    : public ImageTransportSurfaceFBO::StorageProvider,
      public ui::GpuSwitchingObserver {
 public:
  CALayerStorageProvider(ImageTransportSurfaceFBO* transport_surface);
  ~CALayerStorageProvider() override;

  // ImageTransportSurfaceFBO::StorageProvider implementation:
  gfx::Size GetRoundedSize(gfx::Size size) override;
  bool AllocateColorBufferStorage(CGLContextObj context,
                                  GLuint texture,
                                  gfx::Size pixel_size,
                                  float scale_factor) override;
  void FreeColorBufferStorage() override;
  void SwapBuffers(const gfx::Size& size, float scale_factor) override;
  void WillWriteToBackbuffer() override;
  void DiscardBackbuffer() override;
  void SwapBuffersAckedByBrowser(bool disable_throttling) override;

  // Interface to ImageTransportLayer:
  CGLContextObj LayerShareGroupContext();
  bool LayerCanDraw();
  void LayerDoDraw();
  void LayerResetStorageProvider();

  // ui::GpuSwitchingObserver implementation.
  void OnGpuSwitched() override;

 private:
  void DrawImmediatelyAndUnblockBrowser();

  // The browser will be blocked while there is a frame that was sent to it but
  // hasn't drawn yet. This call will un-block the browser.
  void UnblockBrowserIfNeeded();

  ImageTransportSurfaceFBO* transport_surface_;

  // Used to determine if we should use setNeedsDisplay or setAsynchronous to
  // animate.
  const bool gpu_vsync_disabled_;

  // Used also to determine if we should wait for CoreAnimation to call our
  // drawInCGLContext, or if we should force it with displayIfNeeded.
  bool throttling_disabled_;

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

  // When a CAContext is destroyed in the GPU process, it will become a blank
  // CALayer in the browser process. Put retains on these contexts in this queue
  // when they are discarded, and remove one item from the queue as each frame
  // is acked.
  std::list<base::scoped_nsobject<CAContext> > previously_discarded_contexts_;

  // Indicates that the CALayer should be recreated at the next swap. This is
  // to ensure that the CGLContext created for the CALayer be on the right GPU.
  bool recreate_layer_after_gpu_switch_;

  // Weak factory against which a timeout task for forcing a draw is created.
  base::WeakPtrFactory<CALayerStorageProvider> pending_draw_weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(CALayerStorageProvider);
};

}  // namespace content

#endif  // CONTENT_COMMON_GPU_IMAGE_TRANSPORT_SURFACE_CALAYER_MAC_H_
