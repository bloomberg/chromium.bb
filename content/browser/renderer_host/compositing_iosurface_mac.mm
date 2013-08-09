// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/compositing_iosurface_mac.h"

#include <OpenGL/CGLRenderers.h>
#include <OpenGL/OpenGL.h>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/debug/trace_event.h"
#include "base/logging.h"
#include "base/mac/mac_util.h"
#include "base/message_loop/message_loop.h"
#include "base/threading/platform_thread.h"
#include "content/browser/renderer_host/compositing_iosurface_context_mac.h"
#include "content/browser/renderer_host/compositing_iosurface_shader_programs_mac.h"
#include "content/browser/renderer_host/compositing_iosurface_transformer_mac.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_view_mac.h"
#include "content/common/content_constants_internal.h"
#include "content/port/browser/render_widget_host_view_frame_subscriber.h"
#include "gpu/command_buffer/service/gpu_switches.h"
#include "media/base/video_util.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/scoped_ns_graphics_context_save_gstate_mac.h"
#include "ui/gfx/size_conversions.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/io_surface_support_mac.h"

#ifdef NDEBUG
#define CHECK_GL_ERROR()
#define CHECK_AND_SAVE_GL_ERROR()
#else
#define CHECK_GL_ERROR() do {                                           \
    GLenum gl_error = glGetError();                                     \
    LOG_IF(ERROR, gl_error != GL_NO_ERROR) << "GL Error :" << gl_error; \
  } while (0)
#define CHECK_AND_SAVE_GL_ERROR() do {                                  \
    GLenum gl_error = GetAndSaveGLError();                              \
    LOG_IF(ERROR, gl_error != GL_NO_ERROR) << "GL Error :" << gl_error; \
  } while (0)
#endif

