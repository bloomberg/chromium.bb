// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/gpu/image_transport_surface_calayer_mac.h"

#include <OpenGL/CGLRenderers.h>

#include "base/command_line.h"
#include "base/mac/sdk_forward_declarations.h"
#include "ui/accelerated_widget_mac/surface_handle_types.h"
#include "ui/base/cocoa/animation_utils.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/gl/gl_gl_api_implementation.h"
#include "ui/gl/gl_switches.h"
#include "ui/gl/gpu_switching_manager.h"

namespace {
const size_t kFramesToKeepCAContextAfterDiscard = 2;
const size_t kCanDrawFalsesBeforeSwitchFromAsync = 4;
}

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
    LOG(ERROR) << "CGLCreateContext failed with CGL error: " << error;
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
      gpu_vsync_disabled_(base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableGpuVsync)),
      throttling_disabled_(false),
      has_pending_draw_(false),
      can_draw_returned_false_count_(0),
      fbo_texture_(0),
      fbo_scale_factor_(1),
      recreate_layer_after_gpu_switch_(false),
      pending_draw_weak_factory_(this) {
  ui::GpuSwitchingManager::GetInstance()->AddObserver(this);
}

CALayerStorageProvider::~CALayerStorageProvider() {
  ui::GpuSwitchingManager::GetInstance()->RemoveObserver(this);
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
    LOG(ERROR) << "OpenGL error hit but ignored before allocating buffer "
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
  glFlush();

  bool hit_error = false;
  while ((error = glGetError()) != GL_NO_ERROR) {
    LOG(ERROR) << "OpenGL error hit while trying to allocate buffer storage: "
               << error;
    hit_error = true;
  }
  if (hit_error)
    return false;

  // Set the parameters that will be used to allocate the CALayer to draw the
  // texture into.
  share_group_context_.reset(CGLRetainContext(context));
  fbo_texture_ = texture;
  fbo_pixel_size_ = pixel_size;
  fbo_scale_factor_ = scale_factor;
  return true;
}

void CALayerStorageProvider::FreeColorBufferStorage() {
  // Note that |context_| still holds a reference to |layer_|, and will until
  // a new frame is swapped in.
  [layer_ resetStorageProvider];
  layer_.reset();

  share_group_context_.reset();
  fbo_texture_ = 0;
  fbo_pixel_size_ = gfx::Size();
  can_draw_returned_false_count_ = 0;
}

void CALayerStorageProvider::SwapBuffers(
    const gfx::Size& size, float scale_factor) {
  DCHECK(!has_pending_draw_);

  // Recreate the CALayer on the new GPU if a GPU switch has occurred. Note
  // that the CAContext will retain a reference to the old CALayer until the
  // call to -[CAContext setLayer:] replaces the old CALayer with the new one.
  if (recreate_layer_after_gpu_switch_) {
    [layer_ resetStorageProvider];
    layer_.reset();
    recreate_layer_after_gpu_switch_ = false;
  }

  // Set the pending draw flag only after destroying the old layer (otherwise
  // destroying it will un-set the flag).
  has_pending_draw_ = true;

  // Allocate a CAContext to use to transport the CALayer to the browser
  // process, if needed.
  if (!context_) {
    base::scoped_nsobject<NSDictionary> dict([[NSDictionary alloc] init]);
    CGSConnectionID connection_id = CGSMainConnectionID();
    context_.reset([CAContext contextWithCGSConnection:connection_id
                                               options:dict]);
    [context_ retain];
  }

  // Allocate a CALayer to use to draw the content and make it current to the
  // CAContext, if needed.
  if (!layer_) {
    layer_.reset([[ImageTransportLayer alloc] initWithStorageProvider:this]);
    gfx::Size dip_size(gfx::ToFlooredSize(gfx::ScaleSize(
        fbo_pixel_size_, 1.0f / fbo_scale_factor_)));
    [layer_ setContentsScale:fbo_scale_factor_];
    [layer_ setFrame:CGRectMake(0, 0, dip_size.width(), dip_size.height())];

    [context_ setLayer:layer_];
  }

  // Replacing the CAContext's CALayer will sometimes results in an immediate
  // draw.
  if (!has_pending_draw_)
    return;

  // Tell CoreAnimation to draw our frame.
  if (gpu_vsync_disabled_ || throttling_disabled_) {
    DrawImmediatelyAndUnblockBrowser();
  } else {
    if (![layer_ isAsynchronous])
      [layer_ setAsynchronous:YES];

    // If CoreAnimation doesn't end up drawing our frame, un-block the browser
    // after a timeout of 1/6th of a second has passed.
    base::MessageLoop::current()->PostDelayedTask(
        FROM_HERE,
        base::Bind(&CALayerStorageProvider::DrawImmediatelyAndUnblockBrowser,
                   pending_draw_weak_factory_.GetWeakPtr()),
        base::TimeDelta::FromSeconds(1) / 6);
  }
}

