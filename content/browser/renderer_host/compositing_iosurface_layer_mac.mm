// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/compositing_iosurface_layer_mac.h"

#include <CoreFoundation/CoreFoundation.h>
#include <OpenGL/gl.h>

#include "base/mac/sdk_forward_declarations.h"
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
    [self setAutoresizingMask:kCALayerWidthSizable | kCALayerHeightSizable];
    [self setContentsGravity:kCAGravityTopLeft];
    [self setFrame:NSRectToCGRect(
        [renderWidgetHostView_->cocoa_view() bounds])];
    [self setNeedsDisplay];
    [self updateScaleFactor];
  }
  return self;
}

- (BOOL)ensureContext {
  if (context_)
    return YES;

  if (!renderWidgetHostView_)
    return NO;

  if (renderWidgetHostView_->compositing_iosurface_) {
    context_ = renderWidgetHostView_->compositing_iosurface_->context();
    [context_->nsgl_context() clearDrawable];
  }

  if (!context_) {
    context_ = content::CompositingIOSurfaceContext::Get(
        renderWidgetHostView_->window_number());
  }

  return context_ ? YES : NO;
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
  if ([self ensureContext])
    return context_->cgl_context();
  return nil;
}

- (void)releaseCGLContext:(CGLContextObj)glContext {
  if (!context_)
    return;

  DCHECK(glContext == context_->cgl_context());
  context_ = nil;
}

- (void)drawInCGLContext:(CGLContextObj)glContext
             pixelFormat:(CGLPixelFormatObj)pixelFormat
            forLayerTime:(CFTimeInterval)timeInterval
             displayTime:(const CVTimeStamp*)timeStamp {
  if (!context_ || !renderWidgetHostView_ ||
      !renderWidgetHostView_->compositing_iosurface_) {
    glClearColor(1, 1, 1, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    return;
  }

  DCHECK(glContext == context_->cgl_context());

  gfx::Size window_size([self frame].size);
  float window_scale_factor = 1.f;
  if ([self respondsToSelector:(@selector(contentsScale))])
    window_scale_factor = [self contentsScale];

  if (!renderWidgetHostView_->compositing_iosurface_->DrawIOSurface(
        window_size,
        window_scale_factor,
        renderWidgetHostView_->frame_subscriber(),
        true)) {
    renderWidgetHostView_->GotAcceleratedCompositingError();
  }
}

@end
