// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/gpu/image_transport_surface_calayer_mac.h"

#include "base/command_line.h"
#include "base/mac/sdk_forward_declarations.h"
#include "content/common/gpu/surface_handle_types_mac.h"
#include "ui/base/cocoa/animation_utils.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/gl/gl_gl_api_implementation.h"
#include "ui/gl/gl_switches.h"

@interface ImageTransportLayer : CAOpenGLLayer {
  content::CALayerStorageProvider* storageProvider_;
}
- (id)initWithStorageProvider:(content::CALayerStorageProvider*)storageProvider;
- (void)resetStorageProvider;
@end

@implementation ImageTransportLayer

- (id)initWithStorageProvider:
    (content::CALayerStorageProvider*)storageProvider {
  if (self = [super init])
    storageProvider_ = storageProvider;
  return self;
}

- (void)resetStorageProvider {
  if (storageProvider_)
    storageProvider_->LayerResetStorageProvider();
  storageProvider_ = NULL;
}

- (CGLPixelFormatObj)copyCGLPixelFormatForDisplayMask:(uint32_t)mask {
  if (!storageProvider_)
    return NULL;
  return CGLRetainPixelFormat(CGLGetPixelFormat(
      storageProvider_->LayerShareGroupContext()));
}

- (CGLContextObj)copyCGLContextForPixelFormat:(CGLPixelFormatObj)pixelFormat {
  if (!storageProvider_)
    return NULL;
  CGLContextObj context = NULL;
  CGLError error = CGLCreateContext(
      pixelFormat, storageProvider_->LayerShareGroupContext(), &context);
  if (error != kCGLNoError)
    DLOG(ERROR) << "CGLCreateContext failed with CGL error: " << error;
  return context;
}

- (BOOL)canDrawInCGLContext:(CGLContextObj)glContext
                pixelFormat:(CGLPixelFormatObj)pixelFormat
               forLayerTime:(CFTimeInterval)timeInterval
                displayTime:(const CVTimeStamp*)timeStamp {
  if (!storageProvider_)
    return NO;
  return storageProvider_->LayerCanDraw();
}

- (void)drawInCGLContext:(CGLContextObj)glContext
             pixelFormat:(CGLPixelFormatObj)pixelFormat
            forLayerTime:(CFTimeInterval)timeInterval
             displayTime:(const CVTimeStamp*)timeStamp {
  // While in this callback, CoreAnimation has set |glContext| to be current.
  // Ensure that the GL calls that we make are made against the native GL API.
  gfx::ScopedSetGLToRealGLApi scoped_set_gl_api;

  if (storageProvider_) {
    storageProvider_->LayerDoDraw();
  } else {
    glClearColor(1, 1, 1, 1);
    glClear(GL_COLOR_BUFFER_BIT);
  }
  [super drawInCGLContext:glContext
              pixelFormat:pixelFormat
             forLayerTime:timeInterval
              displayTime:timeStamp];
}

@end