namespace content {
namespace {

// How many times to test if asynchronous copy has completed.
// This value is chosen such that we allow at most 1 second to finish a copy.
const int kFinishCopyRetryCycles = 100;

// Time in milliseconds to allow asynchronous copy to finish.
// This value is shorter than 16ms such that copy can complete within a vsync.
const int kFinishCopyPollingPeriodMs = 10;

bool HasAppleFenceExtension() {
  static bool initialized_has_fence = false;
  static bool has_fence = false;

  if (!initialized_has_fence) {
    has_fence =
        strstr(reinterpret_cast<const char*>(glGetString(GL_EXTENSIONS)),
               "GL_APPLE_fence") != NULL;
    initialized_has_fence = true;
  }
  return has_fence;
}

bool HasPixelBufferObjectExtension() {
  static bool initialized_has_pbo = false;
  static bool has_pbo = false;

  if (!initialized_has_pbo) {
    has_pbo =
        strstr(reinterpret_cast<const char*>(glGetString(GL_EXTENSIONS)),
               "GL_ARB_pixel_buffer_object") != NULL;
    initialized_has_pbo = true;
  }
  return has_pbo;
}

// Helper function to reverse the argument order.  Also takes ownership of
// |bitmap_output| for the life of the binding.
void ReverseArgumentOrder(
    const base::Callback<void(bool, const SkBitmap&)>& callback,
    scoped_ptr<SkBitmap> bitmap_output, bool success) {
  callback.Run(success, *bitmap_output);
}

// Called during an async GPU readback with a pointer to the pixel buffer.  In
// the snapshot path, we just memcpy the data into our output bitmap since the
// width, height, and stride should all be equal.
bool MapBufferToSkBitmap(const SkBitmap* output, const void* buf, int ignored) {
  TRACE_EVENT0("browser", "MapBufferToSkBitmap");

  if (buf) {
    SkAutoLockPixels output_lock(*output);
    memcpy(output->getPixels(), buf, output->getSize());
  }
  return buf != NULL;
}

// Copies tightly-packed scanlines from |buf| to |region_in_frame| in the given
// |target| VideoFrame's |plane|.  Assumption: |buf|'s width is
// |region_in_frame.width()| and its stride is always in 4-byte alignment.
//
// TODO(miu): Refactor by moving this function into media/video_util.
// http://crbug.com/219779
bool MapBufferToVideoFrame(
    const scoped_refptr<media::VideoFrame>& target,
    const gfx::Rect& region_in_frame,
    const void* buf,
    int plane) {
  COMPILE_ASSERT(media::VideoFrame::kYPlane == 0, VideoFrame_kYPlane_mismatch);
  COMPILE_ASSERT(media::VideoFrame::kUPlane == 1, VideoFrame_kUPlane_mismatch);
  COMPILE_ASSERT(media::VideoFrame::kVPlane == 2, VideoFrame_kVPlane_mismatch);

  TRACE_EVENT1("browser", "MapBufferToVideoFrame", "plane", plane);

  // Apply black-out in the regions surrounding the view area (for
  // letterboxing/pillarboxing).  Only do this once, since this is performed on
  // all planes in the VideoFrame here.
  if (plane == 0)
    media::LetterboxYUV(target.get(), region_in_frame);

  if (buf) {
    int packed_width = region_in_frame.width();
    int packed_height = region_in_frame.height();
    // For planes 1 and 2, the width and height are 1/2 size (rounded up).
    if (plane > 0) {
      packed_width = (packed_width + 1) / 2;
      packed_height = (packed_height + 1) / 2;
    }
    const uint8* src = reinterpret_cast<const uint8*>(buf);
    const int src_stride = (packed_width % 4 == 0 ?
                                packed_width :
                                (packed_width + 4 - (packed_width % 4)));
    const uint8* const src_end = src + packed_height * src_stride;

    // Calculate starting offset and stride into the destination buffer.
    const int dst_stride = target->stride(plane);
    uint8* dst = target->data(plane);
    if (plane == 0)
      dst += (region_in_frame.y() * dst_stride) + region_in_frame.x();
    else
      dst += (region_in_frame.y() / 2 * dst_stride) + (region_in_frame.x() / 2);

    // Copy each row, accounting for strides in the source and destination.
    for (; src < src_end; src += src_stride, dst += dst_stride)
      memcpy(dst, src, packed_width);
  }
  return buf != NULL;
}

}  // namespace

CVReturn DisplayLinkCallback(CVDisplayLinkRef display_link,
                             const CVTimeStamp* now,
                             const CVTimeStamp* output_time,
                             CVOptionFlags flags_in,
                             CVOptionFlags* flags_out,
                             void* context) {
  CompositingIOSurfaceMac* surface =
      static_cast<CompositingIOSurfaceMac*>(context);
  surface->DisplayLinkTick(display_link, output_time);
  return kCVReturnSuccess;
}

CompositingIOSurfaceMac::CopyContext::CopyContext(
    const scoped_refptr<CompositingIOSurfaceContext>& context)
  : transformer(new CompositingIOSurfaceTransformer(
        GL_TEXTURE_RECTANGLE_ARB, true, context->shader_program_cache())),
    output_readback_format(GL_BGRA),
    num_outputs(0),
    fence(0),
    cycles_elapsed(0) {
  memset(output_textures, 0, sizeof(output_textures));
  memset(frame_buffers, 0, sizeof(frame_buffers));
  memset(pixel_buffers, 0, sizeof(pixel_buffers));
}

CompositingIOSurfaceMac::CopyContext::~CopyContext() {
  DCHECK_EQ(frame_buffers[0], 0u) << "Failed to call ReleaseCachedGLObjects().";
}

void CompositingIOSurfaceMac::CopyContext::ReleaseCachedGLObjects() {
  // No outstanding callbacks should be pending.
  DCHECK(map_buffer_callback.is_null());
  DCHECK(done_callback.is_null());

  // For an asynchronous read-back, there are more objects to delete:
  if (fence) {
    glDeleteBuffers(arraysize(pixel_buffers), pixel_buffers); CHECK_GL_ERROR();
    memset(pixel_buffers, 0, sizeof(pixel_buffers));
    glDeleteFencesAPPLE(1, &fence); CHECK_GL_ERROR();
    fence = 0;
  }

  glDeleteFramebuffersEXT(arraysize(frame_buffers), frame_buffers);
  CHECK_GL_ERROR();
  memset(frame_buffers, 0, sizeof(frame_buffers));

  // Note: |output_textures| are owned by the transformer.
  if (transformer)
    transformer->ReleaseCachedGLObjects();
}

void CompositingIOSurfaceMac::CopyContext::PrepareReadbackFramebuffers() {
  for (int i = 0; i < num_outputs; ++i) {
    if (!frame_buffers[i]) {
      glGenFramebuffersEXT(1, &frame_buffers[i]); CHECK_GL_ERROR();
    }
  }
}

void CompositingIOSurfaceMac::CopyContext::PrepareForAsynchronousReadback() {
  PrepareReadbackFramebuffers();
  if (!fence) {
    glGenFencesAPPLE(1, &fence); CHECK_GL_ERROR();
  }
  for (int i = 0; i < num_outputs; ++i) {
    if (!pixel_buffers[i]) {
      glGenBuffersARB(1, &pixel_buffers[i]); CHECK_GL_ERROR();
    }
  }
}


// static
CompositingIOSurfaceMac* CompositingIOSurfaceMac::Create(
    const scoped_refptr<CompositingIOSurfaceContext>& context) {
  IOSurfaceSupport* io_surface_support = IOSurfaceSupport::Initialize();
  if (!io_surface_support) {
    LOG(ERROR) << "No IOSurface support";
    return NULL;
  }

  return new CompositingIOSurfaceMac(io_surface_support,
                                     context);
}

CompositingIOSurfaceMac::CompositingIOSurfaceMac(
    IOSurfaceSupport* io_surface_support,
    const scoped_refptr<CompositingIOSurfaceContext>& context)
    : io_surface_support_(io_surface_support),
      context_(context),
      io_surface_handle_(0),
      scale_factor_(1.f),
      texture_(0),
      finish_copy_timer_(
          FROM_HERE,
          base::TimeDelta::FromMilliseconds(kFinishCopyPollingPeriodMs),
          base::Bind(&CompositingIOSurfaceMac::CheckIfAllCopiesAreFinished,
                     base::Unretained(this),
                     false),
          true),
      display_link_(0),
      display_link_stop_timer_(FROM_HERE, base::TimeDelta::FromSeconds(1),
                               this, &CompositingIOSurfaceMac::StopDisplayLink),
      vsync_interval_numerator_(0),
      vsync_interval_denominator_(0),
      initialized_is_intel_(false),
      is_intel_(false),
      screen_(0),
      gl_error_(GL_NO_ERROR) {
  CHECK(context_);
}

void CompositingIOSurfaceMac::SetupCVDisplayLink() {
  if (display_link_) {
    LOG(ERROR) << "DisplayLink already setup";
    return;
  }

  CVDisplayLinkRef display_link;
  CVReturn ret = CVDisplayLinkCreateWithActiveCGDisplays(&display_link);
  if (ret != kCVReturnSuccess) {
    LOG(WARNING) << "CVDisplayLinkCreateWithActiveCGDisplays failed: " << ret;
    return;
  }

  display_link_ = display_link;

  ret = CVDisplayLinkSetOutputCallback(display_link_,
                                       &DisplayLinkCallback, this);
  DCHECK(ret == kCVReturnSuccess)
      << "CVDisplayLinkSetOutputCallback failed: " << ret;

  StartOrContinueDisplayLink();

  CVTimeStamp cv_time;
  ret = CVDisplayLinkGetCurrentTime(display_link_, &cv_time);
  DCHECK(ret == kCVReturnSuccess)
      << "CVDisplayLinkGetCurrentTime failed: " << ret;

  {
    base::AutoLock lock(lock_);
    CalculateVsyncParametersLockHeld(&cv_time);
  }

  // Stop display link for now, it will be started when needed during Draw.
  StopDisplayLink();
}

void CompositingIOSurfaceMac::SetContext(
    const scoped_refptr<CompositingIOSurfaceContext>& new_context) {
  CHECK(new_context);

  if (context_ == new_context)
    return;

  // Asynchronous copies must complete in the same context they started in.
  CheckIfAllCopiesAreFinished(true);
  CGLSetCurrentContext(context_->cgl_context());
  DestroyAllCopyContextsWithinContext();
  CGLSetCurrentContext(0);

  context_ = new_context;
}

bool CompositingIOSurfaceMac::is_vsync_disabled() const {
  return context_->is_vsync_disabled();
}

void CompositingIOSurfaceMac::GetVSyncParameters(base::TimeTicks* timebase,
                                                 uint32* interval_numerator,
                                                 uint32* interval_denominator) {
  base::AutoLock lock(lock_);
  *timebase = vsync_timebase_;
  *interval_numerator = vsync_interval_numerator_;
  *interval_denominator = vsync_interval_denominator_;
}

CompositingIOSurfaceMac::~CompositingIOSurfaceMac() {
  FailAllCopies();
  CVDisplayLinkRelease(display_link_);
  CGLSetCurrentContext(context_->cgl_context());
  DestroyAllCopyContextsWithinContext();
  UnrefIOSurfaceWithContextCurrent();
  CGLSetCurrentContext(0);
  context_ = NULL;
}

bool CompositingIOSurfaceMac::SetIOSurface(
    uint64 io_surface_handle,
    const gfx::Size& size,
    float scale_factor,
    const ui::LatencyInfo& latency_info) {
  pixel_io_surface_size_ = size;
  scale_factor_ = scale_factor;
  dip_io_surface_size_ = gfx::ToFlooredSize(
      gfx::ScaleSize(pixel_io_surface_size_, 1.0 / scale_factor_));

  CGLError cgl_error = CGLSetCurrentContext(context_->cgl_context());
  if (cgl_error != kCGLNoError) {
    LOG(ERROR) << "CGLSetCurrentContext error in SetIOSurface: " << cgl_error;
    return false;
  }
  bool result = MapIOSurfaceToTexture(io_surface_handle);
  CGLSetCurrentContext(0);
  latency_info_.MergeWith(latency_info);
  return result;
}

int CompositingIOSurfaceMac::GetRendererID() {
  GLint current_renderer_id = -1;
  if (CGLGetParameter(context_->cgl_context(),
                      kCGLCPCurrentRendererID,
                      &current_renderer_id) == kCGLNoError)
    return current_renderer_id & kCGLRendererIDMatchingMask;
  return -1;
}

bool CompositingIOSurfaceMac::DrawIOSurface(
    const gfx::Size& window_size,
    float window_scale_factor,
    RenderWidgetHostViewFrameSubscriber* frame_subscriber,
    bool using_core_animation) {
  bool result = true;

  if (display_link_ == NULL)
    SetupCVDisplayLink();

  bool has_io_surface = HasIOSurface();
  TRACE_EVENT1("browser", "CompositingIOSurfaceMac::DrawIOSurface",
               "has_io_surface", has_io_surface);

  gfx::Size pixel_window_size = gfx::ToFlooredSize(
      gfx::ScaleSize(window_size, window_scale_factor));
  glViewport(0, 0, pixel_window_size.width(), pixel_window_size.height());

  SurfaceQuad quad;
  quad.set_size(dip_io_surface_size_, pixel_io_surface_size_);

  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();

  // Note that the projection keeps things in view units, so the use of
  // window_size / dip_io_surface_size_ (as opposed to the pixel_ variants)
  // below is correct.
  glOrtho(0, window_size.width(), window_size.height(), 0, -1, 1);
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();

  glDisable(GL_DEPTH_TEST);
  glDisable(GL_BLEND);

  if (has_io_surface) {
    context_->shader_program_cache()->UseBlitProgram();
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_RECTANGLE_ARB, texture_);

    DrawQuad(quad);

    glBindTexture(GL_TEXTURE_RECTANGLE_ARB, 0); CHECK_AND_SAVE_GL_ERROR();

    // Fill the resize gutters with white.
    if (window_size.width() > dip_io_surface_size_.width() ||
        window_size.height() > dip_io_surface_size_.height()) {
      context_->shader_program_cache()->UseSolidWhiteProgram();
      SurfaceQuad filler_quad;
      if (window_size.width() > dip_io_surface_size_.width()) {
        // Draw right-side gutter down to the bottom of the window.
        filler_quad.set_rect(dip_io_surface_size_.width(), 0.0f,
                             window_size.width(), window_size.height());
        DrawQuad(filler_quad);
      }
      if (window_size.height() > dip_io_surface_size_.height()) {
        // Draw bottom gutter to the width of the IOSurface.
        filler_quad.set_rect(
            0.0f, dip_io_surface_size_.height(),
            dip_io_surface_size_.width(), window_size.height());
        DrawQuad(filler_quad);
      }
    }

    // Workaround for issue 158469. Issue a dummy draw call with texture_ not
    // bound to blit_rgb_sampler_location_, in order to shake all references
    // to the IOSurface out of the driver.
    glBegin(GL_TRIANGLES);
    glEnd();

    glUseProgram(0); CHECK_AND_SAVE_GL_ERROR();
  } else {
    // Should match the clear color of RenderWidgetHostViewMac.
    glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
  }

