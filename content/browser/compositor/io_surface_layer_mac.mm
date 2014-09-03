// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/compositor/io_surface_layer_mac.h"

#include <CoreFoundation/CoreFoundation.h>
#include <OpenGL/CGLIOSurface.h>
#include <OpenGL/CGLRenderers.h>
#include <OpenGL/OpenGL.h>

#include "base/mac/mac_util.h"
#include "base/mac/sdk_forward_declarations.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_view_mac.h"
#include "ui/base/cocoa/animation_utils.h"
#include "ui/gfx/size_conversions.h"
#include "ui/gl/scoped_cgl.h"
#include "ui/gl/gpu_switching_manager.h"

// Convenience macro for checking errors in the below code.
#define CHECK_GL_ERROR() do {                                           \
    GLenum gl_error = glGetError();                                     \
    LOG_IF(ERROR, gl_error != GL_NO_ERROR) << "GL Error: " << gl_error; \
  } while (0)

////////////////////////////////////////////////////////////////////////////////
// IOSurfaceLayer(Private)

@interface IOSurfaceLayer(Private)
// Force a draw immediately, but only if one was requested.
- (void)displayIfNeededAndAck;

// Called  when it has been a fixed interval of time and a frame has yet to be
// drawn.
- (void)timerFired;
@end

////////////////////////////////////////////////////////////////////////////////
// IOSurfaceLayerHelper

namespace content {

// IOSurfaceLayerHelper provides C++ functionality needed for the
// IOSurfaceLayer class (interfacing with base::DelayTimer).
class IOSurfaceLayerHelper {
 public:
  IOSurfaceLayerHelper(IOSurfaceLayer* layer);
  ~IOSurfaceLayerHelper();
  void ResetTimer();

 private:
  void TimerFired();

  // The layer that owns this helper.
  IOSurfaceLayer* const layer_;

  // The browser places back-pressure on the GPU by not acknowledging swap
  // calls until they appear on the screen. This can lead to hangs if the
  // view is moved offscreen (among other things). Prevent hangs by always
  // acknowledging the frame after timeout of 1/6th of a second  has passed.
  base::DelayTimer<IOSurfaceLayerHelper> timer_;
};


IOSurfaceLayerHelper::IOSurfaceLayerHelper(
    IOSurfaceLayer* layer)
        : layer_(layer),
          timer_(
              FROM_HERE,
              base::TimeDelta::FromSeconds(1) / 6,
              this,
              &IOSurfaceLayerHelper::TimerFired) {}

IOSurfaceLayerHelper::~IOSurfaceLayerHelper() {
}

void IOSurfaceLayerHelper::ResetTimer() {
  timer_.Reset();
}

void IOSurfaceLayerHelper::TimerFired() {
  [layer_ timerFired];
}

}  // namespace content

////////////////////////////////////////////////////////////////////////////////
// IOSurfaceLayer

@implementation IOSurfaceLayer

- (id)initWithClient:(content::IOSurfaceLayerClient*)client
     withScaleFactor:(float)scale_factor {
  if (self = [super init]) {
    client_ = client;
    helper_.reset(new content::IOSurfaceLayerHelper(self));
    needs_display_ = false;
    has_pending_frame_ = false;
    did_not_draw_counter_ = 0;
    is_pumping_frames_ = false;
    io_surface_texture_ = 0;
    io_surface_texture_dirty_ = false;
    cgl_renderer_id_ = 0;

    [self setBackgroundColor:CGColorGetConstantColor(kCGColorWhite)];
    [self setAnchorPoint:CGPointMake(0, 0)];
    // Setting contents gravity is necessary to prevent the layer from being
    // scaled during dyanmic resizes (especially with devtools open).
    [self setContentsGravity:kCAGravityTopLeft];
    if ([self respondsToSelector:(@selector(setContentsScale:))])
      [self setContentsScale:scale_factor];
  }
  return self;
}

- (void)dealloc {
  DCHECK(!helper_ && !client_);
  [super dealloc];
}

