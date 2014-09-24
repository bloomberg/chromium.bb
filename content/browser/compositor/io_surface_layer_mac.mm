// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/compositor/io_surface_layer_mac.h"

#include <CoreFoundation/CoreFoundation.h>
#include <OpenGL/CGLIOSurface.h>
#include <OpenGL/CGLRenderers.h>
#include <OpenGL/gl.h>
#include <OpenGL/OpenGL.h>

#include "base/mac/mac_util.h"
#include "base/mac/sdk_forward_declarations.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_view_mac.h"
#include "content/browser/renderer_host/compositing_iosurface_context_mac.h"
#include "content/browser/renderer_host/compositing_iosurface_mac.h"
#include "ui/base/cocoa/animation_utils.h"
#include "ui/gfx/size_conversions.h"
#include "ui/gl/gpu_switching_manager.h"

////////////////////////////////////////////////////////////////////////////////
// IOSurfaceLayerHelper

namespace content {

IOSurfaceLayerHelper::IOSurfaceLayerHelper(
    IOSurfaceLayerClient* client,
    IOSurfaceLayer* layer)
        : client_(client),
          layer_(layer),
          needs_display_(false),
          has_pending_frame_(false),
          did_not_draw_counter_(0),
          is_pumping_frames_(false),
          timer_(
              FROM_HERE,
              base::TimeDelta::FromSeconds(1) / 6,
              this,
              &IOSurfaceLayerHelper::TimerFired) {}

IOSurfaceLayerHelper::~IOSurfaceLayerHelper() {
  // Any acks that were waiting on this layer to draw will not occur, so ack
  // them now to prevent blocking the renderer.
  AckPendingFrame(true);
}

void IOSurfaceLayerHelper::GotNewFrame() {
  // A trace value of 2 indicates that there is a pending swap ack. See
  // canDrawInCGLContext for other value meanings.
  TRACE_COUNTER_ID1("browser", "PendingSwapAck", this, 2);

  has_pending_frame_ = true;
  needs_display_ = true;
  timer_.Reset();

  // If reqested, draw immediately and don't bother trying to use the
  // isAsynchronous property to ensure smooth animation. If this is while
  // frames are being pumped then ack and display immediately to get a
  // correct-sized frame displayed as soon as possible.
  if (is_pumping_frames_ || client_->IOSurfaceLayerShouldAckImmediately()) {
    SetNeedsDisplayAndDisplayAndAck();
  } else {
    if (![layer_ isAsynchronous])
      [layer_ setAsynchronous:YES];
  }
}

void IOSurfaceLayerHelper::SetNeedsDisplay() {
  needs_display_ = true;
}

bool IOSurfaceLayerHelper::CanDraw() {
  // If we return NO 30 times in a row, switch to being synchronous to avoid
  // burning CPU cycles on this callback.
  if (needs_display_) {
    did_not_draw_counter_ = 0;
  } else {
    did_not_draw_counter_ += 1;
    if (did_not_draw_counter_ == 30)
      [layer_ setAsynchronous:NO];
  }

  // Add an instantaneous blip to the PendingSwapAck state to indicate
  // that CoreAnimation asked if a frame is ready. A blip up to to 3 (usually
  // from 2, indicating that a swap ack is pending) indicates that we
  // requested a draw. A blip up to 1 (usually from 0, indicating there is no
  // pending swap ack) indicates that we did not request a draw. This would
  // be more natural to do with a tracing pseudo-thread
  // http://crbug.com/366300
  TRACE_COUNTER_ID1("browser", "PendingSwapAck", this, needs_display_ ? 3 : 1);
  TRACE_COUNTER_ID1("browser", "PendingSwapAck", this,
                    has_pending_frame_ ? 2 : 0);

  return needs_display_;
}

void IOSurfaceLayerHelper::DidDraw(bool success) {
  needs_display_ = false;
  AckPendingFrame(success);
}

void IOSurfaceLayerHelper::AckPendingFrame(bool success) {
  if (!has_pending_frame_)
    return;
  has_pending_frame_ = false;
  if (success)
    client_->IOSurfaceLayerDidDrawFrame();
  else
    client_->IOSurfaceLayerHitError();
  // A trace value of 0 indicates that there is no longer a pending swap ack.
  TRACE_COUNTER_ID1("browser", "PendingSwapAck", this, 0);
}

void IOSurfaceLayerHelper::SetNeedsDisplayAndDisplayAndAck() {
  // Drawing using setNeedsDisplay and displayIfNeeded will result in
  // subsequent canDrawInCGLContext callbacks getting dropped, and jerky
  // animation. Disable asynchronous drawing before issuing these calls as a
  // workaround.
  // http://crbug.com/395827
  if ([layer_ isAsynchronous])
    [layer_ setAsynchronous:NO];

  [layer_ setNeedsDisplay];
  DisplayIfNeededAndAck();
}

void IOSurfaceLayerHelper::DisplayIfNeededAndAck() {
  if (!needs_display_)
    return;

  // As in SetNeedsDisplayAndDisplayAndAck, disable asynchronous drawing before
  // issuing displayIfNeeded.
  // http://crbug.com/395827
  if ([layer_ isAsynchronous])
    [layer_ setAsynchronous:NO];

  // Do not bother drawing while pumping new frames -- wait until the waiting
  // block ends to draw any of the new frames.
  if (!is_pumping_frames_)
    [layer_ displayIfNeeded];

  // Calls to setNeedsDisplay can sometimes be ignored, especially if issued
  // rapidly (e.g, with vsync off). This is unacceptable because the failure
  // to ack a single frame will hang the renderer. Ensure that the renderer
  // not be blocked by lying and claiming that we drew the frame.
  AckPendingFrame(true);
}

void IOSurfaceLayerHelper::TimerFired() {
  DisplayIfNeededAndAck();
}

void IOSurfaceLayerHelper::BeginPumpingFrames() {
  is_pumping_frames_ = true;
}

void IOSurfaceLayerHelper::EndPumpingFrames() {
  is_pumping_frames_ = false;
  DisplayIfNeededAndAck();
}

}  // namespace content

