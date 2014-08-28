// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_COMPOSITING_IOSURFACE_LAYER_MAC_H_
#define CONTENT_BROWSER_RENDERER_HOST_COMPOSITING_IOSURFACE_LAYER_MAC_H_

#import <Cocoa/Cocoa.h>

#include "base/mac/scoped_cftyperef.h"
#include "base/memory/ref_counted.h"
#include "base/timer/timer.h"
#include "ui/gfx/size.h"

@class IOSurfaceLayer;

namespace content {
class CompositingIOSurfaceMac;
class CompositingIOSurfaceContext;

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

// IOSurfaceLayerHelper provides C++ functionality needed for the
// IOSurfaceLayer class, and does most of the heavy lifting for the
// class.
// TODO(ccameron): This class should own IOSurfaceLayer, rather than
// vice versa.
class IOSurfaceLayerHelper {
 public:
  IOSurfaceLayerHelper(IOSurfaceLayerClient* client,
                                  IOSurfaceLayer* layer);
  ~IOSurfaceLayerHelper();

  // Called when the IOSurfaceLayer gets a new frame.
  void GotNewFrame();

  // Called whenever -[IOSurfaceLayer setNeedsDisplay] is called.
  void SetNeedsDisplay();

  // Called whenever -[IOSurfaceLayer canDrawInCGLContext] is called,
  // to determine if a new frame should be drawn.
  bool CanDraw();

  // Called whenever -[IOSurfaceLayer drawInCGLContext] draws a
  // frame.
  void DidDraw(bool success);

  // Immediately re-draw the layer, even if the content has not changed, and
  // ensure that the frame be acked.
  void SetNeedsDisplayAndDisplayAndAck();

  // Immediately draw the layer, only if one is pending, and ensure that the
  // frame be acked.
  void DisplayIfNeededAndAck();

  // Mark a bracket in which new frames are being pumped in a restricted nested
  // run loop. During this time frames are acked immediately and draws are
  // deferred until the bracket ends.
  void BeginPumpingFrames();
  void EndPumpingFrames();

 private:
  // Called whenever the frame provided in GotNewFrame should be acknowledged
  // (this may be because it was drawn, or it may be to unblock the
  // compositor).
  void AckPendingFrame(bool success);

  void TimerFired();

  // The client that the owning layer was created with.
  content::IOSurfaceLayerClient* const client_;

  // The layer that owns this helper.
  IOSurfaceLayer* const layer_;

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

  // The browser places back-pressure on the GPU by not acknowledging swap
  // calls until they appear on the screen. This can lead to hangs if the
  // view is moved offscreen (among other things). Prevent hangs by always
  // acknowledging the frame after timeout of 1/6th of a second  has passed.
  base::DelayTimer<IOSurfaceLayerHelper> timer_;
};

}  // namespace content

// The CoreAnimation layer for drawing accelerated content.
@interface IOSurfaceLayer : CAOpenGLLayer {
 @private
  scoped_refptr<content::CompositingIOSurfaceMac> iosurface_;
  scoped_refptr<content::CompositingIOSurfaceContext> context_;

  scoped_ptr<content::IOSurfaceLayerHelper> helper_;
}

- (id)initWithClient:(content::IOSurfaceLayerClient*)client
     withScaleFactor:(float)scale_factor;

- (bool)gotFrameWithIOSurface:(IOSurfaceID)io_surface_id
                withPixelSize:(gfx::Size)pixel_size
              withScaleFactor:(float)scale_factor;

// Context poison accessors.
- (void)poisonContextAndSharegroup;
- (bool)hasBeenPoisoned;

- (float)scaleFactor;

// The CGL renderer ID.
- (int)rendererID;

// Mark that the client is no longer valid and cannot be called back into. This
// must be called before the layer is destroyed.
- (void)resetClient;

// Called when a new frame is received.
- (void)gotNewFrame;

// Force a draw immediately (even if this means re-displaying a previously
// displayed frame).
- (void)setNeedsDisplayAndDisplayAndAck;

// Force a draw immediately, but only if one was requested.
- (void)displayIfNeededAndAck;

// Mark a bracket in which new frames are being pumped in a restricted nested
// run loop.
- (void)beginPumpingFrames;
- (void)endPumpingFrames;
@end

#endif  // CONTENT_BROWSER_RENDERER_HOST_COMPOSITING_IOSURFACE_LAYER_MAC_H_
