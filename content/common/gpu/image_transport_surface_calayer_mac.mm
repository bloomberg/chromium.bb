// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/gpu/image_transport_surface_calayer_mac.h"

#include <OpenGL/CGLRenderers.h>

#include "base/command_line.h"
#include "base/mac/sdk_forward_declarations.h"
#include "base/trace_event/trace_event.h"
#include "gpu/config/gpu_info_collector.h"
#include "ui/accelerated_widget_mac/surface_handle_types.h"
#include "ui/base/cocoa/animation_utils.h"
#include "ui/base/ui_base_switches.h"
#include "ui/gfx/geometry/dip_util.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/gl/gl_gl_api_implementation.h"
#include "ui/gl/gl_switches.h"
#include "ui/gl/gpu_switching_manager.h"

namespace {
const size_t kFramesToKeepCAContextAfterDiscard = 2;
const size_t kCanDrawFalsesBeforeSwitchFromAsync = 4;
const base::TimeDelta kMinDeltaToSwitchToAsync =
    base::TimeDelta::FromSecondsD(1. / 15.);

bool CanUseNSCGLSurface(const gpu::gles2::FeatureInfo* feature_info) {
  // Respect command line flags for the API's usage.
  static bool forced_at_command_line =
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kForceNSCGLSurfaceApi);
  if (forced_at_command_line)
    return true;
  static bool disabled_at_command_line =
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableNSCGLSurfaceApi);
  if (disabled_at_command_line)
    return false;

  // If there are multiple displays connected, then it is possible that we will
  // end up on the slow path, where -[NSCGLSurface layerContents] will return
  // a CGImage that is a dearly-made copy of the surface. Since we don't yet
  // know how to avoid those sharp edges, just avoid using NSCGLSurface when
  // multiple screens are present.
  uint32_t count = 0;
  CGError cg_error = CGGetActiveDisplayList(count, NULL, &count);
  if (cg_error != kCGErrorSuccess) {
    LOG(ERROR) << "Failed to query the number of displays.";
    return false;
  }
  if (count != 1)
    return false;

  // Systems with multiple GPUs can exhibit problems where incorrect content
  // will briefly flash during resize, and especially during transitions between
  // the iGPU and the dGPU. These problems are exhibited by layer-backed
  // NSOpenGLViews as well.
  if (feature_info->workarounds().disable_ns_cgl_surface_api)
    return false;

  // Leave this feature disabled until a flag for it is available.
  return false;
}

}  // namespace

// Private NSCGLSurface API.
@interface NSCGLSurface : NSObject
- (void)flushRect:(CGRect)rect;
- (void)attachToCGLContext:(CGLContextObj)cglContext;
- (id)initWithSize:(CGSize)size
        colorSpace:(CGColorSpaceRef)colorSpace
            atomic:(BOOL)atomic;
@property(readonly) CGImageRef image;
@property(readonly) id layerContents;
@end

// Private CALayer API.
@interface CALayer (Private)
- (void)setContentsChanged;
@end

@interface ImageTransportCAOpenGLLayer : CAOpenGLLayer <ImageTransportLayer> {
  content::CALayerStorageProvider* storageProvider_;
  base::Closure didDrawCallback_;

  // Used to determine if we should use setNeedsDisplay or setAsynchronous to
  // animate. If the last swap time happened very recently, then
  // setAsynchronous is used (which allows smooth animation, but comes with the
  // penalty of the canDrawInCGLContext function waking up the process every
  // vsync).
  base::TimeTicks lastSynchronousSwapTime_;

  // A counter that is incremented whenever LayerCanDraw returns false. If this
  // reaches a threshold, then |layer_| is switched to synchronous drawing to
  // save CPU work.
  uint32 canDrawReturnedFalseCount_;

  gfx::Size pixelSize_;
}

- (id)initWithStorageProvider:(content::CALayerStorageProvider*)storageProvider
                    pixelSize:(gfx::Size)pixelSize
                  scaleFactor:(float)scaleFactor;
- (void)drawNewFrame:(gfx::Rect)dirtyRect;
- (void)drawPendingFrameImmediately;
- (void)resetStorageProvider;
@end