  static bool initialized_workaround = false;
  static bool force_on_workaround = false;
  static bool force_off_workaround = false;
  if (!initialized_workaround) {
    force_on_workaround = CommandLine::ForCurrentProcess()->HasSwitch(
        switches::kForceGLFinishWorkaround);
    force_off_workaround = CommandLine::ForCurrentProcess()->HasSwitch(
        switches::kDisableGpuDriverBugWorkarounds);

    initialized_workaround = true;
  }

  const bool workaround_needed =
      IsVendorIntel() && !base::mac::IsOSMountainLionOrLater();
  const bool use_glfinish_workaround =
      (workaround_needed || force_on_workaround) && !force_off_workaround;

  if (use_glfinish_workaround) {
    TRACE_EVENT0("gpu", "glFinish");
    // http://crbug.com/123409 : work around bugs in graphics driver on
    // MacBook Air with Intel HD graphics, and possibly on other models,
    // by forcing the graphics pipeline to be completely drained at this
    // point.
    // This workaround is not necessary on Mountain Lion.
    glFinish();
  }

  base::Closure copy_done_callback;
  if (frame_subscriber) {
    const base::Time present_time = base::Time::Now();
    scoped_refptr<media::VideoFrame> frame;
    RenderWidgetHostViewFrameSubscriber::DeliverFrameCallback callback;
    if (frame_subscriber->ShouldCaptureFrame(present_time, &frame, &callback)) {
      copy_done_callback = CopyToVideoFrameWithinContext(
          gfx::Rect(pixel_io_surface_size_), true, frame,
          base::Bind(callback, present_time));
    }
  }