- (float)scaleFactor {
  if ([self respondsToSelector:(@selector(contentsScale))])
    return [self contentsScale];
  return 1;
}

- (int)rendererID {
  return cgl_renderer_id_;
}

- (void)timerFired {
  [self displayIfNeededAndAck];
}

- (void)resetClient {
  // Any acks that were waiting on this layer to draw will not occur, so ack
  // them now to prevent blocking the renderer.
  [self ackPendingFrame];
  helper_.reset();
  client_ = NULL;
}

- (void)gotFrameWithIOSurface:(IOSurfaceID)io_surface_id
                withPixelSize:(gfx::Size)pixel_size
              withScaleFactor:(float)scale_factor {
  // A trace value of 2 indicates that there is a pending swap ack. See
  // canDrawInCGLContext for other value meanings.
  TRACE_COUNTER_ID1("browser", "PendingSwapAck", self, 2);
  has_pending_frame_ = true;
  needs_display_ = true;
  helper_->ResetTimer();

  frame_pixel_size_ = pixel_size;

  // If this is a new IOSurface, open the IOSurface and mark that the
  // GL texture needs to bind to the new surface.
  if (!io_surface_ || io_surface_id != IOSurfaceGetID(io_surface_)) {
    io_surface_.reset(IOSurfaceLookup(io_surface_id));
    io_surface_texture_dirty_ = true;
    if (!io_surface_) {
      LOG(ERROR) << "Failed to open IOSurface for frame";
      if (client_)
        client_->IOSurfaceLayerHitError();
    }
  }

  // If reqested, draw immediately and don't bother trying to use the
  // isAsynchronous property to ensure smooth animation. If this is while
  // frames are being pumped then ack and display immediately to get a
  // correct-sized frame displayed as soon as possible.
  if (is_pumping_frames_ ||
      (client_ && client_->IOSurfaceLayerShouldAckImmediately())) {
    [self setNeedsDisplayAndDisplayAndAck];
  } else {
    if (![self isAsynchronous])
      [self setAsynchronous:YES];
  }
}

- (BOOL)canDrawInCGLContext:(CGLContextObj)glContext
                pixelFormat:(CGLPixelFormatObj)pixelFormat
               forLayerTime:(CFTimeInterval)timeInterval
                displayTime:(const CVTimeStamp*)timeStamp {
  // If we return NO 30 times in a row, switch to being synchronous to avoid
  // burning CPU cycles on this callback.
  if (needs_display_) {
    did_not_draw_counter_ = 0;
  } else {
    did_not_draw_counter_ += 1;
    if (did_not_draw_counter_ == 30)
      [self setAsynchronous:NO];
  }

  // Add an instantaneous blip to the PendingSwapAck state to indicate
  // that CoreAnimation asked if a frame is ready. A blip up to to 3 (usually
  // from 2, indicating that a swap ack is pending) indicates that we
  // requested a draw. A blip up to 1 (usually from 0, indicating there is no
  // pending swap ack) indicates that we did not request a draw. This would
  // be more natural to do with a tracing pseudo-thread
  // http://crbug.com/366300
  TRACE_COUNTER_ID1("browser", "PendingSwapAck", self, needs_display_ ? 3 : 1);
  TRACE_COUNTER_ID1("browser", "PendingSwapAck", self,
                    has_pending_frame_ ? 2 : 0);

  return needs_display_;
}

- (void)ackPendingFrame {
  if (!has_pending_frame_)
    return;
  has_pending_frame_ = false;
  if (client_)
    client_->IOSurfaceLayerDidDrawFrame();
  // A trace value of 0 indicates that there is no longer a pending swap ack.
  TRACE_COUNTER_ID1("browser", "PendingSwapAck", self, 0);
}

