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

- (id)initWithRenderWidgetHostViewMac:(content::RenderWidgetHostViewMac*)r {
  if (self = [super init]) {
    renderWidgetHostView_ = r;
    context_ = content::CompositingIOSurfaceContext::Get(
        content::CompositingIOSurfaceContext::kCALayerContextWindowNumber);
    DCHECK(context_);
    needsDisplay_ = NO;

    [self setBackgroundColor:CGColorGetConstantColor(kCGColorWhite)];
    [self setAnchorPoint:CGPointMake(0, 0)];
    // Setting contents gravity is necessary to prevent the layer from being
    // scaled during dyanmic resizes (especially with devtools open).
    [self setContentsGravity:kCAGravityTopLeft];
    if (renderWidgetHostView_->compositing_iosurface_ &&
        [self respondsToSelector:(@selector(setContentsScale:))]) {
      [self setContentsScale:
          renderWidgetHostView_->compositing_iosurface_->scale_factor()];
    }
  }
  return self;
}

- (void)disableCompositing{
  renderWidgetHostView_ = nil;
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
    // not be blocked.
    if (needsDisplay_)
      renderWidgetHostView_->SendPendingSwapAck();
  } else {
    needsDisplay_ = YES;
    if (![self isAsynchronous])
      [self setAsynchronous:YES];
  }
}

- (void)timerSinceGotNewFrameFired {
  if (![self isAsynchronous])
    return;

  [self setAsynchronous:NO];

  // If there was a pending frame, ensure that it goes through.
  if (needsDisplay_) {
    [self setNeedsDisplay];
    [self displayIfNeeded];
  }
  // If that fails then ensure that, at a minimum, the renderer is not blocked.
  if (needsDisplay_)
    renderWidgetHostView_->SendPendingSwapAck();
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
  needsDisplay_ = YES;
  [super setNeedsDisplay];
}

- (BOOL)canDrawInCGLContext:(CGLContextObj)glContext
                pixelFormat:(CGLPixelFormatObj)pixelFormat
               forLayerTime:(CFTimeInterval)timeInterval
                displayTime:(const CVTimeStamp*)timeStamp {
  return needsDisplay_;
}

- (void)drawInCGLContext:(CGLContextObj)glContext
             pixelFormat:(CGLPixelFormatObj)pixelFormat
            forLayerTime:(CFTimeInterval)timeInterval
             displayTime:(const CVTimeStamp*)timeStamp {
  TRACE_EVENT0("browser", "CompositingIOSurfaceLayer::drawInCGLContext");

  if (!context_ ||
      (context_ && context_->cgl_context() != glContext) ||
      !renderWidgetHostView_ ||
      !renderWidgetHostView_->compositing_iosurface_) {
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

  if (!renderWidgetHostView_->compositing_iosurface_->DrawIOSurface(
        context_,
        window_rect,
        window_scale_factor,
        false)) {
    renderWidgetHostView_->GotAcceleratedCompositingError();
    return;
  }

  needsDisplay_ = NO;
  renderWidgetHostView_->SendPendingLatencyInfoToHost();
  renderWidgetHostView_->SendPendingSwapAck();

  [super drawInCGLContext:glContext
              pixelFormat:pixelFormat
             forLayerTime:timeInterval
              displayTime:timeStamp];
}

@end