@interface ImageTransportNSCGLSurface : CALayer <ImageTransportLayer> {
  content::CALayerStorageProvider* storageProvider_;
  base::ScopedTypeRef<CGLContextObj> cglContext_;
  base::scoped_nsobject<NSCGLSurface> surface_;
  gfx::Size pixelSize_;
}

- (id)initWithStorageProvider:(content::CALayerStorageProvider*)storageProvider
                    pixelSize:(gfx::Size)pixelSize
                  scaleFactor:(float)scaleFactor;
- (void)drawNewFrame:(gfx::Rect)dirtyRect;
- (void)drawPendingFrameImmediately;
- (void)resetStorageProvider;
@end

@implementation ImageTransportCAOpenGLLayer

- (id)initWithStorageProvider:
    (content::CALayerStorageProvider*)storageProvider
                    pixelSize:(gfx::Size)pixelSize
                  scaleFactor:(float)scaleFactor {
  if (self = [super init]) {
    gfx::Size dipSize = gfx::ConvertSizeToDIP(scaleFactor, pixelSize);
    [self setContentsScale:scaleFactor];
    [self setFrame:CGRectMake(0, 0, dipSize.width(), dipSize.height())];
    storageProvider_ = storageProvider;
    pixelSize_ = pixelSize;

    // -[CAOpenGLLayer drawInCGLContext] won't get called until we're in the
    // visible layer hierarchy, so call setLayer: immediately, to make this
    // happen.
    [storageProvider_->LayerCAContext() setLayer:self];
  }
  return self;
}

- (void)drawNewFrame:(gfx::Rect)dirtyRect {
  // This tracing would be more natural to do with a pseudo-thread for each
  // layer, rather than a counter.
  // http://crbug.com/366300
  // A trace value of 2 indicates that there is a pending swap ack. See
  // canDrawInCGLContext for other value meanings.
  TRACE_COUNTER_ID1("gpu", "CALayerPendingSwap", self, 2);

  if (![self isAsynchronous]) {
    // Switch to asynchronous drawing only if we get two frames in rapid
    // succession.
    base::TimeTicks this_swap_time = base::TimeTicks::Now();
    base::TimeDelta delta = this_swap_time - lastSynchronousSwapTime_;
    if (delta <= kMinDeltaToSwitchToAsync) {
      lastSynchronousSwapTime_ = base::TimeTicks();
      [self setAsynchronous:YES];
    } else {
      lastSynchronousSwapTime_ = this_swap_time;
      [self setNeedsDisplay];
    }
  }
}

- (void)drawPendingFrameImmediately {
  DCHECK(storageProvider_->LayerHasPendingDraw());
  if ([self isAsynchronous])
    [self setAsynchronous:NO];
  [self setNeedsDisplay];
  [self displayIfNeeded];
}

- (void)resetStorageProvider {
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
  didDrawCallback_ = storageProvider_->LayerShareGroupContextDirtiedCallback();
  return CGLRetainContext(storageProvider_->LayerShareGroupContext());
}

- (BOOL)canDrawInCGLContext:(CGLContextObj)glContext
                pixelFormat:(CGLPixelFormatObj)pixelFormat
               forLayerTime:(CFTimeInterval)timeInterval
                displayTime:(const CVTimeStamp*)timeStamp {
  TRACE_EVENT0("gpu", "CALayerStorageProvider::LayerCanDraw");

  if (!storageProvider_)
    return NO;

  if (storageProvider_->LayerHasPendingDraw()) {
    // If there is a draw pending then increase the signal from 2 to 3, to
    // indicate that there is a swap pending, and CoreAnimation has asked to
    // draw it.
    TRACE_COUNTER_ID1("gpu", "CALayerPendingSwap", self, 3);

    canDrawReturnedFalseCount_ = 0;
    return YES;
  } else {
    // If there is not a draw pending, then give an instantaneous blip up from
    // 0 to 1, indicating that CoreAnimation was ready to draw a frame but we
    // were not (or didn't have new content to draw).
    TRACE_COUNTER_ID1("gpu", "CALayerPendingSwap", self, 1);
    TRACE_COUNTER_ID1("gpu", "CALayerPendingSwap", self, 0);

    if ([self isAsynchronous]) {
      // If we are in asynchronous mode, we will be getting callbacks at every
      // vsync, asking us if we have anything to draw. If we get many of these
      // in a row, ask that we stop getting these callback for now, so that we
      // don't waste CPU cycles.
      if (canDrawReturnedFalseCount_ >= kCanDrawFalsesBeforeSwitchFromAsync)
        [self setAsynchronous:NO];
      else
        canDrawReturnedFalseCount_ += 1;
    }
    return NO;
  }
}

