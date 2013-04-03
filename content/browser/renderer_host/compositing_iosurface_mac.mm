// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/compositing_iosurface_mac.h"

#include <OpenGL/CGLRenderers.h>
#include <OpenGL/OpenGL.h>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/debug/trace_event.h"
#include "base/logging.h"
#include "base/mac/mac_util.h"
#include "base/message_loop.h"
#include "base/threading/platform_thread.h"
#include "content/browser/renderer_host/compositing_iosurface_context_mac.h"
#include "content/browser/renderer_host/compositing_iosurface_shader_programs_mac.h"
#include "content/browser/renderer_host/compositing_iosurface_transformer_mac.h"
#include "content/common/content_constants_internal.h"
#include "content/port/browser/render_widget_host_view_frame_subscriber.h"
#include "gpu/command_buffer/service/gpu_switches.h"
#include "media/base/video_util.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/scoped_ns_graphics_context_save_gstate_mac.h"
#include "ui/gl/gl_context.h"
#include "ui/gfx/size_conversions.h"
#include "ui/surface/io_surface_support_mac.h"

#ifdef NDEBUG
#define CHECK_GL_ERROR()
#else
#define CHECK_GL_ERROR() do {                                           \
    GLenum gl_error = glGetError();                                     \
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
    media::LetterboxYUV(target, region_in_frame);

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

CompositingIOSurfaceMac::CopyContext::CopyContext()
  : num_outputs(0),
    fence(0),
    cycles_elapsed(0) {
  memset(output_textures, 0, sizeof(output_textures));
  memset(frame_buffers, 0, sizeof(frame_buffers));
  memset(pixel_buffers, 0, sizeof(pixel_buffers));
}

CompositingIOSurfaceMac::CopyContext::~CopyContext() {
}

void CompositingIOSurfaceMac::CopyContext::CleanUp() {
  glDeleteFramebuffersEXT(num_outputs, frame_buffers); CHECK_GL_ERROR();
  glDeleteTextures(num_outputs, output_textures);
  CHECK_GL_ERROR();

  // For an asynchronous read-back, there are more objects to delete:
  if (fence) {
    glDeleteBuffers(num_outputs, pixel_buffers);
    CHECK_GL_ERROR();
    glDeleteFencesAPPLE(1, &fence); CHECK_GL_ERROR();
  }
}

// static
CompositingIOSurfaceMac* CompositingIOSurfaceMac::Create(SurfaceOrder order) {
  TRACE_EVENT0("browser", "CompositingIOSurfaceMac::Create");
  IOSurfaceSupport* io_surface_support = IOSurfaceSupport::Initialize();
  if (!io_surface_support) {
    LOG(WARNING) << "No IOSurface support";
    return NULL;
  }

  scoped_refptr<CompositingIOSurfaceContext> context =
      CompositingIOSurfaceContext::Get(order);

  CVDisplayLinkRef display_link;
  CVReturn ret = CVDisplayLinkCreateWithActiveCGDisplays(&display_link);
  if (ret != kCVReturnSuccess) {
    LOG(ERROR) << "CVDisplayLinkCreateWithActiveCGDisplays failed: " << ret;
    return NULL;
  }

  return new CompositingIOSurfaceMac(io_surface_support,
                                     context,
                                     display_link);
}