- (void)setNeedsDisplayAndDisplayAndAck {
  // Drawing using setNeedsDisplay and displayIfNeeded will result in
  // subsequent canDrawInCGLContext callbacks getting dropped, and jerky
  // animation. Disable asynchronous drawing before issuing these calls as a
  // workaround.
  // http://crbug.com/395827
  if ([self isAsynchronous])
    [self setAsynchronous:NO];

  [self setNeedsDisplay];
  [self displayIfNeededAndAck];
}

- (void)displayIfNeededAndAck {
  if (!needs_display_)
    return;

  // As in SetNeedsDisplayAndDisplayAndAck, disable asynchronous drawing before
  // issuing displayIfNeeded.
  // http://crbug.com/395827
  if ([self isAsynchronous])
    [self setAsynchronous:NO];

  // Do not bother drawing while pumping new frames -- wait until the waiting
  // block ends to draw any of the new frames.
  if (!is_pumping_frames_)
    [self displayIfNeeded];

  // Calls to setNeedsDisplay can sometimes be ignored, especially if issued
  // rapidly (e.g, with vsync off). This is unacceptable because the failure
  // to ack a single frame will hang the renderer. Ensure that the renderer
  // not be blocked by lying and claiming that we drew the frame.
  [self ackPendingFrame];
}

- (void)beginPumpingFrames {
  is_pumping_frames_ = true;
}

- (void)endPumpingFrames {
  is_pumping_frames_ = false;
  [self displayIfNeededAndAck];
}

// The remaining methods implement the CAOpenGLLayer interface.

- (CGLPixelFormatObj)copyCGLPixelFormatForDisplayMask:(uint32_t)mask {
  // Create the pixel format object for the context.
  std::vector<CGLPixelFormatAttribute> attribs;
  attribs.push_back(kCGLPFADepthSize);
  attribs.push_back(static_cast<CGLPixelFormatAttribute>(0));
  if (ui::GpuSwitchingManager::GetInstance()->SupportsDualGpus()) {
    attribs.push_back(kCGLPFAAllowOfflineRenderers);
    attribs.push_back(static_cast<CGLPixelFormatAttribute>(1));
  }
  attribs.push_back(static_cast<CGLPixelFormatAttribute>(0));
  GLint number_virtual_screens = 0;
  base::ScopedTypeRef<CGLPixelFormatObj> pixel_format;
  CGLError error = CGLChoosePixelFormat(
      &attribs.front(), pixel_format.InitializeInto(), &number_virtual_screens);
  if (error != kCGLNoError) {
    LOG(ERROR) << "Failed to create pixel format object.";
    return NULL;
  }
  return CGLRetainPixelFormat(pixel_format);
}

- (void)releaseCGLContext:(CGLContextObj)glContext {
  // Destroy the GL resources used by this context.
  if (io_surface_texture_) {
    gfx::ScopedCGLSetCurrentContext scoped_set_current_context(glContext);
    glDeleteTextures(1, &io_surface_texture_);
    io_surface_texture_ = 0;
  }
  io_surface_texture_dirty_ = true;
  cgl_renderer_id_ = 0;
  [super releaseCGLContext:(CGLContextObj)glContext];
}

- (void)setNeedsDisplay {
  needs_display_ = true;
  [super setNeedsDisplay];
}

