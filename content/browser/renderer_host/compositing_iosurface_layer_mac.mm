// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/compositing_iosurface_layer_mac.h"

#include <CoreFoundation/CoreFoundation.h>
#include <OpenGL/gl.h>

#include "base/mac/mac_util.h"
#include "base/mac/sdk_forward_declarations.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_view_mac.h"
#include "content/browser/renderer_host/compositing_iosurface_context_mac.h"
#include "content/browser/renderer_host/compositing_iosurface_mac.h"
#include "ui/base/cocoa/animation_utils.h"
#include "ui/gfx/size_conversions.h"
#include "ui/gl/gpu_switching_manager.h"

@implementation CompositingIOSurfaceLayer

- (content::CompositingIOSurfaceMac*)iosurface {
  return iosurface_.get();
}

- (content::CompositingIOSurfaceContext*)context {
  return context_.get();
}

- (id)initWithIOSurface:(scoped_refptr<content::CompositingIOSurfaceMac>)
                            iosurface
             withClient:(content::CompositingIOSurfaceLayerClient*)client {
  if (self = [super init]) {
    iosurface_ = iosurface;
    client_ = client;

    context_ = content::CompositingIOSurfaceContext::Get(
        content::CompositingIOSurfaceContext::kCALayerContextWindowNumber);
    DCHECK(context_);
    needs_display_ = NO;
    did_not_draw_counter_ = 0;

    [self setBackgroundColor:CGColorGetConstantColor(kCGColorWhite)];
    [self setAnchorPoint:CGPointMake(0, 0)];
    // Setting contents gravity is necessary to prevent the layer from being
    // scaled during dyanmic resizes (especially with devtools open).
    [self setContentsGravity:kCAGravityTopLeft];
    if ([self respondsToSelector:(@selector(setContentsScale:))]) {
      [self setContentsScale:iosurface_->scale_factor()];
    }
  }
  return self;
}

- (void)resetClient {
  client_ = NULL;
}

- (void)gotNewFrame {
  if (context_ && context_->is_vsync_disabled()) {
    // If vsync is disabled, draw immediately and don't bother trying to use
    // the isAsynchronous property to ensure smooth animation.
    [self setNeedsDisplay];
    [self displayIfNeeded];

    // Calls to setNeedsDisplay can sometimes be ignored, especially if issued
    // rapidly (e.g, with vsync off). This is unacceptable because the failure
    // to ack a single frame will hang the renderer. Ensure that the renderer
    // not be blocked by lying and claiming that we drew the frame.
    if (needs_display_ && client_)
      client_->AcceleratedLayerDidDrawFrame(true);
  } else {
    needs_display_ = YES;
    if (![self isAsynchronous])
      [self setAsynchronous:YES];
  }
}

// The remaining methods implement the CAOpenGLLayer interface.

- (CGLPixelFormatObj)copyCGLPixelFormatForDisplayMask:(uint32_t)mask {
  if (!context_)
    return [super copyCGLPixelFormatForDisplayMask:mask];
  return CGLRetainPixelFormat(CGLGetPixelFormat(context_->cgl_context()));
}

- (CGLContextObj)copyCGLContextForPixelFormat:(CGLPixelFormatObj)pixelFormat {
  if (!context_)
    return [super copyCGLContextForPixelFormat:pixelFormat];
  return CGLRetainContext(context_->cgl_context());
}

- (void)setNeedsDisplay {
  needs_display_ = YES;
  [super setNeedsDisplay];
}

- (BOOL)canDrawInCGLContext:(CGLContextObj)glContext
                pixelFormat:(CGLPixelFormatObj)pixelFormat
               forLayerTime:(CFTimeInterval)timeInterval
                displayTime:(const CVTimeStamp*)timeStamp {
  // Add an instantaneous blip to the PendingSwapAck state to indicate
  // that CoreAnimation asked if a frame is ready. A blip up to to 3 (usually
  // from 2, indicating that a swap ack is pending) indicates that we requested
  // a draw. A blip up to 1 (usually from 0, indicating there is no pending swap
  // ack) indicates that we did not request a draw. This would be more natural
  // to do with a tracing pseudo-thread
  // http://crbug.com/366300
  if (client_) {
    TRACE_COUNTER_ID1("browser", "PendingSwapAck", self,
        needs_display_ ? 3 : 1);
    TRACE_COUNTER_ID1("browser", "PendingSwapAck", self,
        client_->AcceleratedLayerHasNotAckedPendingFrame() ? 2 : 0);
  }

  // If we return NO 30 times in a row, switch to being synchronous to avoid
  // burning CPU cycles on this callback.
  if (needs_display_) {
    did_not_draw_counter_ = 0;
  } else {
    did_not_draw_counter_ += 1;
    if (did_not_draw_counter_ > 30)
      [self setAsynchronous:NO];
  }

  return needs_display_;
}

- (void)drawInCGLContext:(CGLContextObj)glContext
             pixelFormat:(CGLPixelFormatObj)pixelFormat
            forLayerTime:(CFTimeInterval)timeInterval
             displayTime:(const CVTimeStamp*)timeStamp {
  TRACE_EVENT0("browser", "CompositingIOSurfaceLayer::drawInCGLContext");

  if (!iosurface_->HasIOSurface() || context_->cgl_context() != glContext) {
    glClearColor(1, 1, 1, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    return;
  }

  // The correct viewport to cover the layer will be set up by the caller.
  // Transform this into a window size for DrawIOSurface, where it will be
  // transformed back into this viewport.
  GLint viewport[4];
  glGetIntegerv(GL_VIEWPORT, viewport);
  gfx::Rect window_rect(viewport[0], viewport[1], viewport[2], viewport[3]);
  float window_scale_factor = 1.f;
  if ([self respondsToSelector:(@selector(contentsScale))])
    window_scale_factor = [self contentsScale];
  window_rect = ToNearestRect(
      gfx::ScaleRect(window_rect, 1.f/window_scale_factor));

  bool draw_succeeded = iosurface_->DrawIOSurface(
      context_, window_rect, window_scale_factor, false);

  needs_display_ = NO;
  if (client_)
    client_->AcceleratedLayerDidDrawFrame(draw_succeeded);

  [super drawInCGLContext:glContext
              pixelFormat:pixelFormat
             forLayerTime:timeInterval
              displayTime:timeStamp];
}

@end
