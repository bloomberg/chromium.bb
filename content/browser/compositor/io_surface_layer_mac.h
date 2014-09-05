// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_COMPOSITOR_IO_SURFACE_LAYER_MAC_H_
#define CONTENT_BROWSER_COMPOSITOR_IO_SURFACE_LAYER_MAC_H_

#import <Cocoa/Cocoa.h>
#include <IOSurface/IOSurfaceAPI.h>

#include "base/mac/scoped_cftyperef.h"
#include "base/memory/ref_counted.h"
#include "base/timer/timer.h"
#include "ui/gfx/size.h"

@class IOSurfaceLayer;

namespace content {
class IOSurfaceLayerHelper;

// The interface through which the IOSurfaceLayer calls back into
// the structrue that created it (RenderWidgetHostViewMac or
// BrowserCompositorViewMac).
class IOSurfaceLayerClient {
 public:
  // Used to indicate that the layer should attempt to draw immediately and
  // should (even if the draw is elided by the system), ack the frame
  // immediately.
  virtual bool IOSurfaceLayerShouldAckImmediately() const = 0;

  // Called when a frame is drawn or when, because the layer is not visible, it
  // is known that the frame will never drawn.
  virtual void IOSurfaceLayerDidDrawFrame() = 0;

  // Called when an error prevents the frame from being drawn.
  virtual void IOSurfaceLayerHitError() = 0;
};

}  // namespace content

// The CoreAnimation layer for drawing accelerated content.
@interface IOSurfaceLayer : CAOpenGLLayer {
 @private
  content::IOSurfaceLayerClient* client_;
  scoped_ptr<content::IOSurfaceLayerHelper> helper_;

  // Used to track when canDrawInCGLContext should return YES. This can be
  // in response to receiving a new compositor frame, or from any of the events
  // that cause setNeedsDisplay to be called on the layer.
  bool needs_display_;

  // This is set when a frame is received, and un-set when the frame is drawn.
  bool has_pending_frame_;

  // Incremented every time that this layer is asked to draw but does not have
  // new content to draw.
  uint64 did_not_draw_counter_;

  // Set when inside a BeginPumpingFrames/EndPumpingFrames block.
  bool is_pumping_frames_;

  // The IOSurface being drawn by this layer.
  base::ScopedCFTypeRef<IOSurfaceRef> io_surface_;

  // The size of the frame in pixels. This will be less or equal than the pixel
  // size of |io_surface_|.
  gfx::Size frame_pixel_size_;

  // The GL texture that is bound to |io_surface_|. If |io_surface_| changes,
  // then this is marked as dirty by setting |io_surface_texture_dirty_|.
  GLuint io_surface_texture_;
  bool io_surface_texture_dirty_;

  // The CGL renderer ID, captured at draw time.
  GLint cgl_renderer_id_;
}

- (id)initWithClient:(content::IOSurfaceLayerClient*)client
     withScaleFactor:(float)scale_factor;

// Called when a new frame is received.
- (void)gotFrameWithIOSurface:(IOSurfaceID)io_surface_id
                withPixelSize:(gfx::Size)pixel_size
              withScaleFactor:(float)scale_factor;

- (float)scaleFactor;

// The CGL renderer ID.
- (int)rendererID;

// Mark that the client is no longer valid and cannot be called back into. This
// must be called before the layer is destroyed.
- (void)resetClient;

// Force a draw immediately (even if this means re-displaying a previously
// displayed frame).
- (void)setNeedsDisplayAndDisplayAndAck;

// Mark a bracket in which new frames are being pumped in a restricted nested
// run loop.
- (void)beginPumpingFrames;
- (void)endPumpingFrames;
@end

#endif  // CONTENT_BROWSER_COMPOSITOR_IO_SURFACE_LAYER_MAC_H_