////////////////////////////////////////////////////////////////////////////////
// IOSurfaceLayer

@implementation IOSurfaceLayer

- (content::CompositingIOSurfaceMac*)iosurface {
  return iosurface_.get();
}

- (content::CompositingIOSurfaceContext*)context {
  return context_.get();
}

- (id)initWithClient:(content::IOSurfaceLayerClient*)client
     withScaleFactor:(float)scale_factor {
  if (self = [super init]) {
    helper_.reset(new content::IOSurfaceLayerHelper(client, self));

    iosurface_ = content::CompositingIOSurfaceMac::Create();
    context_ = content::CompositingIOSurfaceContext::Get(
        content::CompositingIOSurfaceContext::kCALayerContextWindowNumber);
    if (!iosurface_.get() || !context_.get()) {
      LOG(ERROR) << "Failed create CompositingIOSurface or context";
      [self resetClient];
      [self release];
      return nil;
    }

    [self setBackgroundColor:CGColorGetConstantColor(kCGColorWhite)];
    [self setAnchorPoint:CGPointMake(0, 0)];
    // Setting contents gravity is necessary to prevent the layer from being
    // scaled during dyanmic resizes (especially with devtools open).
    [self setContentsGravity:kCAGravityTopLeft];
    if ([self respondsToSelector:(@selector(setContentsScale:))]) {
      [self setContentsScale:scale_factor];
    }
  }
  return self;
}

- (void)dealloc {
  DCHECK(!helper_);
  [super dealloc];
}

- (bool)gotFrameWithIOSurface:(IOSurfaceID)io_surface_id
                withPixelSize:(gfx::Size)pixel_size
              withScaleFactor:(float)scale_factor {
  bool result = true;
  gfx::ScopedCGLSetCurrentContext scoped_set_current_context(
      context_->cgl_context());
  result = iosurface_->SetIOSurfaceWithContextCurrent(
      context_, io_surface_id, pixel_size, scale_factor);
  return result;
}

- (void)poisonContextAndSharegroup {
  context_->PoisonContextAndSharegroup();
}

- (bool)hasBeenPoisoned {
  return context_->HasBeenPoisoned();
}

- (float)scaleFactor {
  return iosurface_->scale_factor();
}

- (int)rendererID {
  return iosurface_->GetRendererID();
}

- (void)resetClient {
  helper_.reset();
}

- (void)gotNewFrame {
  helper_->GotNewFrame();
}

- (void)setNeedsDisplayAndDisplayAndAck {
  helper_->SetNeedsDisplayAndDisplayAndAck();
}

- (void)displayIfNeededAndAck {
  helper_->DisplayIfNeededAndAck();
}

- (void)beginPumpingFrames {
  helper_->BeginPumpingFrames();
}

- (void)endPumpingFrames {
  helper_->EndPumpingFrames();
}

// The remaining methods implement the CAOpenGLLayer interface.

- (CGLPixelFormatObj)copyCGLPixelFormatForDisplayMask:(uint32_t)mask {
  if (!context_.get())
    return [super copyCGLPixelFormatForDisplayMask:mask];
  return CGLRetainPixelFormat(CGLGetPixelFormat(context_->cgl_context()));
}

- (CGLContextObj)copyCGLContextForPixelFormat:(CGLPixelFormatObj)pixelFormat {
  if (!context_.get())
    return [super copyCGLContextForPixelFormat:pixelFormat];
  return CGLRetainContext(context_->cgl_context());
}

- (void)setNeedsDisplay {
  if (helper_)
    helper_->SetNeedsDisplay();
  [super setNeedsDisplay];
}

- (BOOL)canDrawInCGLContext:(CGLContextObj)glContext
                pixelFormat:(CGLPixelFormatObj)pixelFormat
               forLayerTime:(CFTimeInterval)timeInterval
                displayTime:(const CVTimeStamp*)timeStamp {
  if (helper_)
    return helper_->CanDraw();
  return NO;
}

- (void)drawInCGLContext:(CGLContextObj)glContext
             pixelFormat:(CGLPixelFormatObj)pixelFormat
            forLayerTime:(CFTimeInterval)timeInterval
             displayTime:(const CVTimeStamp*)timeStamp {
  TRACE_EVENT0("browser", "IOSurfaceLayer::drawInCGLContext");

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
      context_, window_rect, window_scale_factor);

  if (helper_)
    helper_->DidDraw(draw_succeeded);

  [super drawInCGLContext:glContext
              pixelFormat:pixelFormat
             forLayerTime:timeInterval
              displayTime:timeStamp];
}

@end
