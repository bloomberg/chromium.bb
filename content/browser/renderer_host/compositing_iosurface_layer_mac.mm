// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/compositing_iosurface_layer_mac.h"

#include <CoreFoundation/CoreFoundation.h>
#include <OpenGL/gl.h>

#include "base/mac/sdk_forward_declarations.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_view_mac.h"
#include "content/browser/renderer_host/compositing_iosurface_context_mac.h"
#include "content/browser/renderer_host/compositing_iosurface_mac.h"
#include "ui/base/cocoa/animation_utils.h"
#include "ui/gfx/size_conversions.h"

@implementation CompositingIOSurfaceLayer

@synthesize context = context_;

- (id)initWithRenderWidgetHostViewMac:(content::RenderWidgetHostViewMac*)r {
  if (self = [super init]) {
    renderWidgetHostView_ = r;

    ScopedCAActionDisabler disabler;
    [self setBackgroundColor:CGColorGetConstantColor(kCGColorWhite)];
    [self setContentsGravity:kCAGravityTopLeft];
    [self setFrame:NSRectToCGRect(
        [renderWidgetHostView_->cocoa_view() bounds])];
    [self setNeedsDisplay];
    [self updateScaleFactor];
  }
  return self;
}

- (void)updateScaleFactor {
  if (!renderWidgetHostView_ ||
      ![self respondsToSelector:(@selector(contentsScale))] ||
      ![self respondsToSelector:(@selector(setContentsScale:))])
    return;

  float current_scale_factor = [self contentsScale];
  float new_scale_factor = current_scale_factor;
  if (renderWidgetHostView_->compositing_iosurface_) {
    new_scale_factor =
        renderWidgetHostView_->compositing_iosurface_->scale_factor();
  }

  if (new_scale_factor == current_scale_factor)
    return;

  ScopedCAActionDisabler disabler;
  [self setContentsScale:new_scale_factor];
}

- (void)disableCompositing{
  ScopedCAActionDisabler disabler;
  [self removeFromSuperlayer];
  renderWidgetHostView_ = nil;
}

// The remaining methods implement the CAOpenGLLayer interface.

- (CGLContextObj)copyCGLContextForPixelFormat:(CGLPixelFormatObj)pixelFormat {
  if (!renderWidgetHostView_)
    return nil;

  context_ = renderWidgetHostView_->compositing_iosurface_context_;
  if (!context_)
    return nil;

  return context_->cgl_context();
}

- (void)releaseCGLContext:(CGLContextObj)glContext {
  if (!context_.get())
    return;

  DCHECK(glContext == context_->cgl_context());
  context_ = nil;
}

- (void)drawInCGLContext:(CGLContextObj)glContext
             pixelFormat:(CGLPixelFormatObj)pixelFormat
            forLayerTime:(CFTimeInterval)timeInterval
             displayTime:(const CVTimeStamp*)timeStamp {
  TRACE_EVENT0("browser", "CompositingIOSurfaceLayer::drawInCGLContext");

  if (!context_.get() || !renderWidgetHostView_ ||
      !renderWidgetHostView_->compositing_iosurface_) {
    glClearColor(1, 1, 1, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    return;
  }

  DCHECK(glContext == context_->cgl_context());

  // Cache a copy of renderWidgetHostView_ because it may be reset if
  // a software frame is received in GetBackingStore.
  content::RenderWidgetHostViewMac* cached_view = renderWidgetHostView_;

  // If a resize is in progress then GetBackingStore request a frame of the
  // current window size and block until a frame of the right size comes in.
  // This makes the window content not lag behind the resize (at the cost of
  // blocking on the browser's main thread).
  if (cached_view->render_widget_host_) {
    cached_view->about_to_validate_and_paint_ = true;
    (void)cached_view->render_widget_host_->GetBackingStore(true);
    cached_view->about_to_validate_and_paint_ = false;
  }

  // If a transition to software mode has occurred, this layer should be
  // removed from the heirarchy now, so don't draw anything.
  if (!renderWidgetHostView_)
    return;

  gfx::Size window_size([self frame].size);
  float window_scale_factor = 1.f;
  if ([self respondsToSelector:(@selector(contentsScale))])
    window_scale_factor = [self contentsScale];

  CGLSetCurrentContext(glContext);
  if (!renderWidgetHostView_->compositing_iosurface_->DrawIOSurface(
        window_size,
        window_scale_factor,
        renderWidgetHostView_->frame_subscriber(),
        true)) {
    renderWidgetHostView_->GotAcceleratedCompositingError();
  }
}

@end
