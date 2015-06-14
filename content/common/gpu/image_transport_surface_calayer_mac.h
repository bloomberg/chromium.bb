// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_GPU_IMAGE_TRANSPORT_SURFACE_CALAYER_MAC_H_
#define CONTENT_COMMON_GPU_IMAGE_TRANSPORT_SURFACE_CALAYER_MAC_H_

#include "base/mac/scoped_nsobject.h"
#include "content/common/gpu/image_transport_surface_fbo_mac.h"
#include "ui/base/cocoa/remote_layer_api.h"
#include "ui/gl/gpu_switching_observer.h"
#include "ui/gl/scoped_cgl.h"

// Interface to the CALayer that will draw the content, and will be sent to the
// browser process via a CAContext.
@protocol ImageTransportLayer
// Draw a new frame whenever is appropriate (for pull-based systems, this may
// result in the frame being drawn at some point in the future).
- (void)drawNewFrame;
// Draw the frame immediately (force pull models to do a pull immedately).
- (void)drawPendingFrameImmediately;
// This is called when the layer is no longer being used by the
// CALayerStorageProvider.
- (void)resetStorageProvider;
@end

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
  bool AllocateColorBufferStorage(
      CGLContextObj context, const base::Closure& context_dirtied_callback,
      GLuint texture, gfx::Size pixel_size, float scale_factor) override;
  void FreeColorBufferStorage() override;
  void FrameSizeChanged(
      const gfx::Size& pixel_size, float scale_factor) override;
  void SwapBuffers() override;
  void WillWriteToBackbuffer() override;
  void DiscardBackbuffer() override;
  void SwapBuffersAckedByBrowser(bool disable_throttling) override;

  // Interface to ImageTransportLayer:
  CGLContextObj LayerShareGroupContext();
  base::Closure LayerShareGroupContextDirtiedCallback();
  bool LayerHasPendingDraw() const;
  void LayerDoDraw();

  // ui::GpuSwitchingObserver implementation.
  void OnGpuSwitched() override;

 private:
  void DrawImmediatelyAndUnblockBrowser();

  // The browser will be blocked while there is a frame that was sent to it but
  // hasn't drawn yet. This call will un-block the browser.
  void UnblockBrowserIfNeeded();

  // Inform the layer that it is no longer being used, and reset the layer.
  void ResetLayer();

  ImageTransportSurfaceFBO* transport_surface_;

  // Used to determine if we should use setNeedsDisplay or setAsynchronous to
  // animate. If vsync is disabled, an immediate setNeedsDisplay and
  // displayIfNeeded are called.
  const bool gpu_vsync_disabled_;

  // Used also to determine if we should wait for CoreAnimation to call our
  // drawInCGLContext, or if we should force it with displayIfNeeded.
  bool throttling_disabled_;

  // Set when a new swap occurs, and un-set when |layer_| draws that frame.
  bool has_pending_draw_;

  // The texture with the pixels to draw, and the share group it is allocated
  // in.
  base::ScopedTypeRef<CGLContextObj> share_group_context_;
  base::Closure share_group_context_dirtied_callback_;
  GLuint fbo_texture_;
  gfx::Size fbo_pixel_size_;
  float fbo_scale_factor_;

  // State for the Core Profile code path.
  GLuint program_;
  GLuint vertex_shader_;
  GLuint fragment_shader_;
  GLuint position_location_;
  GLuint tex_location_;
  GLuint vertex_buffer_;
  GLuint vertex_array_;

  // The CALayer that the current frame is being drawn into.
  base::scoped_nsobject<CAContext> context_;
  base::scoped_nsobject<CALayer<ImageTransportLayer>> layer_;

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