- (void)drawInCGLContext:(CGLContextObj)glContext
             pixelFormat:(CGLPixelFormatObj)pixelFormat
            forLayerTime:(CFTimeInterval)timeInterval
             displayTime:(const CVTimeStamp*)timeStamp {
  // While in this callback, CoreAnimation has set |glContext| to be current.
  // Ensure that the GL calls that we make are made against the native GL API.
  gfx::ScopedSetGLToRealGLApi scoped_set_gl_api;

  if (storageProvider_) {
    storageProvider_->LayerDoDraw(gfx::Rect(pixelSize_));

    // A trace value of 0 indicates that there is no longer a pending swap ack.
    TRACE_COUNTER_ID1("gpu", "CALayerPendingSwap", self, 0);
  } else {
    glClearColor(1, 1, 1, 1);
    glClear(GL_COLOR_BUFFER_BIT);
  }
  [super drawInCGLContext:glContext
              pixelFormat:pixelFormat
             forLayerTime:timeInterval
              displayTime:timeStamp];

  DCHECK(!didDrawCallback_.is_null());
  didDrawCallback_.Run();
}

@end

@implementation ImageTransportNSCGLSurface

- (id)initWithStorageProvider:
    (content::CALayerStorageProvider*)storageProvider
                    pixelSize:(gfx::Size)pixelSize
                  scaleFactor:(float)scaleFactor {
  if (self = [super init]) {
    ScopedCAActionDisabler disabler;
    gfx::Size dipSize = gfx::ConvertSizeToDIP(scaleFactor, pixelSize);
    [self setContentsScale:scaleFactor];
    [self setFrame:CGRectMake(0, 0, dipSize.width(), dipSize.height())];
    storageProvider_ = storageProvider;

    // Allocate the NSCGLSurface to render into.
    base::ScopedCFTypeRef<CGColorSpaceRef> cgColorSpace(
        CGDisplayCopyColorSpace(CGMainDisplayID()));
    Class NSCGLSurface_class = NSClassFromString(@"NSCGLSurface");
    surface_.reset([[NSCGLSurface_class alloc] initWithSize:pixelSize.ToCGSize()
                                                 colorSpace:cgColorSpace
                                                     atomic:NO]);

    // Create a context in the share group of the storage provider. We will
    // draw content using this context.
    CGLError cglError = kCGLNoError;
    cglError = CGLCreateContext(
        CGLGetPixelFormat(storageProvider_->LayerShareGroupContext()),
        storageProvider_->LayerShareGroupContext(),
        cglContext_.InitializeInto());
    LOG_IF(ERROR, cglError != kCGLNoError) <<
        "Failed to create CGL context for NSCGL surface.";

    pixelSize_ = pixelSize;
  }
  return self;
}

- (void)drawNewFrame:(gfx::Rect)dirtyRect {
  // Draw the first frame to the layer as covering the full layer. Subsequent
  // frames may use partial damage.
  if (![self contents])
    dirtyRect = gfx::Rect(pixelSize_);

  // Make the context current to the thread, make the surface be the current
  // drawable for the context, and draw.
  [surface_ attachToCGLContext:cglContext_];
  {
    gfx::ScopedCGLSetCurrentContext scopedSetCurrentContext(cglContext_);
    gfx::ScopedSetGLToRealGLApi scopedSetRealGLApi;
    glBindFramebufferEXT(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, pixelSize_.width(), pixelSize_.height());
    storageProvider_->LayerDoDraw(dirtyRect);
    glFlush();
  }
  [surface_ attachToCGLContext:NULL];
  [surface_ flushRect:dirtyRect.ToCGRect()];

  if (![self contents]) {
    // The first time we draw, set the layer contents and the CAContext's layer
    {
      ScopedCAActionDisabler disabler;
      [self setContents:[surface_ layerContents]];
    }
    [storageProvider_->LayerCAContext() setLayer:self];
  } else {
    // For subsequent draws, just indicate that the layer contents has changed.
    // This has lower power usage than calling -[CALayer setContents:].
    [self setContentsChanged];
  }
}

