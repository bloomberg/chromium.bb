// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/compositing_iosurface_mac.h"

#include <OpenGL/OpenGL.h>
#include <vector>

#include "base/command_line.h"
#include "base/debug/trace_event.h"
#include "base/threading/platform_thread.h"
#include "content/common/content_constants_internal.h"
#include "content/public/browser/browser_thread.h"
#include "gpu/command_buffer/service/gpu_switches.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/scoped_ns_graphics_context_save_gstate_mac.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_switches.h"
#include "ui/surface/io_surface_support_mac.h"

#ifdef NDEBUG
#define CHECK_GL_ERROR()
#else
#define CHECK_GL_ERROR() do {                                           \
    GLenum gl_error = glGetError();                                     \
    LOG_IF(ERROR, gl_error != GL_NO_ERROR) << "GL Error :" << gl_error; \
  } while (0)
#endif

#define SHADER_STRING_GLSL(shader) #shader

namespace content {
namespace {

const char* g_vertex_shader_blit_rgb = SHADER_STRING_GLSL(
    varying vec2 texture_coordinate;
    void main() {
      gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;
      texture_coordinate = vec2(gl_MultiTexCoord0);
    });

const char* g_fragment_shader_blit_rgb = SHADER_STRING_GLSL(
    varying vec2 texture_coordinate;
    uniform sampler2DRect texture;
    void main() {
      gl_FragColor = vec4(texture2DRect(texture, texture_coordinate).rgb, 1.0);
    });

const char* g_vertex_shader_white = SHADER_STRING_GLSL(
    void main() {
      gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;
    });

const char* g_fragment_shader_white = SHADER_STRING_GLSL(
    void main() {
      gl_FragColor = vec4(1.0, 1.0, 1.0, 1.0);
    });

// Create and compile shader, return its ID or 0 on error.
GLuint CompileShaderGLSL(GLenum type, const char* shader_str) {
  GLuint shader = glCreateShader(type);
  glShaderSource(shader, 1, &shader_str, NULL);
  glCompileShader(shader); CHECK_GL_ERROR();
  GLint error;
  glGetShaderiv(shader, GL_COMPILE_STATUS, &error);
  if (error != GL_TRUE) {
    glDeleteShader(shader);
    return 0;
  }
  return shader;
}

// Compile the given vertex and shader source strings into a GLSL program.
GLuint CreateProgramGLSL(const char* vertex_shader_str,
                         const char* fragment_shader_str) {
  GLuint vertex_shader =
      CompileShaderGLSL(GL_VERTEX_SHADER, vertex_shader_str);
  if (!vertex_shader)
    return 0;

  GLuint fragment_shader =
      CompileShaderGLSL(GL_FRAGMENT_SHADER, fragment_shader_str);
  if (!fragment_shader) {
    glDeleteShader(vertex_shader);
    return 0;
  }

  GLuint program = glCreateProgram(); CHECK_GL_ERROR();
  glAttachShader(program, vertex_shader);
  glAttachShader(program, fragment_shader);
  glLinkProgram(program); CHECK_GL_ERROR();

  // Flag shaders for deletion so that they will be deleted when the program
  // is deleted. That way we don't have to retain these IDs.
  glDeleteShader(vertex_shader);
  glDeleteShader(fragment_shader);

  GLint error;
  glGetProgramiv(program, GL_LINK_STATUS, &error);
  if (error != GL_TRUE) {
    glDeleteProgram(program);
    return 0;
  }
  return program;
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

CompositingIOSurfaceMac* CompositingIOSurfaceMac::Create() {
  TRACE_EVENT0("browser", "CompositingIOSurfaceMac::Create");
  IOSurfaceSupport* io_surface_support = IOSurfaceSupport::Initialize();
  if (!io_surface_support) {
    LOG(WARNING) << "No IOSurface support";
    return NULL;
  }

  std::vector<NSOpenGLPixelFormatAttribute> attributes;
  attributes.push_back(NSOpenGLPFADoubleBuffer);
  // We don't need a depth buffer - try setting its size to 0...
  attributes.push_back(NSOpenGLPFADepthSize); attributes.push_back(0);
  if (gfx::GLContext::SupportsDualGpus())
    attributes.push_back(NSOpenGLPFAAllowOfflineRenderers);
  attributes.push_back(0);

  scoped_nsobject<NSOpenGLPixelFormat> glPixelFormat(
      [[NSOpenGLPixelFormat alloc] initWithAttributes:&attributes.front()]);
  if (!glPixelFormat) {
    LOG(ERROR) << "NSOpenGLPixelFormat initWithAttributes failed";
    return NULL;
  }

  scoped_nsobject<NSOpenGLContext> glContext(
      [[NSOpenGLContext alloc] initWithFormat:glPixelFormat
                                 shareContext:nil]);
  if (!glContext) {
    LOG(ERROR) << "NSOpenGLContext initWithFormat failed";
    return NULL;
  }

  // We "punch a hole" in the window, and have the WindowServer render the
  // OpenGL surface underneath so we can draw over it.
  GLint belowWindow = -1;
  [glContext setValues:&belowWindow forParameter:NSOpenGLCPSurfaceOrder];

  CGLContextObj cglContext = (CGLContextObj)[glContext CGLContextObj];
  if (!cglContext) {
    LOG(ERROR) << "CGLContextObj failed";
    return NULL;
  }

  // Draw at beam vsync.
  bool is_vsync_disabled =
      CommandLine::ForCurrentProcess()->HasSwitch(switches::kDisableGpuVsync);
  GLint swapInterval = is_vsync_disabled ? 0 : 1;
  [glContext setValues:&swapInterval forParameter:NSOpenGLCPSwapInterval];

  // Build shaders.
  CGLSetCurrentContext(cglContext);
  GLuint shader_program_blit_rgb =
      CreateProgramGLSL(g_vertex_shader_blit_rgb, g_fragment_shader_blit_rgb);
  GLuint shader_program_white =
      CreateProgramGLSL(g_vertex_shader_white, g_fragment_shader_white);
  GLint blit_rgb_sampler_location =
      glGetUniformLocation(shader_program_blit_rgb, "texture");
  CGLSetCurrentContext(0);

  if (!shader_program_blit_rgb || !shader_program_white ||
      blit_rgb_sampler_location == -1) {
    LOG(ERROR) << "IOSurface shader build error";
    return NULL;
  }

  CVDisplayLinkRef display_link;
  CVReturn ret = CVDisplayLinkCreateWithActiveCGDisplays(&display_link);
  if (ret != kCVReturnSuccess) {
    LOG(ERROR) << "CVDisplayLinkCreateWithActiveCGDisplays failed: " << ret;
    return NULL;
  }

  // Set the display link for the current renderer
  CGLPixelFormatObj cglPixelFormat =
      (CGLPixelFormatObj)[glPixelFormat CGLPixelFormatObj];
  ret = CVDisplayLinkSetCurrentCGDisplayFromOpenGLContext(display_link,
                                                          cglContext,
                                                          cglPixelFormat);
  if (ret != kCVReturnSuccess) {
    CVDisplayLinkRelease(display_link);
    LOG(ERROR) << "CVDisplayLinkSetCurrentCGDisplayFromOpenGLContext failed: "
               << ret;
    return NULL;
  }

  return new CompositingIOSurfaceMac(io_surface_support, glContext.release(),
                                     cglContext,
                                     shader_program_blit_rgb,
                                     blit_rgb_sampler_location,
                                     shader_program_white,
                                     is_vsync_disabled,
                                     display_link);
}

CompositingIOSurfaceMac::CompositingIOSurfaceMac(
    IOSurfaceSupport* io_surface_support,
    NSOpenGLContext* glContext,
    CGLContextObj cglContext,
    GLuint shader_program_blit_rgb,
    GLint blit_rgb_sampler_location,
    GLuint shader_program_white,
    bool is_vsync_disabled,
    CVDisplayLinkRef display_link)
    : io_surface_support_(io_surface_support),
      glContext_(glContext),
      cglContext_(cglContext),
      io_surface_handle_(0),
      texture_(0),
      shader_program_blit_rgb_(shader_program_blit_rgb),
      blit_rgb_sampler_location_(blit_rgb_sampler_location),
      shader_program_white_(shader_program_white),
      is_vsync_disabled_(is_vsync_disabled),
      display_link_(display_link),
      display_link_stop_timer_(FROM_HERE, base::TimeDelta::FromSeconds(1),
                               this, &CompositingIOSurfaceMac::StopDisplayLink),
      vsync_count_(0),
      swap_count_(0),
      vsync_interval_numerator_(0),
      vsync_interval_denominator_(0) {
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

void CompositingIOSurfaceMac::GetVSyncParameters(base::TimeTicks* timebase,
                                                 uint32* interval_numerator,
                                                 uint32* interval_denominator) {
  base::AutoLock lock(lock_);
  *timebase = vsync_timebase_;
  *interval_numerator = vsync_interval_numerator_;
  *interval_denominator = vsync_interval_denominator_;
}

CompositingIOSurfaceMac::~CompositingIOSurfaceMac() {
  CVDisplayLinkRelease(display_link_);
  UnrefIOSurface();
}

void CompositingIOSurfaceMac::SetIOSurface(uint64 io_surface_handle,
                                           const gfx::Size& size) {
  pixel_io_surface_size_ = size;
  CGLSetCurrentContext(cglContext_);
  MapIOSurfaceToTexture(io_surface_handle);
  CGLSetCurrentContext(0);
}

void CompositingIOSurfaceMac::DrawIOSurface(NSView* view, float scale_factor) {
  CGLSetCurrentContext(cglContext_);

  bool has_io_surface = MapIOSurfaceToTexture(io_surface_handle_);

  TRACE_EVENT1("browser", "CompositingIOSurfaceMac::DrawIOSurface",
               "has_io_surface", has_io_surface);

  [glContext_ setView:view];
  gfx::Size window_size(NSSizeToCGSize([view frame].size));
  gfx::Size pixel_window_size = window_size.Scale(scale_factor);
  glViewport(0, 0, pixel_window_size.width(), pixel_window_size.height());

  // TODO: After a resolution change, the DPI-ness of the view and the
  // IOSurface might not be in sync.
  io_surface_size_ = pixel_io_surface_size_.Scale(1.0 / scale_factor);
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
    glUseProgram(shader_program_blit_rgb_);

    int texture_unit = 0;
    glUniform1i(blit_rgb_sampler_location_, texture_unit);
    glActiveTexture(GL_TEXTURE0 + texture_unit);
    glBindTexture(GL_TEXTURE_RECTANGLE_ARB, texture_);

    DrawQuad(quad_);

    glBindTexture(GL_TEXTURE_RECTANGLE_ARB, 0); CHECK_GL_ERROR();

    // Fill the resize gutters with white.
    if (window_size.width() > io_surface_size_.width() ||
        window_size.height() > io_surface_size_.height()) {
      glUseProgram(shader_program_white_);
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

    glUseProgram(0); CHECK_GL_ERROR();
  } else {
    // Should match the clear color of RenderWidgetHostViewMac.
    glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
  }

  static bool initialized_workaround = false;
  static bool use_glfinish_workaround = false;

  if (!initialized_workaround) {
    use_glfinish_workaround =
        (strstr(reinterpret_cast<const char*>(
            glGetString(GL_VENDOR)), "Intel") ||
         CommandLine::ForCurrentProcess()->HasSwitch(
             switches::kForceGLFinishWorkaround)) &&
        !CommandLine::ForCurrentProcess()->HasSwitch(
            switches::kDisableGpuDriverBugWorkarounds);

    initialized_workaround = true;
  }

  if (use_glfinish_workaround) {
    // http://crbug.com/123409 : work around bugs in graphics driver on
    // MacBook Air with Intel HD graphics, and possibly on other models,
    // by forcing the graphics pipeline to be completely drained at this
    // point.
    glFinish();
  }

  CGLFlushDrawable(cglContext_);

  // For latency_tests.cc:
  UNSHIPPED_TRACE_EVENT_INSTANT0("test_gpu", "CompositorSwapBuffersComplete");

  CGLSetCurrentContext(0);

  StartOrContinueDisplayLink();

  if (!is_vsync_disabled_)
    RateLimitDraws();
}

bool CompositingIOSurfaceMac::CopyTo(
      const gfx::Rect& src_pixel_subrect,
      const gfx::Size& dst_pixel_size,
      void* out) {
  if (!MapIOSurfaceToTexture(io_surface_handle_))
    return false;

  CGLSetCurrentContext(cglContext_);
  GLuint target = GL_TEXTURE_RECTANGLE_ARB;

  GLuint dst_texture = 0;
  glGenTextures(1, &dst_texture); CHECK_GL_ERROR();
  glBindTexture(target, dst_texture); CHECK_GL_ERROR();

  GLuint dst_framebuffer = 0;
  glGenFramebuffersEXT(1, &dst_framebuffer); CHECK_GL_ERROR();
  glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, dst_framebuffer); CHECK_GL_ERROR();

  glTexImage2D(target,
               0,
               GL_RGBA,
               dst_pixel_size.width(),
               dst_pixel_size.height(),
               0,
               GL_BGRA,
               GL_UNSIGNED_INT_8_8_8_8_REV,
               NULL); CHECK_GL_ERROR();
  glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT,
                            GL_COLOR_ATTACHMENT0_EXT,
                            target,
                            dst_texture,
                            0); CHECK_GL_ERROR();
  glBindTexture(target, 0); CHECK_GL_ERROR();

  glViewport(0, 0, dst_pixel_size.width(), dst_pixel_size.height());

  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glOrtho(0, dst_pixel_size.width(), 0, dst_pixel_size.height(), -1, 1);
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();

  glDisable(GL_DEPTH_TEST);
  glDisable(GL_BLEND);

  glUseProgram(shader_program_blit_rgb_);

  int texture_unit = 0;
  glUniform1i(blit_rgb_sampler_location_, texture_unit);
  glActiveTexture(GL_TEXTURE0 + texture_unit);
  glBindTexture(GL_TEXTURE_RECTANGLE_ARB, texture_);

  SurfaceQuad quad;
  quad.set_rect(0.0f, 0.0f, dst_pixel_size.width(), dst_pixel_size.height());
  quad.set_texcoord_rect(src_pixel_subrect.x(), src_pixel_subrect.y(),
                         src_pixel_subrect.right(), src_pixel_subrect.bottom());
  DrawQuad(quad);

  glBindTexture(GL_TEXTURE_RECTANGLE_ARB, 0); CHECK_GL_ERROR();
  glUseProgram(0);

  CGLFlushDrawable(cglContext_);

  glReadPixels(0, 0, dst_pixel_size.width(), dst_pixel_size.height(),
               GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, out);

  glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0); CHECK_GL_ERROR();

  glDeleteFramebuffersEXT(1, &dst_framebuffer);
  glDeleteTextures(1, &dst_texture);

  CGLSetCurrentContext(0);
  return true;
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
      cglContext_,
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
  CGLSetCurrentContext(cglContext_);
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
  [glContext_ update];
}

void CompositingIOSurfaceMac::ClearDrawable() {
  [glContext_ clearDrawable];
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

}  // namespace content