  if (!using_core_animation) {
    CGLError cgl_error =  CGLFlushDrawable(context_->cgl_context());
    if (cgl_error != kCGLNoError) {
      LOG(ERROR) << "CGLFlushDrawable error in DrawIOSurface: " << cgl_error;
      result = false;
    }
  }

  latency_info_.swap_timestamp = base::TimeTicks::HighResNow();
  RenderWidgetHostImpl::CompositorFrameDrawn(latency_info_);
  latency_info_.Clear();

  // Try to finish previous copy requests after flush to get better pipelining.
  std::vector<base::Closure> copy_done_callbacks;
  CheckIfAllCopiesAreFinishedWithinContext(false, &copy_done_callbacks);

  // Check if any of the drawing calls result in an error.
  GetAndSaveGLError();
  if (gl_error_ != GL_NO_ERROR) {
    LOG(ERROR) << "GL error in DrawIOSurface: " << gl_error_;
    result = false;
  }

  if (!using_core_animation)
    CGLSetCurrentContext(0);

  if (!copy_done_callback.is_null())
    copy_done_callbacks.push_back(copy_done_callback);
  for (size_t i = 0; i < copy_done_callbacks.size(); ++i)
    copy_done_callbacks[i].Run();

  StartOrContinueDisplayLink();

  return result;
}

void CompositingIOSurfaceMac::CopyTo(
      const gfx::Rect& src_pixel_subrect,
      const gfx::Size& dst_pixel_size,
      const base::Callback<void(bool, const SkBitmap&)>& callback) {
  scoped_ptr<SkBitmap> output(new SkBitmap());
  output->setConfig(SkBitmap::kARGB_8888_Config,
                    dst_pixel_size.width(), dst_pixel_size.height());
  if (!output->allocPixels()) {
    DLOG(ERROR) << "Failed to allocate SkBitmap pixels!";
    callback.Run(false, *output);
    return;
  }
  DCHECK_EQ(output->rowBytesAsPixels(), dst_pixel_size.width())
      << "Stride is required to be equal to width for GPU readback.";
  output->setIsOpaque(true);

  CGLSetCurrentContext(context_->cgl_context());
  const base::Closure copy_done_callback = CopyToSelectedOutputWithinContext(
      src_pixel_subrect, gfx::Rect(dst_pixel_size), false,
      output.get(), NULL,
      base::Bind(&ReverseArgumentOrder, callback, base::Passed(&output)));
  CGLSetCurrentContext(0);
  if (!copy_done_callback.is_null())
    copy_done_callback.Run();
}

void CompositingIOSurfaceMac::CopyToVideoFrame(
    const gfx::Rect& src_pixel_subrect,
    const scoped_refptr<media::VideoFrame>& target,
    const base::Callback<void(bool)>& callback) {
  CGLSetCurrentContext(context_->cgl_context());
  const base::Closure copy_done_callback = CopyToVideoFrameWithinContext(
      src_pixel_subrect, false, target, callback);
  CGLSetCurrentContext(0);
  if (!copy_done_callback.is_null())
    copy_done_callback.Run();
}