- (void)drawPendingFrameImmediately {
  [self drawNewFrame:gfx::Rect(pixelSize_)];
}

- (void)resetStorageProvider {
  storageProvider_ = NULL;
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
      fbo_texture_(0),
      fbo_scale_factor_(1),
      program_(0),
      vertex_shader_(0),
      fragment_shader_(0),
      position_location_(0),
      tex_location_(0),
      vertex_buffer_(0),
      vertex_array_(0),
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
    CGLContextObj context, const base::Closure& context_dirtied_callback,
    GLuint texture, gfx::Size pixel_size, float scale_factor) {
  // Allocate an ordinary OpenGL texture to back the FBO.
  GLenum error;
  while ((error = glGetError()) != GL_NO_ERROR) {
    LOG(ERROR) << "OpenGL error hit but ignored before allocating buffer "
               << "storage: " << error;
  }

  if (gfx::GetGLImplementation() ==
      gfx::kGLImplementationDesktopGLCoreProfile) {
    glTexImage2D(GL_TEXTURE_2D,
                 0,
                 GL_RGBA,
                 pixel_size.width(),
                 pixel_size.height(),
                 0,
                 GL_RGBA,
                 GL_UNSIGNED_BYTE,
                 NULL);
    glFlush();

    if (!vertex_shader_) {
      const char* source =
          "#version 150\n"
          "in vec4 position;\n"
          "out vec2 texcoord;\n"
          "void main() {\n"
          "    texcoord = vec2(position.x, position.y);\n"
          "    gl_Position = vec4(2*position.x-1, 2*position.y-1,\n"
          "        position.z, position.w);\n"
          "}\n";
      vertex_shader_ = glCreateShader(GL_VERTEX_SHADER);
      glShaderSource(vertex_shader_, 1, &source, NULL);
      glCompileShader(vertex_shader_);
#if DCHECK_IS_ON()
      GLint status = GL_FALSE;
      glGetShaderiv(vertex_shader_, GL_COMPILE_STATUS, &status);
      DCHECK(status == GL_TRUE);
#endif
    }
    if (!fragment_shader_) {
      const char* source =
          "#version 150\n"
          "uniform sampler2D tex;\n"
          "in vec2 texcoord;\n"
          "out vec4 frag_color;\n"
          "void main() {\n"
          "    frag_color = texture(tex, texcoord);\n"
          "}\n";
      fragment_shader_ = glCreateShader(GL_FRAGMENT_SHADER);
      glShaderSource(fragment_shader_, 1, &source, NULL);
      glCompileShader(fragment_shader_);
#if DCHECK_IS_ON()
      GLint status = GL_FALSE;
      glGetShaderiv(fragment_shader_, GL_COMPILE_STATUS, &status);
      DCHECK(status == GL_TRUE);
#endif
    }
    if (!program_) {
      program_ = glCreateProgram();
      glAttachShader(program_, vertex_shader_);
      glAttachShader(program_, fragment_shader_);
      glBindFragDataLocation(program_, 0, "frag_color");
      glLinkProgram(program_);
#if DCHECK_IS_ON()
      GLint status = GL_FALSE;
      glGetProgramiv(program_, GL_LINK_STATUS, &status);
      DCHECK(status == GL_TRUE);
#endif
      position_location_ = glGetAttribLocation(program_, "position");
      tex_location_ = glGetUniformLocation(program_, "tex");
    }
    if (!vertex_buffer_) {
      GLfloat vertex_data[24] = {
        0, 0, 0, 1,
        1, 0, 0, 1,
        1, 1, 0, 1,
        1, 1, 0, 1,
        0, 1, 0, 1,
        0, 0, 0, 1,
      };
      glGenBuffersARB(1, &vertex_buffer_);
      // If the allocation path used GLContext::RestoreStateIfDirtiedExternally
      // as the draw path does, this manual state restoration would not be
      // necessary.
      GLint bound_buffer = 0;
      glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &bound_buffer);
      glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer_);
      glBufferData(GL_ARRAY_BUFFER, sizeof(vertex_data),
                   vertex_data, GL_STATIC_DRAW);
      glBindBuffer(GL_ARRAY_BUFFER, bound_buffer);
    }
    if (!vertex_array_) {
      // If the allocation path used GLContext::RestoreStateIfDirtiedExternally
      // as the draw path does, this manual state restoration would not be
      // necessary.
      GLint bound_vao = 0;
      glGetIntegerv(GL_VERTEX_ARRAY_BINDING_OES, &bound_vao);
      glGenVertexArraysOES(1, &vertex_array_);
      glBindVertexArrayOES(vertex_array_);
      {
        glEnableVertexAttribArray(position_location_);
        GLint bound_buffer = 0;
        glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &bound_buffer);
        glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer_);
        glVertexAttribPointer(position_location_, 4, GL_FLOAT, GL_FALSE, 0, 0);
        glBindBuffer(GL_ARRAY_BUFFER, bound_buffer);
      }
      glBindVertexArrayOES(bound_vao);
    }
  } else {
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
  }

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
  share_group_context_dirtied_callback_ = context_dirtied_callback;
  fbo_texture_ = texture;
  fbo_pixel_size_ = pixel_size;
  fbo_scale_factor_ = scale_factor;
  return true;
}