CompositingIOSurfaceMac::CompositingIOSurfaceMac(
    IOSurfaceSupport* io_surface_support,
    scoped_refptr<CompositingIOSurfaceContext> context,
    CVDisplayLinkRef display_link)
    : io_surface_support_(io_surface_support),
      context_(context),
      io_surface_handle_(0),
      texture_(0),
      finish_copy_timer_(
          FROM_HERE,
          base::TimeDelta::FromMilliseconds(kFinishCopyPollingPeriodMs),
          base::Bind(&CompositingIOSurfaceMac::FinishAllCopies,
                     base::Unretained(this)),
          true),
      display_link_(display_link),
      display_link_stop_timer_(FROM_HERE, base::TimeDelta::FromSeconds(1),
                               this, &CompositingIOSurfaceMac::StopDisplayLink),
      vsync_count_(0),
      swap_count_(0),
      vsync_interval_numerator_(0),
      vsync_interval_denominator_(0),
      initialized_is_intel_(false),
      is_intel_(false),
      screen_(0) {
  CVReturn ret = CVDisplayLinkSetOutputCallback(display_link_,
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
  CleanupAllCopiesWithinContext();
  UnrefIOSurfaceWithContextCurrent();
  CGLSetCurrentContext(0);
  context_ = nil;
}

void CompositingIOSurfaceMac::SetIOSurface(uint64 io_surface_handle,
                                           const gfx::Size& size) {
  pixel_io_surface_size_ = size;
  CGLSetCurrentContext(context_->cgl_context());
  MapIOSurfaceToTexture(io_surface_handle);
  CGLSetCurrentContext(0);
}

int CompositingIOSurfaceMac::GetRendererID() {
  GLint current_renderer_id = -1;
  if (CGLGetParameter(context_->cgl_context(),
                      kCGLCPCurrentRendererID,
                      &current_renderer_id) == kCGLNoError)
    return current_renderer_id & kCGLRendererIDMatchingMask;
  return -1;
}

void CompositingIOSurfaceMac::DrawIOSurface(
    NSView* view, float scale_factor,
    RenderWidgetHostViewFrameSubscriber* frame_subscriber) {
  CGLSetCurrentContext(context_->cgl_context());

  bool has_io_surface = MapIOSurfaceToTexture(io_surface_handle_);

  TRACE_EVENT1("browser", "CompositingIOSurfaceMac::DrawIOSurface",
               "has_io_surface", has_io_surface);

  [context_->nsgl_context() setView:view];
  gfx::Size window_size(NSSizeToCGSize([view frame].size));
  gfx::Size pixel_window_size = gfx::ToFlooredSize(
      gfx::ScaleSize(window_size, scale_factor));
  glViewport(0, 0, pixel_window_size.width(), pixel_window_size.height());

  // TODO: After a resolution change, the DPI-ness of the view and the
  // IOSurface might not be in sync.
  io_surface_size_ = gfx::ToFlooredSize(
      gfx::ScaleSize(pixel_io_surface_size_, 1.0 / scale_factor));
  quad_.set_size(io_surface_size_, pixel_io_surface_size_);

  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();

  // Note that the projection keeps things in view units, so the use of
  // window_size / io_surface_size_ (as opposed to the pixel_ variants) below is
  // correct.
  glOrtho(0, window_size.width(), window_size.height(), 0, -1, 1);
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();

  glDisable(GL_DEPTH_TEST);
  glDisable(GL_BLEND);

  if (has_io_surface) {
    context_->shader_program_cache()->UseBlitProgram();
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_RECTANGLE_ARB, texture_);

    DrawQuad(quad_);

    glBindTexture(GL_TEXTURE_RECTANGLE_ARB, 0); CHECK_GL_ERROR();

    // Fill the resize gutters with white.
    if (window_size.width() > io_surface_size_.width() ||
        window_size.height() > io_surface_size_.height()) {
      context_->shader_program_cache()->UseSolidWhiteProgram();
      SurfaceQuad filler_quad;
      if (window_size.width() > io_surface_size_.width()) {
        // Draw right-side gutter down to the bottom of the window.
        filler_quad.set_rect(io_surface_size_.width(), 0.0f,
                             window_size.width(), window_size.height());
        DrawQuad(filler_quad);
      }
      if (window_size.height() > io_surface_size_.height()) {
        // Draw bottom gutter to the width of the IOSurface.
        filler_quad.set_rect(0.0f, io_surface_size_.height(),
                             io_surface_size_.width(), window_size.height());
        DrawQuad(filler_quad);
      }
    }

    // Workaround for issue 158469. Issue a dummy draw call with texture_ not
    // bound to blit_rgb_sampler_location_, in order to shake all references
    // to the IOSurface out of the driver.
    glBegin(GL_TRIANGLES);
    glEnd();

    glUseProgram(0); CHECK_GL_ERROR();
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
    scoped_refptr<media::VideoFrame> frame;
    RenderWidgetHostViewFrameSubscriber::DeliverFrameCallback callback;
    if (frame_subscriber->ShouldCaptureFrame(&frame, &callback)) {
      copy_done_callback = CopyToVideoFrameWithinContext(
          gfx::Rect(pixel_io_surface_size_), scale_factor, true, frame,
          base::Bind(callback, base::Time::Now()));
    }
  }

  CGLFlushDrawable(context_->cgl_context());

  // For latency_tests.cc:
  UNSHIPPED_TRACE_EVENT_INSTANT0("test_gpu", "CompositorSwapBuffersComplete",
                                 TRACE_EVENT_SCOPE_THREAD);

  // Try to finish previous copy requests after flush to get better pipelining.
  std::vector<base::Closure> copy_done_callbacks;
  FinishAllCopiesWithinContext(&copy_done_callbacks);

  CGLSetCurrentContext(0);

  if (!copy_done_callback.is_null())
    copy_done_callbacks.push_back(copy_done_callback);
  for (size_t i = 0; i < copy_done_callbacks.size(); ++i)
    copy_done_callbacks[i].Run();

  StartOrContinueDisplayLink();

  if (!is_vsync_disabled())
    RateLimitDraws();
}