namespace content {

CALayerStorageProvider::CALayerStorageProvider(
    ImageTransportSurfaceFBO* transport_surface)
        : transport_surface_(transport_surface),
          gpu_vsync_disabled_(CommandLine::ForCurrentProcess()->HasSwitch(
              switches::kDisableGpuVsync)),
          has_pending_draw_(false),
          can_draw_returned_false_count_(0),
          fbo_texture_(0),
          fbo_scale_factor_(1),
          weak_factory_(this) {}

CALayerStorageProvider::~CALayerStorageProvider() {
}

gfx::Size CALayerStorageProvider::GetRoundedSize(gfx::Size size) {
  return size;
}

bool CALayerStorageProvider::AllocateColorBufferStorage(
    CGLContextObj context, GLuint texture,
    gfx::Size pixel_size, float scale_factor) {
  // Allocate an ordinary OpenGL texture to back the FBO.
  GLenum error;
  while ((error = glGetError()) != GL_NO_ERROR) {
    DLOG(ERROR) << "Error found (and ignored) before allocating buffer "
                << "storage: " << error;
  }
  glTexImage2D(GL_TEXTURE_RECTANGLE_ARB,
               0,
               GL_RGBA,
               pixel_size.width(),
               pixel_size.height(),
               0,
               GL_RGBA,
               GL_UNSIGNED_BYTE,
               NULL);
  error = glGetError();
  if (error != GL_NO_ERROR) {
    DLOG(ERROR) << "glTexImage failed with GL error: " << error;
    return false;
  }
  glFlush();

  // Disable the fade-in animation as the layer is changed.
  ScopedCAActionDisabler disabler;

  // Set the parameters that will be used to allocate the CALayer to draw the
  // texture into.
  share_group_context_.reset(CGLRetainContext(context));
  fbo_texture_ = texture;
  fbo_pixel_size_ = pixel_size;
  fbo_scale_factor_ = scale_factor;
  return true;
}

void CALayerStorageProvider::FreeColorBufferStorage() {
  // We shouldn't be asked to free a texture when we still have yet to draw it.
  DCHECK(!has_pending_draw_);
  has_pending_draw_ = false;
  can_draw_returned_false_count_ = 0;

  // Note that |context_| still holds a reference to |layer_|, and will until
  // a new frame is swapped in.
  [layer_ displayIfNeeded];
  [layer_ resetStorageProvider];
  layer_.reset();

  share_group_context_.reset();
  fbo_texture_ = 0;
  fbo_pixel_size_ = gfx::Size();
}

void CALayerStorageProvider::SwapBuffers(
    const gfx::Size& size, float scale_factor) {
  DCHECK(!has_pending_draw_);
  has_pending_draw_ = true;

  // Allocate a CAContext to use to transport the CALayer to the browser
  // process.
  if (!context_) {
    base::scoped_nsobject<NSDictionary> dict([[NSDictionary alloc] init]);
    CGSConnectionID connection_id = CGSMainConnectionID();
    context_.reset([CAContext contextWithCGSConnection:connection_id
                                               options:dict]);
    [context_ retain];
  }

  // Allocate a CALayer to use to draw the content.
  if (!layer_) {
    layer_.reset([[ImageTransportLayer alloc] initWithStorageProvider:this]);
    gfx::Size dip_size(gfx::ToFlooredSize(gfx::ScaleSize(
        fbo_pixel_size_, 1.0f / fbo_scale_factor_)));
    [layer_ setContentsScale:fbo_scale_factor_];
    [layer_ setFrame:CGRectMake(0, 0, dip_size.width(), dip_size.height())];

    // Make the CALayer current to the CAContext and display its contents
    // immediately.
    [context_ setLayer:layer_];
  }

  // Tell CoreAnimation to draw our frame. We will send the IPC to the browser
  // when CoreAnimation has drawn our frame.
  if (gpu_vsync_disabled_) {
    DrawWithVsyncDisabled();
  } else {
    if (![layer_ isAsynchronous])
      [layer_ setAsynchronous:YES];
  }
}

void CALayerStorageProvider::DrawWithVsyncDisabled() {
  DCHECK(has_pending_draw_);
  [layer_ setNeedsDisplay];

  // Sometimes, setNeedsDisplay calls are dropped on the floor. Make this not
  // hang the renderer by re-issuing the call if the draw has not yet
  // happened.
  if (has_pending_draw_) {
    // Delay sending another draw immediately to avoid starving the run loop.
    base::MessageLoop::current()->PostDelayedTask(
        FROM_HERE,
        base::Bind(&CALayerStorageProvider::DrawWithVsyncDisabled,
                   weak_factory_.GetWeakPtr()),
        base::TimeDelta::FromMilliseconds(5));
  }
}

void CALayerStorageProvider::WillWriteToBackbuffer() {
  // TODO(ccameron): The browser may need to continue issuing swaps even when
  // they do not draw. In these cases it is necessary to either double-buffer
  // the resulting texture, or to drop frames.
}

void CALayerStorageProvider::DiscardBackbuffer() {
  // If this surface's backbuffer is discarded, it is because this surface has
  // been made non-visible. Ensure that the previous contents are not briefly
  // flashed when this is made visible by creating a new CALayer and CAContext
  // at the next swap.
  [layer_ resetStorageProvider];
  layer_.reset();
  context_.reset();
}

void CALayerStorageProvider::SwapBuffersAckedByBrowser() {
}

CGLContextObj CALayerStorageProvider::LayerShareGroupContext() {
  return share_group_context_;
}

bool CALayerStorageProvider::LayerCanDraw() {
  if (has_pending_draw_) {
    can_draw_returned_false_count_ = 0;
    return true;
  } else {
    if ([layer_ isAsynchronous]) {
      DCHECK(!gpu_vsync_disabled_);
      // If we are in asynchronous mode, we will be getting callbacks at every
      // vsync, asking us if we have anything to draw. If we get 30 of these in
      // a row, ask that we stop getting these callback for now, so that we
      // don't waste CPU cycles.
      if (can_draw_returned_false_count_ == 30)
        [layer_ setAsynchronous:NO];
      else
        can_draw_returned_false_count_ += 1;
    }
    return false;
  }
}

void CALayerStorageProvider::LayerDoDraw() {
  GLint viewport[4] = {0, 0, 0, 0};
  glGetIntegerv(GL_VIEWPORT, viewport);
  gfx::Size viewport_size(viewport[2], viewport[3]);

  // Set the coordinate system to be one-to-one with pixels.
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glOrtho(0, viewport_size.width(), 0, viewport_size.height(), -1, 1);
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();

  // Draw a fullscreen quad.
  glColor4f(1, 1, 1, 1);
  glEnable(GL_TEXTURE_RECTANGLE_ARB);
  glBindTexture(GL_TEXTURE_RECTANGLE_ARB, fbo_texture_);
  glBegin(GL_QUADS);
  {
    glTexCoord2f(0, 0);
    glVertex2f(0, 0);

    glTexCoord2f(0, fbo_pixel_size_.height());
    glVertex2f(0, fbo_pixel_size_.height());

    glTexCoord2f(fbo_pixel_size_.width(), fbo_pixel_size_.height());
    glVertex2f(fbo_pixel_size_.width(), fbo_pixel_size_.height());

    glTexCoord2f(fbo_pixel_size_.width(), 0);
    glVertex2f(fbo_pixel_size_.width(), 0);
  }
  glEnd();
  glBindTexture(GL_TEXTURE_RECTANGLE_ARB, 0);
  glDisable(GL_TEXTURE_RECTANGLE_ARB);

  // Allow forward progress in the context now that the swap is complete.
  DCHECK(has_pending_draw_);
  SendPendingSwapToBrowserAfterFrameDrawn();
}

void CALayerStorageProvider::LayerResetStorageProvider() {
  // If we are providing back-pressure by waiting for a draw, that draw will
  // now never come, so release the pressure now.
  SendPendingSwapToBrowserAfterFrameDrawn();
}

void CALayerStorageProvider::SendPendingSwapToBrowserAfterFrameDrawn() {
  if (!has_pending_draw_)
    return;
  weak_factory_.InvalidateWeakPtrs();
  has_pending_draw_ = false;
  transport_surface_->SendSwapBuffers(
      SurfaceHandleFromCAContextID([context_ contextId]),
      fbo_pixel_size_,
      fbo_scale_factor_);
}

}  //  namespace content
