// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/compositor/browser_compositor_view_mac.h"

#include "base/debug/trace_event.h"
#include "base/mac/scoped_cftyperef.h"
#include "content/browser/renderer_host/compositing_iosurface_context_mac.h"
#include "content/browser/renderer_host/compositing_iosurface_mac.h"
#include "content/browser/renderer_host/software_layer_mac.h"
#include "ui/base/cocoa/animation_utils.h"
#include "ui/gl/scoped_cgl.h"

// The default implementation of additions to the NSView interface for browser
// compositing should never be called. Log an error if they are.
@implementation NSView (BrowserCompositorView)

- (void)gotAcceleratedIOSurfaceFrame:(uint64)surface_handle
                       withPixelSize:(gfx::Size)pixel_size
                     withScaleFactor:(float)scale_factor {
  DLOG(ERROR) << "-[NSView gotAcceleratedIOSurfaceFrame] called on "
              << "non-overriden class.";
}

- (void)gotSoftwareFrame:(cc::SoftwareFrameData*)frame_data
         withScaleFactor:(float)scale_factor
              withCanvas:(SkCanvas*)canvas {
  DLOG(ERROR) << "-[NSView gotSoftwareFrame] called on non-overridden class.";
}

@end  // NSView (BrowserCompositorView)

@implementation BrowserCompositorViewMac : NSView

- (id)initWithSuperview:(NSView*)view {
  if (self = [super init]) {
    // Disable the fade-in animation as the layer and view are added.
    ScopedCAActionDisabler disabler;

    // Make this view host a transparent layer.
    background_layer_.reset([[CALayer alloc] init]);
    [background_layer_ setContentsGravity:kCAGravityTopLeft];
    [self setLayer:background_layer_];
    [self setWantsLayer:YES];

    compositor_.reset(new ui::Compositor(self));
    [view addSubview:self];
  }
  return self;
}

// This function closely mirrors RenderWidgetHostViewMac::LayoutLayers. When
// only delegated rendering is supported, only one copy of this code will
// need to exist.
- (void)layoutLayers {
  // Disable animation of the layers' resizing or repositioning.
  ScopedCAActionDisabler disabler;

  NSSize superview_frame_size = [[self superview] frame].size;
  [self setFrame:NSMakeRect(
      0, 0, superview_frame_size.width, superview_frame_size.height)];

  CGRect new_background_frame = CGRectMake(
      0,
      0,
      superview_frame_size.width,
      superview_frame_size.height);
  [background_layer_ setFrame:new_background_frame];

  // The bounds of the accelerated layer determine the size of the GL surface
  // that will be drawn to. Make sure that this is big enough to draw the
  // IOSurface.
  if (accelerated_layer_) {
    CGRect layer_bounds = CGRectMake(
      0,
      0,
      [accelerated_layer_ iosurface]->dip_io_surface_size().width(),
      [accelerated_layer_ iosurface]->dip_io_surface_size().height());
    CGPoint layer_position = CGPointMake(
      0,
      CGRectGetHeight(new_background_frame) - CGRectGetHeight(layer_bounds));
    bool bounds_changed = !CGRectEqualToRect(
        layer_bounds, [accelerated_layer_ bounds]);
    [accelerated_layer_ setBounds:layer_bounds];
    [accelerated_layer_ setPosition:layer_position];
    if (bounds_changed) {
      [accelerated_layer_ setNeedsDisplay];
      [accelerated_layer_ displayIfNeeded];
    }
  }

  // The content area of the software layer is the size of the image provided.
  // Make the bounds of the layer match the superview's bounds, to ensure that
  // the visible contents are drawn.
  [software_layer_ setBounds:new_background_frame];
}

- (ui::Compositor*)compositor {
  return compositor_.get();
}

- (void)gotAcceleratedIOSurfaceFrame:(uint64)surface_handle
                       withPixelSize:(gfx::Size)pixel_size
                     withScaleFactor:(float)scale_factor {
  ScopedCAActionDisabler disabler;

  // If there is not a layer for accelerated frames, create one.
  if (!accelerated_layer_) {
    // Disable the fade-in animation as the layer is added.
    ScopedCAActionDisabler disabler;
    scoped_refptr<content::CompositingIOSurfaceMac> iosurface =
        content::CompositingIOSurfaceMac::Create();
    accelerated_layer_.reset([[CompositingIOSurfaceLayer alloc]
        initWithIOSurface:iosurface
               withClient:NULL]);
    [[self layer] addSublayer:accelerated_layer_];
  }

  {
    bool result = true;
    gfx::ScopedCGLSetCurrentContext scoped_set_current_context(
        [accelerated_layer_ context]->cgl_context());
    result = [accelerated_layer_ iosurface]->SetIOSurfaceWithContextCurrent(
        [accelerated_layer_ context], surface_handle, pixel_size, scale_factor);
    // TODO(ccameron): On failure, poison the GL context, tear down the layers,
    // and request a new frame.
    ignore_result(result);
  }
  [accelerated_layer_ gotNewFrame];
  [self layoutLayers];

  // If there was a software layer, remove it.
  if (software_layer_) {
    // Disable the fade-out animation as the layer is removed.
    ScopedCAActionDisabler disabler;
    [software_layer_ removeFromSuperlayer];
    software_layer_.reset();
  }
}

- (void)gotSoftwareFrame:(cc::SoftwareFrameData*)frame_data
         withScaleFactor:(float)scale_factor
              withCanvas:(SkCanvas*)canvas {
  if (!frame_data || !canvas)
    return;

  // If there is not a layer for software frames, create one.
  if (!software_layer_) {
    // Disable the fade-in animation as the layer is added.
    ScopedCAActionDisabler disabler;
    software_layer_.reset([[SoftwareLayer alloc] init]);
    [[self layer] addSublayer:software_layer_];
  }

  SkImageInfo info;
  size_t row_bytes;
  const void* pixels = canvas->peekPixels(&info, &row_bytes);
  [software_layer_ setContentsToData:pixels
                        withRowBytes:row_bytes
                       withPixelSize:gfx::Size(info.fWidth, info.fHeight)
                     withScaleFactor:scale_factor];
  [self layoutLayers];

  // If there was an accelerated layer, remove it.
  if (accelerated_layer_) {
    // Disable the fade-out animation as the layer is removed.
    ScopedCAActionDisabler disabler;
    [accelerated_layer_ removeFromSuperlayer];
    accelerated_layer_.reset();
  }
}

@end  // BrowserCompositorViewMac