- (void)drawInCGLContext:(CGLContextObj)glContext
             pixelFormat:(CGLPixelFormatObj)pixelFormat
            forLayerTime:(CFTimeInterval)timeInterval
             displayTime:(const CVTimeStamp*)timeStamp {
  TRACE_EVENT0("browser", "IOSurfaceLayer::drawInCGLContext");

  // Create the texture if it has not been created in this context yet.
  if (!io_surface_texture_) {
    glGenTextures(1, &io_surface_texture_);
    glBindTexture(GL_TEXTURE_RECTANGLE_ARB, io_surface_texture_);
    glTexParameteri(
        GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(
        GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glBindTexture(GL_TEXTURE_RECTANGLE_ARB, 0);
    io_surface_texture_dirty_ = true;
  }

  // Associate the IOSurface with this texture, if the underlying IOSurface has
  // been changed.
  if (io_surface_texture_dirty_ && io_surface_) {
    glBindTexture(GL_TEXTURE_RECTANGLE_ARB, io_surface_texture_);
    CGLError cgl_error = CGLTexImageIOSurface2D(
        glContext,
        GL_TEXTURE_RECTANGLE_ARB,
        GL_RGBA,
        IOSurfaceGetWidth(io_surface_),
        IOSurfaceGetHeight(io_surface_),
        GL_BGRA,
        GL_UNSIGNED_INT_8_8_8_8_REV,
        io_surface_.get(),
        0 /* plane */);
    glBindTexture(GL_TEXTURE_RECTANGLE_ARB, 0);
    if (cgl_error != kCGLNoError) {
      LOG(ERROR) << "CGLTexImageIOSurface2D failed with " << cgl_error;
      glDeleteTextures(1, &io_surface_texture_);
      io_surface_texture_ = 0;
      if (client_)
        client_->IOSurfaceLayerHitError();
    }
  } else if (io_surface_texture_) {
    glDeleteTextures(1, &io_surface_texture_);
    io_surface_texture_ = 0;
  }

  // Fill the viewport with the texture. The viewport must be smaller or equal
  // than the texture, because it is resized as frames arrive.
  if (io_surface_texture_) {
    GLint viewport[4];
    glGetIntegerv(GL_VIEWPORT, viewport);
    gfx::Size viewport_pixel_size(viewport[2], viewport[3]);
    DCHECK_LE(
        viewport_pixel_size.width(),
        frame_pixel_size_.width());
    DCHECK_LE(
        viewport_pixel_size.height(),
        frame_pixel_size_.height());

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, viewport_pixel_size.width(),
            0, viewport_pixel_size.height(), -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    glColor4f(1, 1, 1, 1);
    glEnable(GL_TEXTURE_RECTANGLE_ARB);
    glBindTexture(GL_TEXTURE_RECTANGLE_ARB, io_surface_texture_);
    glBegin(GL_QUADS);
    glTexCoord2f(0, 0);
    glVertex2f(0, 0);
    glTexCoord2f(viewport[2], 0);
    glVertex2f(frame_pixel_size_.width(), 0);
    glTexCoord2f(viewport[2], viewport[3]);
    glVertex2f(frame_pixel_size_.width(), frame_pixel_size_.height());
    glTexCoord2f(0, viewport[3]);
    glVertex2f(0, frame_pixel_size_.height());
    glEnd();
    glBindTexture(GL_TEXTURE_RECTANGLE_ARB, 0);
    glDisable(GL_TEXTURE_RECTANGLE_ARB);

    // Workaround for issue 158469. Issue a dummy draw call with
    // io_surface_texture_ not bound to a texture, in order to shake all
    // references to the IOSurface out of the driver.
    glBegin(GL_TRIANGLES);
    glEnd();
  } else {
    glClearColor(1, 1, 1, 1);
    glClear(GL_COLOR_BUFFER_BIT);
  }

  // Query the current GL renderer to send back to the GPU process.
  {
    CGLError cgl_error = CGLGetParameter(
        glContext, kCGLCPCurrentRendererID, &cgl_renderer_id_);
    if (cgl_error == kCGLNoError) {
      cgl_renderer_id_ &= kCGLRendererIDMatchingMask;
    } else {
      LOG(ERROR) << "CGLGetParameter for kCGLCPCurrentRendererID failed with "
                 << cgl_error;
      cgl_renderer_id_ = 0;
    }
  }

  // If we hit any errors, tell the client.
  while (GLenum gl_error = glGetError()) {
    LOG(ERROR) << "Hit GL error " << gl_error;
    if (client_)
      client_->IOSurfaceLayerHitError();
  }

  needs_display_ = false;
  [super drawInCGLContext:glContext
              pixelFormat:pixelFormat
             forLayerTime:timeInterval
              displayTime:timeStamp];

  [self ackPendingFrame];
}

@end