void CALayerStorageProvider::FreeColorBufferStorage() {
  if (gfx::GetGLImplementation() ==
      gfx::kGLImplementationDesktopGLCoreProfile) {
    if (vertex_shader_)
      glDeleteShader(vertex_shader_);
    if (fragment_shader_)
      glDeleteShader(fragment_shader_);
    if (program_)
      glDeleteProgram(program_);
    if (vertex_buffer_)
      glDeleteBuffersARB(1, &vertex_buffer_);
    if (vertex_array_)
      glDeleteVertexArraysOES(1, &vertex_array_);
    vertex_shader_ = 0;
    fragment_shader_ = 0;
    program_ = 0;
    vertex_buffer_ = 0;
    vertex_array_ = 0;
  }

  // Note that |context_| still holds a reference to |layer_|, and will until
  // a new frame is swapped in.
  ResetLayer();

  share_group_context_.reset();
  share_group_context_dirtied_callback_ = base::Closure();
  fbo_texture_ = 0;
  fbo_pixel_size_ = gfx::Size();
}

void CALayerStorageProvider::FrameSizeChanged(const gfx::Size& pixel_size,
                                              float scale_factor) {
  DCHECK_EQ(fbo_pixel_size_.ToString(), pixel_size.ToString());
  DCHECK_EQ(fbo_scale_factor_, scale_factor);
}

void CALayerStorageProvider::SwapBuffers(const gfx::Rect& dirty_rect) {
  TRACE_EVENT0("gpu", "CALayerStorageProvider::SwapBuffers");
  DCHECK(!has_pending_draw_);

  // Recreate the CALayer on the new GPU if a GPU switch has occurred. Note
  // that the CAContext will retain a reference to the old CALayer until the
  // call to -[CAContext setLayer:] replaces the old CALayer with the new one.
  if (recreate_layer_after_gpu_switch_) {
    ResetLayer();
    recreate_layer_after_gpu_switch_ = false;
  }

  // Determine if it is safe to use an NSCGLSurface, or if we should use the
  // CAOpenGLLayer fallback. If we're not using the preferred type of layer,
  // then reset the layer and re-create one of the preferred type.
  bool can_use_ns_cgl_surface =
      CanUseNSCGLSurface(transport_surface_->GetFeatureInfo());
  Class expected_layer_class = can_use_ns_cgl_surface ?
      [ImageTransportNSCGLSurface class] : [ImageTransportCAOpenGLLayer class];
  if (![layer_ isKindOfClass:expected_layer_class])
    ResetLayer();

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
    if (can_use_ns_cgl_surface) {
      layer_.reset([[ImageTransportNSCGLSurface alloc]
          initWithStorageProvider:this
                        pixelSize:fbo_pixel_size_
                      scaleFactor:fbo_scale_factor_]);
    } else {
      layer_.reset([[ImageTransportCAOpenGLLayer alloc]
          initWithStorageProvider:this
                        pixelSize:fbo_pixel_size_
                      scaleFactor:fbo_scale_factor_]);
    }
  }

  // Replacing the CAContext's CALayer will sometimes results in an immediate
  // draw.
  if (!has_pending_draw_)
    return;

  // Tell CoreAnimation to draw our frame.
  if (gpu_vsync_disabled_ || throttling_disabled_) {
    DrawImmediatelyAndUnblockBrowser();
  } else {
    [layer_ drawNewFrame:dirty_rect];
  }

  if (has_pending_draw_) {
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
  [layer_ drawPendingFrameImmediately];

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
  ResetLayer();

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
  TRACE_EVENT0("gpu", "CALayerStorageProvider::SwapBuffersAckedByBrowser");
  throttling_disabled_ = disable_throttling;
  if (!previously_discarded_contexts_.empty())
    previously_discarded_contexts_.pop_front();
}