void CALayerStorageProvider::DrawImmediatelyAndUnblockBrowser() {
  CHECK(has_pending_draw_);
  if ([layer_ isAsynchronous])
    [layer_ setAsynchronous:NO];
  [layer_ setNeedsDisplay];
  [layer_ displayIfNeeded];

  // Sometimes, the setNeedsDisplay+displayIfNeeded pairs have no effect. This
  // can happen if the NSView that this layer is attached to isn't in the
  // window hierarchy (e.g, tab capture of a backgrounded tab). In this case,
  // the frame will never be seen, so drop it.
  UnblockBrowserIfNeeded();
}

void CALayerStorageProvider::WillWriteToBackbuffer() {
  // The browser should always throttle itself so that there are no pending
  // draws when the output surface is written to, but in the event of things
  // like context lost, or changing context, this will not be true. If there
  // exists a pending draw, flush it immediately to maintain a consistent
  // state.
  if (has_pending_draw_)
    DrawImmediatelyAndUnblockBrowser();
}

void CALayerStorageProvider::DiscardBackbuffer() {
  // If this surface's backbuffer is discarded, it is because this surface has
  // been made non-visible. Ensure that the previous contents are not briefly
  // flashed when this is made visible by creating a new CALayer and CAContext
  // at the next swap.
  [layer_ resetStorageProvider];
  layer_.reset();

  // If we remove all references to the CAContext in this process, it will be
  // blanked-out in the browser process (even if the browser process is inside
  // a NSDisableScreenUpdates block). Ensure that the context is kept around
  // until a fixed number of frames (determined empirically) have been acked.
  // http://crbug.com/425819
  while (previously_discarded_contexts_.size() <
      kFramesToKeepCAContextAfterDiscard) {
    previously_discarded_contexts_.push_back(
        base::scoped_nsobject<CAContext>());
  }
  previously_discarded_contexts_.push_back(context_);

  context_.reset();
}

void CALayerStorageProvider::SwapBuffersAckedByBrowser(
    bool disable_throttling) {
  throttling_disabled_ = disable_throttling;
  if (!previously_discarded_contexts_.empty())
    previously_discarded_contexts_.pop_front();
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
      // vsync, asking us if we have anything to draw. If we get many of these
      // in a row, ask that we stop getting these callback for now, so that we
      // don't waste CPU cycles.
      if (can_draw_returned_false_count_ >= kCanDrawFalsesBeforeSwitchFromAsync)
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

  GLint current_renderer_id = 0;
  if (CGLGetParameter(CGLGetCurrentContext(),
                      kCGLCPCurrentRendererID,
                      &current_renderer_id) == kCGLNoError) {
    current_renderer_id &= kCGLRendererIDMatchingMask;
    transport_surface_->SetRendererID(current_renderer_id);
  }

  GLenum error;
  while ((error = glGetError()) != GL_NO_ERROR) {
    LOG(ERROR) << "OpenGL error hit while drawing frame: " << error;
  }

  // Allow forward progress in the context now that the swap is complete.
  UnblockBrowserIfNeeded();
}

void CALayerStorageProvider::LayerResetStorageProvider() {
  // If we are providing back-pressure by waiting for a draw, that draw will
  // now never come, so release the pressure now.
  UnblockBrowserIfNeeded();
}

void CALayerStorageProvider::OnGpuSwitched() {
  recreate_layer_after_gpu_switch_ = true;
}

void CALayerStorageProvider::UnblockBrowserIfNeeded() {
  if (!has_pending_draw_)
    return;
  pending_draw_weak_factory_.InvalidateWeakPtrs();
  has_pending_draw_ = false;
  transport_surface_->SendSwapBuffers(
      ui::SurfaceHandleFromCAContextID([context_ contextId]),
      fbo_pixel_size_,
      fbo_scale_factor_);
}

}  //  namespace content