void CompositingIOSurfaceMac::CopyTo(
      const gfx::Rect& src_pixel_subrect,
      float src_scale_factor,
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
      src_pixel_subrect, src_scale_factor, gfx::Rect(dst_pixel_size), false,
      output.get(), NULL,
      base::Bind(&ReverseArgumentOrder, callback, base::Passed(&output)));
  CGLSetCurrentContext(0);
  if (!copy_done_callback.is_null())
    copy_done_callback.Run();
}

void CompositingIOSurfaceMac::CopyToVideoFrame(
    const gfx::Rect& src_pixel_subrect,
    float src_scale_factor,
    const scoped_refptr<media::VideoFrame>& target,
    const base::Callback<void(bool)>& callback) {
  CGLSetCurrentContext(context_->cgl_context());
  const base::Closure copy_done_callback = CopyToVideoFrameWithinContext(
      src_pixel_subrect, src_scale_factor, false, target, callback);
  CGLSetCurrentContext(0);
  if (!copy_done_callback.is_null())
    copy_done_callback.Run();
}

base::Closure CompositingIOSurfaceMac::CopyToVideoFrameWithinContext(
    const gfx::Rect& src_pixel_subrect,
    float src_scale_factor,
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
  DCHECK(!region_in_frame.IsEmpty());
  DCHECK_LE(region_in_frame.right(), target->coded_size().width());
  DCHECK_LE(region_in_frame.bottom(), target->coded_size().height());

  return CopyToSelectedOutputWithinContext(
      src_pixel_subrect, src_scale_factor, region_in_frame, called_within_draw,
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
  if (!io_surface_.get()) {
    io_surface_handle_ = 0;
    return false;
  }

  io_surface_handle_ = io_surface_handle;

  // Actual IOSurface size is rounded up to reduce reallocations during window
  // resize. Get the actual size to properly map the texture.
  gfx::Size rounded_size(
      io_surface_support_->IOSurfaceGetWidth(io_surface_),
      io_surface_support_->IOSurfaceGetHeight(io_surface_));

  // TODO(thakis): Keep track of the view size over IPC. At the moment,
  // the correct view units are computed on first paint.
  io_surface_size_ = pixel_io_surface_size_;

  GLenum target = GL_TEXTURE_RECTANGLE_ARB;
  glGenTextures(1, &texture_);
  glBindTexture(target, texture_);
  glTexParameterf(target, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameterf(target, GL_TEXTURE_MAG_FILTER, GL_NEAREST); CHECK_GL_ERROR();
  GLuint plane = 0;
  CGLError cglerror = io_surface_support_->CGLTexImageIOSurface2D(
      context_->cgl_context(),
      target,
      GL_RGBA,
      rounded_size.width(),
      rounded_size.height(),
      GL_BGRA,
      GL_UNSIGNED_INT_8_8_8_8_REV,
      io_surface_.get(),
      plane);
  CHECK_GL_ERROR();
  if (cglerror != kCGLNoError) {
    LOG(ERROR) << "CGLTexImageIOSurface2D: " << cglerror;
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
  glEnableClientState(GL_VERTEX_ARRAY); CHECK_GL_ERROR();
  glEnableClientState(GL_TEXTURE_COORD_ARRAY); CHECK_GL_ERROR();

  glVertexPointer(2, GL_FLOAT, sizeof(SurfaceVertex), &quad.verts_[0].x_);
  glTexCoordPointer(2, GL_FLOAT, sizeof(SurfaceVertex), &quad.verts_[0].tx_);
  glDrawArrays(GL_QUADS, 0, 4); CHECK_GL_ERROR();

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

void CompositingIOSurfaceMac::GlobalFrameDidChange() {
  [context_->nsgl_context() update];
}

void CompositingIOSurfaceMac::ClearDrawable() {
  [context_->nsgl_context() clearDrawable];
  UnrefIOSurface();
}

void CompositingIOSurfaceMac::DisplayLinkTick(CVDisplayLinkRef display_link,
                                              const CVTimeStamp* time) {
  TRACE_EVENT0("gpu", "CompositingIOSurfaceMac::DisplayLinkTick");
  base::AutoLock lock(lock_);
  // Increment vsync_count but don't let it get ahead of swap_count.
  vsync_count_ = std::min(vsync_count_ + 1, swap_count_);

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

void CompositingIOSurfaceMac::RateLimitDraws() {
  int64 vsync_count;
  int64 swap_count;

  {
    base::AutoLock lock(lock_);
    vsync_count = vsync_count_;
    swap_count = ++swap_count_;
  }

  // It's OK for swap_count to get 2 ahead of vsync_count, but any more
  // indicates that it has become unthrottled. This happens when, for example,
  // the window is obscured by another opaque window.
  if (swap_count > vsync_count + 2) {
    TRACE_EVENT0("gpu", "CompositingIOSurfaceMac::RateLimitDraws");
    // Sleep for one vsync interval. This will prevent spinning while the window
    // is not visible, but will also allow quick recovery when the window
    // becomes visible again.
    int64 sleep_us = 16666;  // default to 60hz if display link API fails.
    if (vsync_interval_denominator_ > 0) {
      sleep_us = (static_cast<int64>(vsync_interval_numerator_) * 1000000) /
                 vsync_interval_denominator_;
    }
    base::PlatformThread::Sleep(base::TimeDelta::FromMicroseconds(sleep_us));
  }
}

void CompositingIOSurfaceMac::StartOrContinueDisplayLink() {
  if (!CVDisplayLinkIsRunning(display_link_)) {
    vsync_count_ = swap_count_ = 0;
    CVDisplayLinkStart(display_link_);
  }
  display_link_stop_timer_.Reset();
}

void CompositingIOSurfaceMac::StopDisplayLink() {
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
    float src_scale_factor,
    const gfx::Rect& dst_pixel_rect,
    bool called_within_draw,
    const SkBitmap* bitmap_output,
    const scoped_refptr<media::VideoFrame>& video_frame_output,
    const base::Callback<void(bool)>& done_callback) {
  DCHECK_NE(bitmap_output != NULL, video_frame_output != NULL);
  DCHECK(!done_callback.is_null());

  // TODO(miu): Forcing synchronous copy for M27.  Will work on fixing the
  // desired asynchronous method for M28.  http://crbug.com/223326
  const bool async_copy = false;
  TRACE_EVENT2(
      "browser", "CompositingIOSurfaceMac::CopyToSelectedOutputWithinContext",
      "output", bitmap_output ? "SkBitmap (ARGB)" : "VideoFrame (YV12)",
      "async_readback", async_copy);

  // Set up source texture, bound to the GL_TEXTURE_RECTANGLE_ARB target.
  if (!MapIOSurfaceToTexture(io_surface_handle_))
    return base::Bind(done_callback, false);

  // Create the transformer_ on first use.
  if (!transformer_) {
    transformer_.reset(new CompositingIOSurfaceTransformer(
        GL_TEXTURE_RECTANGLE_ARB,
        true,
        context_->shader_program_cache()));
  }

  // Send transform commands to the GPU.
  const gfx::Rect src_rect = IntersectWithIOSurface(src_pixel_subrect,
                                                    src_scale_factor);
  CopyContext copy_context;
  if (bitmap_output) {
    if (transformer_->ResizeBilinear(texture_, src_rect, dst_pixel_rect.size(),
                                     &copy_context.output_textures[0])) {
      copy_context.num_outputs = 1;
      copy_context.output_texture_sizes[0] = dst_pixel_rect.size();
    }
  } else {
    if (transformer_->TransformRGBToYV12(
            texture_, src_rect, dst_pixel_rect.size(),
            &copy_context.output_textures[0],
            &copy_context.output_textures[1],
            &copy_context.output_textures[2],
            &copy_context.output_texture_sizes[0],
            &copy_context.output_texture_sizes[1])) {
      copy_context.num_outputs = 3;
      copy_context.output_texture_sizes[2] =
          copy_context.output_texture_sizes[1];
    }
  }
  if (!copy_context.num_outputs)
    return base::Bind(done_callback, false);

  // In the asynchronous case, issue commands to the GPU and return a null
  // closure here.  In the synchronous case, perform a blocking readback and
  // return a callback to be run outside the CGL context to indicate success.
  if (async_copy) {
    copy_context.done_callback = done_callback;
    AsynchronousReadbackForCopy(
        dst_pixel_rect, called_within_draw, &copy_context, bitmap_output,
        video_frame_output);
    copy_requests_.push_back(copy_context);
    if (!finish_copy_timer_.IsRunning())
      finish_copy_timer_.Reset();
    return base::Closure();
  } else {
    const bool success = SynchronousReadbackForCopy(
        dst_pixel_rect, &copy_context, bitmap_output, video_frame_output);
    return base::Bind(done_callback, success);
  }
}

void CompositingIOSurfaceMac::AsynchronousReadbackForCopy(
    const gfx::Rect& dst_pixel_rect,
    bool called_within_draw,
    CopyContext* copy_context,
    const SkBitmap* bitmap_output,
    const scoped_refptr<media::VideoFrame>& video_frame_output) {
  glGenFencesAPPLE(1, &copy_context->fence); CHECK_GL_ERROR();

  // Copy the textures to a PBO.
  glGenFramebuffersEXT(copy_context->num_outputs, copy_context->frame_buffers);
  CHECK_GL_ERROR();
  glGenBuffersARB(copy_context->num_outputs, copy_context->pixel_buffers);
  CHECK_GL_ERROR();
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
    CHECK_GL_ERROR();
    glBufferDataARB(GL_PIXEL_PACK_BUFFER_ARB,
                    copy_context->output_texture_sizes[i].GetArea() * 4,
                    NULL, GL_STREAM_READ_ARB); CHECK_GL_ERROR();
    glReadPixels(0, 0,
                 copy_context->output_texture_sizes[i].width(),
                 copy_context->output_texture_sizes[i].height(),
                 GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, 0); CHECK_GL_ERROR();
  }

  glBindBufferARB(GL_PIXEL_PACK_BUFFER_ARB, 0); CHECK_GL_ERROR();
  glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0); CHECK_GL_ERROR();

  glSetFenceAPPLE(copy_context->fence); CHECK_GL_ERROR();

  // When this asynchronous copy happens in a draw operaton there is not need
  // to explicitly flush because there will be a swap buffer and this flush
  // hurts performance.
  if (!called_within_draw) {
    glFlush(); CHECK_GL_ERROR();
  }

  copy_context->map_buffer_callback = bitmap_output ?
      base::Bind(&MapBufferToSkBitmap, bitmap_output) :
      base::Bind(&MapBufferToVideoFrame, video_frame_output, dst_pixel_rect);
}

void CompositingIOSurfaceMac::FinishAllCopies() {
  std::vector<base::Closure> done_callbacks;
  CGLSetCurrentContext(context_->cgl_context());
  FinishAllCopiesWithinContext(&done_callbacks);
  CGLSetCurrentContext(0);
  for (size_t i = 0; i < done_callbacks.size(); ++i)
    done_callbacks[i].Run();
}

void CompositingIOSurfaceMac::FinishAllCopiesWithinContext(
    std::vector<base::Closure>* done_callbacks) {
  while (!copy_requests_.empty()) {
    CopyContext& copy_context = copy_requests_.front();

    if (copy_context.fence) {
      const bool copy_completed = glTestFenceAPPLE(copy_context.fence);
      CHECK_GL_ERROR();

      if (!copy_completed &&
        copy_context.cycles_elapsed < kFinishCopyRetryCycles) {
        ++copy_context.cycles_elapsed;
        // This copy has not completed there is no need to test subsequent
        // requests.
        break;
      }
    }

    bool success = true;
    for (int i = 0; success && i < copy_context.num_outputs; ++i) {
      TRACE_EVENT1(
        "browser", "CompositingIOSurfaceMac::FinishAllCopyWithinContext",
        "plane", i);

      glBindBufferARB(GL_PIXEL_PACK_BUFFER_ARB, copy_context.pixel_buffers[i]);
      CHECK_GL_ERROR();

      void* buf = glMapBuffer(GL_PIXEL_PACK_BUFFER_ARB, GL_READ_ONLY_ARB);
      CHECK_GL_ERROR();
      success &= copy_context.map_buffer_callback.Run(buf, i);
      glUnmapBufferARB(GL_PIXEL_PACK_BUFFER_ARB); CHECK_GL_ERROR();
    }

    glBindBufferARB(GL_PIXEL_PACK_BUFFER_ARB, 0); CHECK_GL_ERROR();
    copy_context.CleanUp();
    done_callbacks->push_back(base::Bind(copy_context.done_callback, success));
    copy_requests_.pop_front();
  }
  if (copy_requests_.empty())
    finish_copy_timer_.Stop();
}

bool CompositingIOSurfaceMac::SynchronousReadbackForCopy(
    const gfx::Rect& dst_pixel_rect,
    CopyContext* copy_context,
    const SkBitmap* bitmap_output,
    const scoped_refptr<media::VideoFrame>& video_frame_output) {
  bool success = true;
  glGenFramebuffersEXT(copy_context->num_outputs, copy_context->frame_buffers);
  CHECK_GL_ERROR();
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
                 GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV,
                 buf); CHECK_GL_ERROR();
    if (video_frame_output) {
      if (!temp_readback_buffer) {
        // Apply letterbox black-out around view region.
        media::LetterboxYUV(video_frame_output, dst_pixel_rect);
      } else {
        // Copy from temporary buffer and fully render the VideoFrame.
        success &= MapBufferToVideoFrame(video_frame_output, dst_pixel_rect,
                                         temp_readback_buffer.get(), i);
      }
    }
  }

  glBindFramebufferEXT(GL_READ_FRAMEBUFFER_EXT, 0); CHECK_GL_ERROR();
  copy_context->CleanUp();
  return success;
}

void CompositingIOSurfaceMac::CleanupAllCopiesWithinContext() {
  for (size_t i = 0; i < copy_requests_.size(); ++i)
    copy_requests_[i].CleanUp();
  copy_requests_.clear();
}

void CompositingIOSurfaceMac::FailAllCopies() {
  for (size_t i = 0; i < copy_requests_.size(); ++i)
    copy_requests_[i].done_callback.Run(false);
}

gfx::Rect CompositingIOSurfaceMac::IntersectWithIOSurface(
    const gfx::Rect& rect, float scale_factor) const {
  return gfx::IntersectRects(rect,
      gfx::ToEnclosingRect(gfx::ScaleRect(gfx::Rect(io_surface_size_),
                                          scale_factor)));
}

}  // namespace content