CGLContextObj CALayerStorageProvider::LayerShareGroupContext() {
  return share_group_context_;
}

base::Closure CALayerStorageProvider::LayerShareGroupContextDirtiedCallback() {
  return share_group_context_dirtied_callback_;
}

void CALayerStorageProvider::LayerDoDraw(const gfx::Rect& dirty_rect) {
  TRACE_EVENT0("gpu", "CALayerStorageProvider::LayerDoDraw");
  if (gfx::GetGLImplementation() ==
      gfx::kGLImplementationDesktopGLCoreProfile) {
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glClearColor(1, 0, 1, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    glDisable(GL_BLEND);
    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_STENCIL_TEST);
    glDisable(GL_SCISSOR_TEST);

    DCHECK(glIsProgram(program_));
    glUseProgram(program_);
    glBindVertexArrayOES(vertex_array_);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, fbo_texture_);
    glUniform1i(tex_location_, 0);

    glDisable(GL_CULL_FACE);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArrayOES(0);
    glUseProgram(0);
  } else {
    GLint viewport[4] = {0, 0, 0, 0};
    glGetIntegerv(GL_VIEWPORT, viewport);
    gfx::Size viewport_size(viewport[2], viewport[3]);

    // Set the coordinate system to be one-to-one with pixels.
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, viewport_size.width(), 0, viewport_size.height(), -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    // Reset drawing state and draw a fullscreen quad.
    glUseProgram(0);
    glDisable(GL_BLEND);
    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_STENCIL_TEST);
    glDisable(GL_SCISSOR_TEST);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glColor4f(1, 1, 1, 1);
    glActiveTexture(GL_TEXTURE0);
    glEnable(GL_TEXTURE_RECTANGLE_ARB);
    glBindTexture(GL_TEXTURE_RECTANGLE_ARB, fbo_texture_);
    glBegin(GL_QUADS);
    {
      glTexCoord2f(dirty_rect.x(), dirty_rect.y());
      glVertex2f(dirty_rect.x(), dirty_rect.y());

      glTexCoord2f(dirty_rect.x(), dirty_rect.bottom());
      glVertex2f(dirty_rect.x(), dirty_rect.bottom());

      glTexCoord2f(dirty_rect.right(), dirty_rect.bottom());
      glVertex2f(dirty_rect.right(), dirty_rect.bottom());

      glTexCoord2f(dirty_rect.right(), dirty_rect.y());
      glVertex2f(dirty_rect.right(), dirty_rect.y());
    }
    glEnd();
    glBindTexture(GL_TEXTURE_RECTANGLE_ARB, 0);
    glDisable(GL_TEXTURE_RECTANGLE_ARB);
  }

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

bool CALayerStorageProvider::LayerHasPendingDraw() const {
  return has_pending_draw_;
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

void CALayerStorageProvider::ResetLayer() {
  [layer_ resetStorageProvider];

  // If we are providing back-pressure by waiting for a draw, that draw will
  // now never come, so release the pressure now.
  UnblockBrowserIfNeeded();

  // This should only ever be called by the active layer.
  layer_.reset();
}

}  //  namespace content