base::Closure CompositingIOSurfaceMac::CopyToVideoFrameWithinContext(
    const gfx::Rect& src_pixel_subrect,
    bool called_within_draw,
    const scoped_refptr<media::VideoFrame>& target,
    const base::Callback<void(bool)>& callback) {
  gfx::Rect region_in_frame = media::ComputeLetterboxRegion(
      gfx::Rect(target->coded_size()), src_pixel_subrect.size());
  // Make coordinates and sizes even because we letterbox in YUV space right
  // now (see CopyRGBToVideoFrame). They need to be even for the UV samples to
  // line up correctly.
  region_in_frame = gfx::Rect(region_in_frame.x() & ~1,
                              region_in_frame.y() & ~1,
                              region_in_frame.width() & ~1,
                              region_in_frame.height() & ~1);
  DCHECK_LE(region_in_frame.right(), target->coded_size().width());
  DCHECK_LE(region_in_frame.bottom(), target->coded_size().height());

  return CopyToSelectedOutputWithinContext(
      src_pixel_subrect, region_in_frame, called_within_draw,
      NULL, target, callback);
}

bool CompositingIOSurfaceMac::MapIOSurfaceToTexture(
    uint64 io_surface_handle) {
  if (io_surface_.get() && io_surface_handle == io_surface_handle_)
    return true;

  TRACE_EVENT0("browser", "CompositingIOSurfaceMac::MapIOSurfaceToTexture");
  UnrefIOSurfaceWithContextCurrent();

  io_surface_.reset(io_surface_support_->IOSurfaceLookup(
      static_cast<uint32>(io_surface_handle)));
  // Can fail if IOSurface with that ID was already released by the gpu
  // process.
  if (!io_surface_) {
    UnrefIOSurfaceWithContextCurrent();
    return false;
  }

  io_surface_handle_ = io_surface_handle;

  // Actual IOSurface size is rounded up to reduce reallocations during window
  // resize. Get the actual size to properly map the texture.
  gfx::Size rounded_size(
      io_surface_support_->IOSurfaceGetWidth(io_surface_),
      io_surface_support_->IOSurfaceGetHeight(io_surface_));

  glGenTextures(1, &texture_);
  glBindTexture(GL_TEXTURE_RECTANGLE_ARB, texture_);
  glTexParameterf(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameterf(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  CHECK_AND_SAVE_GL_ERROR();
  GLuint plane = 0;
  CGLError cgl_error = io_surface_support_->CGLTexImageIOSurface2D(
      context_->cgl_context(),
      GL_TEXTURE_RECTANGLE_ARB,
      GL_RGBA,
      rounded_size.width(),
      rounded_size.height(),
      GL_BGRA,
      GL_UNSIGNED_INT_8_8_8_8_REV,
      io_surface_.get(),
      plane);
  glBindTexture(GL_TEXTURE_RECTANGLE_ARB, 0);
  if (cgl_error != kCGLNoError) {
    LOG(ERROR) << "CGLTexImageIOSurface2D: " << cgl_error;
    UnrefIOSurfaceWithContextCurrent();
    return false;
  }
  GetAndSaveGLError();
  if (gl_error_ != GL_NO_ERROR) {
    LOG(ERROR) << "GL error in MapIOSurfaceToTexture: " << gl_error_;
    UnrefIOSurfaceWithContextCurrent();
    return false;
  }
  return true;
}

void CompositingIOSurfaceMac::UnrefIOSurface() {
  CGLSetCurrentContext(context_->cgl_context());
  UnrefIOSurfaceWithContextCurrent();
  CGLSetCurrentContext(0);
}

void CompositingIOSurfaceMac::DrawQuad(const SurfaceQuad& quad) {
  glEnableClientState(GL_VERTEX_ARRAY); CHECK_AND_SAVE_GL_ERROR();
  glEnableClientState(GL_TEXTURE_COORD_ARRAY); CHECK_AND_SAVE_GL_ERROR();

  glVertexPointer(2, GL_FLOAT, sizeof(SurfaceVertex), &quad.verts_[0].x_);
  glTexCoordPointer(2, GL_FLOAT, sizeof(SurfaceVertex), &quad.verts_[0].tx_);
  glDrawArrays(GL_QUADS, 0, 4); CHECK_AND_SAVE_GL_ERROR();

  glDisableClientState(GL_VERTEX_ARRAY);
  glDisableClientState(GL_TEXTURE_COORD_ARRAY);
}

bool CompositingIOSurfaceMac::IsVendorIntel() {
  GLint screen;
  CGLGetVirtualScreen(context_->cgl_context(), &screen);
  if (screen != screen_)
    initialized_is_intel_ = false;
  screen_ = screen;
  if (!initialized_is_intel_) {
    is_intel_ = strstr(reinterpret_cast<const char*>(glGetString(GL_VENDOR)),
                      "Intel") != NULL;
    initialized_is_intel_ = true;
  }
  return is_intel_;
}

void CompositingIOSurfaceMac::UnrefIOSurfaceWithContextCurrent() {
  if (texture_) {
    glDeleteTextures(1, &texture_);
    texture_ = 0;
  }

  io_surface_.reset();

  // Forget the ID, because even if it is still around when we want to use it
  // again, OSX may have reused the same ID for a new tab and we don't want to
  // blit random tab contents.
  io_surface_handle_ = 0;
}

void CompositingIOSurfaceMac::DisplayLinkTick(CVDisplayLinkRef display_link,
                                              const CVTimeStamp* time) {
  TRACE_EVENT0("gpu", "CompositingIOSurfaceMac::DisplayLinkTick");
  base::AutoLock lock(lock_);
  CalculateVsyncParametersLockHeld(time);
}

void CompositingIOSurfaceMac::CalculateVsyncParametersLockHeld(
    const CVTimeStamp* time) {
  lock_.AssertAcquired();
  vsync_interval_numerator_ = static_cast<uint32>(time->videoRefreshPeriod);
  vsync_interval_denominator_ = time->videoTimeScale;
  // Verify that videoRefreshPeriod is 32 bits.
  DCHECK((time->videoRefreshPeriod & ~0xffffFFFFull) == 0ull);

  vsync_timebase_ =
      base::TimeTicks::FromInternalValue(time->hostTime / 1000);
}

void CompositingIOSurfaceMac::StartOrContinueDisplayLink() {
  if (display_link_ == NULL)
    return;

  if (!CVDisplayLinkIsRunning(display_link_)) {
    CVDisplayLinkStart(display_link_);
  }
  display_link_stop_timer_.Reset();
}

void CompositingIOSurfaceMac::StopDisplayLink() {
  if (display_link_ == NULL)
    return;

  if (CVDisplayLinkIsRunning(display_link_))
    CVDisplayLinkStop(display_link_);
}

bool CompositingIOSurfaceMac::IsAsynchronousReadbackSupported() {
  // Using PBO crashes on Intel drivers but not on newer Mountain Lion
  // systems. See bug http://crbug.com/152225.
  const bool forced_synchronous = CommandLine::ForCurrentProcess()->HasSwitch(
                                      switches::kForceSynchronousGLReadPixels);
  return (!forced_synchronous &&
          HasAppleFenceExtension() &&
          HasPixelBufferObjectExtension() &&
          (base::mac::IsOSMountainLionOrLater() || !IsVendorIntel()));
}

base::Closure CompositingIOSurfaceMac::CopyToSelectedOutputWithinContext(
    const gfx::Rect& src_pixel_subrect,
    const gfx::Rect& dst_pixel_rect,
    bool called_within_draw,
    const SkBitmap* bitmap_output,
    const scoped_refptr<media::VideoFrame>& video_frame_output,
    const base::Callback<void(bool)>& done_callback) {
  DCHECK_NE(bitmap_output != NULL, video_frame_output.get() != NULL);
  DCHECK(!done_callback.is_null());

  // SWIZZLE_RGBA_FOR_ASYNC_READPIXELS workaround: Fall-back to synchronous
  // readback for SkBitmap output since the Blit shader program doesn't support
  // switchable output formats.
  const bool require_sync_copy_for_workaround = bitmap_output &&
      context_->shader_program_cache()->rgb_to_yv12_output_format() == GL_RGBA;
  const bool async_copy = !require_sync_copy_for_workaround &&
      IsAsynchronousReadbackSupported();
  TRACE_EVENT2(
      "browser", "CompositingIOSurfaceMac::CopyToSelectedOutputWithinContext",
      "output", bitmap_output ? "SkBitmap (ARGB)" : "VideoFrame (YV12)",
      "async_readback", async_copy);

  const gfx::Rect src_rect = IntersectWithIOSurface(src_pixel_subrect);
  if (src_rect.IsEmpty() || dst_pixel_rect.IsEmpty())
    return base::Bind(done_callback, false);

  CopyContext* copy_context;
  if (copy_context_pool_.empty()) {
    // Limit the maximum number of simultaneous copies to two.  Rationale:
    // Really, only one should ever be in-progress at a time, as we should
    // depend on the speed of the hardware to rate-limit the copying naturally.
    // In the asynchronous read-back case, the one currently in-flight copy is
    // highly likely to have finished by this point (i.e., it's just waiting for
    // us to make a glMapBuffer() call).  Therefore, we allow a second copy to
    // be started here.
    if (copy_requests_.size() >= 2)
      return base::Bind(done_callback, false);
    copy_context = new CopyContext(context_);
  } else {
    copy_context = copy_context_pool_.back();
    copy_context_pool_.pop_back();
  }

  if (!HasIOSurface())
    return base::Bind(done_callback, false);

  // Send transform commands to the GPU.
  copy_context->num_outputs = 0;
  if (bitmap_output) {
    if (copy_context->transformer->ResizeBilinear(
            texture_, src_rect, dst_pixel_rect.size(),
            &copy_context->output_textures[0])) {
      copy_context->output_readback_format = GL_BGRA;
      copy_context->num_outputs = 1;
      copy_context->output_texture_sizes[0] = dst_pixel_rect.size();
    }
  } else {
    if (copy_context->transformer->TransformRGBToYV12(
            texture_, src_rect, dst_pixel_rect.size(),
            &copy_context->output_textures[0],
            &copy_context->output_textures[1],
            &copy_context->output_textures[2],
            &copy_context->output_texture_sizes[0],
            &copy_context->output_texture_sizes[1])) {
      copy_context->output_readback_format =
          context_->shader_program_cache()->rgb_to_yv12_output_format();
      copy_context->num_outputs = 3;
      copy_context->output_texture_sizes[2] =
          copy_context->output_texture_sizes[1];
    }
  }
  if (!copy_context->num_outputs)
    return base::Bind(done_callback, false);

  // In the asynchronous case, issue commands to the GPU and return a null
  // closure here.  In the synchronous case, perform a blocking readback and
  // return a callback to be run outside the CGL context to indicate success.
  if (async_copy) {
    copy_context->done_callback = done_callback;
    AsynchronousReadbackForCopy(
        dst_pixel_rect, called_within_draw, copy_context, bitmap_output,
        video_frame_output);
    copy_requests_.push_back(copy_context);
    if (!finish_copy_timer_.IsRunning())
      finish_copy_timer_.Reset();
    return base::Closure();
  } else {
    const bool success = SynchronousReadbackForCopy(
        dst_pixel_rect, copy_context, bitmap_output, video_frame_output);
    return base::Bind(done_callback, success);
  }
}

void CompositingIOSurfaceMac::AsynchronousReadbackForCopy(
    const gfx::Rect& dst_pixel_rect,
    bool called_within_draw,
    CopyContext* copy_context,
    const SkBitmap* bitmap_output,
    const scoped_refptr<media::VideoFrame>& video_frame_output) {
  copy_context->PrepareForAsynchronousReadback();

  // Copy the textures to their corresponding PBO.
  for (int i = 0; i < copy_context->num_outputs; ++i) {
    TRACE_EVENT1(
        "browser", "CompositingIOSurfaceMac::AsynchronousReadbackForCopy",
        "plane", i);

    // Attach the output texture to the FBO.
    glBindFramebufferEXT(
        GL_READ_FRAMEBUFFER_EXT, copy_context->frame_buffers[i]);
    glFramebufferTexture2DEXT(
        GL_READ_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT,
        GL_TEXTURE_RECTANGLE_ARB, copy_context->output_textures[i], 0);
    DCHECK(glCheckFramebufferStatusEXT(GL_READ_FRAMEBUFFER_EXT) ==
               GL_FRAMEBUFFER_COMPLETE_EXT);

    // Create a PBO and issue an asynchronous read-back.
    glBindBufferARB(GL_PIXEL_PACK_BUFFER_ARB, copy_context->pixel_buffers[i]);
    CHECK_AND_SAVE_GL_ERROR();
    glBufferDataARB(GL_PIXEL_PACK_BUFFER_ARB,
                    copy_context->output_texture_sizes[i].GetArea() * 4,
                    NULL, GL_STREAM_READ_ARB);
    CHECK_AND_SAVE_GL_ERROR();
    glReadPixels(0, 0,
                 copy_context->output_texture_sizes[i].width(),
                 copy_context->output_texture_sizes[i].height(),
                 copy_context->output_readback_format,
                 GL_UNSIGNED_INT_8_8_8_8_REV, 0);
    CHECK_AND_SAVE_GL_ERROR();
  }

  glBindBufferARB(GL_PIXEL_PACK_BUFFER_ARB, 0); CHECK_AND_SAVE_GL_ERROR();
  glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0); CHECK_AND_SAVE_GL_ERROR();

  glSetFenceAPPLE(copy_context->fence); CHECK_GL_ERROR();
  copy_context->cycles_elapsed = 0;

  // When this asynchronous copy happens in a draw operaton there is no need
  // to explicitly flush because there will be a swap buffer and this flush
  // hurts performance.
  if (!called_within_draw) {
    glFlush(); CHECK_AND_SAVE_GL_ERROR();
  }

  copy_context->map_buffer_callback = bitmap_output ?
      base::Bind(&MapBufferToSkBitmap, bitmap_output) :
      base::Bind(&MapBufferToVideoFrame, video_frame_output, dst_pixel_rect);
}

void CompositingIOSurfaceMac::CheckIfAllCopiesAreFinished(
    bool block_until_finished) {
  std::vector<base::Closure> done_callbacks;
  CGLSetCurrentContext(context_->cgl_context());
  CheckIfAllCopiesAreFinishedWithinContext(
      block_until_finished, &done_callbacks);
  CGLSetCurrentContext(0);
  for (size_t i = 0; i < done_callbacks.size(); ++i)
    done_callbacks[i].Run();
}

void CompositingIOSurfaceMac::CheckIfAllCopiesAreFinishedWithinContext(
    bool block_until_finished,
    std::vector<base::Closure>* done_callbacks) {
  while (!copy_requests_.empty()) {
    CopyContext* const copy_context = copy_requests_.front();

    if (copy_context->fence && !glTestFenceAPPLE(copy_context->fence)) {
      CHECK_AND_SAVE_GL_ERROR();
      // Doing a glFinishFenceAPPLE can cause transparent window flashes when
      // switching tabs, so only do it when required.
      if (block_until_finished) {
        glFinishFenceAPPLE(copy_context->fence);
        CHECK_AND_SAVE_GL_ERROR();
      } else if (copy_context->cycles_elapsed < kFinishCopyRetryCycles) {
        ++copy_context->cycles_elapsed;
        // This copy has not completed there is no need to test subsequent
        // requests.
        break;
      }
    }
    CHECK_AND_SAVE_GL_ERROR();

    bool success = true;
    for (int i = 0; success && i < copy_context->num_outputs; ++i) {
      TRACE_EVENT1(
        "browser",
        "CompositingIOSurfaceMac::CheckIfAllCopiesAreFinishedWithinContext",
        "plane", i);

      glBindBufferARB(GL_PIXEL_PACK_BUFFER_ARB, copy_context->pixel_buffers[i]);
      CHECK_AND_SAVE_GL_ERROR();

      void* buf = glMapBuffer(GL_PIXEL_PACK_BUFFER_ARB, GL_READ_ONLY_ARB);
      CHECK_AND_SAVE_GL_ERROR();
      success &= copy_context->map_buffer_callback.Run(buf, i);
      glUnmapBufferARB(GL_PIXEL_PACK_BUFFER_ARB); CHECK_AND_SAVE_GL_ERROR();
    }
    copy_context->map_buffer_callback.Reset();
    glBindBufferARB(GL_PIXEL_PACK_BUFFER_ARB, 0); CHECK_AND_SAVE_GL_ERROR();

    copy_requests_.pop_front();
    done_callbacks->push_back(base::Bind(copy_context->done_callback, success));
    copy_context->done_callback.Reset();
    copy_context_pool_.push_back(copy_context);
  }
  if (copy_requests_.empty())
    finish_copy_timer_.Stop();

  CHECK(copy_requests_.empty() || !block_until_finished);
}

bool CompositingIOSurfaceMac::SynchronousReadbackForCopy(
    const gfx::Rect& dst_pixel_rect,
    CopyContext* copy_context,
    const SkBitmap* bitmap_output,
    const scoped_refptr<media::VideoFrame>& video_frame_output) {
  bool success = true;
  copy_context->PrepareReadbackFramebuffers();
  for (int i = 0; i < copy_context->num_outputs; ++i) {
    TRACE_EVENT1(
        "browser", "CompositingIOSurfaceMac::SynchronousReadbackForCopy",
        "plane", i);

    // Attach the output texture to the FBO.
    glBindFramebufferEXT(
        GL_READ_FRAMEBUFFER_EXT, copy_context->frame_buffers[i]);
    glFramebufferTexture2DEXT(
        GL_READ_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT,
        GL_TEXTURE_RECTANGLE_ARB, copy_context->output_textures[i], 0);
    DCHECK(glCheckFramebufferStatusEXT(GL_READ_FRAMEBUFFER_EXT) ==
               GL_FRAMEBUFFER_COMPLETE_EXT);

    // Blocking read-back of pixels from textures.
    void* buf;
    // When data must be transferred into a VideoFrame one scanline at a time,
    // it is necessary to allocate a separate buffer for glReadPixels() that can
    // be populated one-shot.
    //
    // TODO(miu): Don't keep allocating/deleting this buffer for every frame.
    // Keep it cached, allocated on first use.
    scoped_ptr<uint32[]> temp_readback_buffer;
    if (bitmap_output) {
      // The entire SkBitmap is populated, never a region within.  So, read the
      // texture directly into the bitmap's pixel memory.
      buf = bitmap_output->getPixels();
    } else {
      // Optimization: If the VideoFrame is letterboxed (not pillarboxed), and
      // its stride is equal to the stride of the data being read back, then
      // readback directly into the VideoFrame's buffer to save a round of
      // memcpy'ing.
      //
      // TODO(miu): Move these calculations into VideoFrame (need a CalcOffset()
      // method).  http://crbug.com/219779
      const int src_stride = copy_context->output_texture_sizes[i].width() * 4;
      const int dst_stride = video_frame_output->stride(i);
      if (src_stride == dst_stride && dst_pixel_rect.x() == 0) {
        const int y_offset = dst_pixel_rect.y() / (i == 0 ? 1 : 2);
        buf = video_frame_output->data(i) + y_offset * dst_stride;
      } else {
        // Create and readback into a temporary buffer because the data must be
        // transferred to VideoFrame's pixel memory one scanline at a time.
        temp_readback_buffer.reset(
            new uint32[copy_context->output_texture_sizes[i].GetArea()]);
        buf = temp_readback_buffer.get();
      }
    }
    glReadPixels(0, 0,
                 copy_context->output_texture_sizes[i].width(),
                 copy_context->output_texture_sizes[i].height(),
                 copy_context->output_readback_format,
                 GL_UNSIGNED_INT_8_8_8_8_REV, buf);
    CHECK_AND_SAVE_GL_ERROR();
    if (video_frame_output.get()) {
      if (!temp_readback_buffer) {
        // Apply letterbox black-out around view region.
        media::LetterboxYUV(video_frame_output.get(), dst_pixel_rect);
      } else {
        // Copy from temporary buffer and fully render the VideoFrame.
        success &= MapBufferToVideoFrame(video_frame_output, dst_pixel_rect,
                                         temp_readback_buffer.get(), i);
      }
    }
  }

  glBindFramebufferEXT(GL_READ_FRAMEBUFFER_EXT, 0); CHECK_AND_SAVE_GL_ERROR();
  copy_context_pool_.push_back(copy_context);
  return success;
}

void CompositingIOSurfaceMac::FailAllCopies() {
  for (size_t i = 0; i < copy_requests_.size(); ++i) {
    copy_requests_[i]->map_buffer_callback.Reset();

    base::Callback<void(bool)>& done_callback =
        copy_requests_[i]->done_callback;
    if (!done_callback.is_null()) {
      done_callback.Run(false);
      done_callback.Reset();
    }
  }
}

void CompositingIOSurfaceMac::DestroyAllCopyContextsWithinContext() {
  // Move all in-flight copies, if any, back into the pool.  Then, destroy all
  // the CopyContexts in the pool.
  copy_context_pool_.insert(copy_context_pool_.end(),
                            copy_requests_.begin(), copy_requests_.end());
  copy_requests_.clear();
  while (!copy_context_pool_.empty()) {
    scoped_ptr<CopyContext> copy_context(copy_context_pool_.back());
    copy_context_pool_.pop_back();
    copy_context->ReleaseCachedGLObjects();
  }
}

gfx::Rect CompositingIOSurfaceMac::IntersectWithIOSurface(
    const gfx::Rect& rect) const {
  return gfx::IntersectRects(rect,
      gfx::ToEnclosingRect(gfx::Rect(pixel_io_surface_size_)));
}

GLenum CompositingIOSurfaceMac::GetAndSaveGLError() {
  GLenum gl_error = glGetError();
  if (gl_error_ == GL_NO_ERROR)
    gl_error_ = gl_error;
  return gl_error;
}

}  // namespace content
