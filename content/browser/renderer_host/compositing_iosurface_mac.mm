// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/compositing_iosurface_mac.h"

#include <OpenGL/OpenGL.h>
#include <vector>

#include "base/command_line.h"
#include "base/debug/trace_event.h"
#include "content/browser/renderer_host/render_widget_host_view_mac.h"
#include "content/public/browser/browser_thread.h"
#include "gpu/command_buffer/service/gpu_switches.h"
#include "ui/gfx/gl/gl_context.h"
#include "ui/gfx/gl/gl_switches.h"
#include "ui/gfx/scoped_ns_graphics_context_save_gstate_mac.h"
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

namespace {

static const char* g_vertex_shader_blit_rgb = SHADER_STRING_GLSL(
    varying vec2 texture_coordinate;
    void main() {
      gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;
      texture_coordinate = vec2(gl_MultiTexCoord0);
    });

static const char* g_fragment_shader_blit_rgb = SHADER_STRING_GLSL(
    varying vec2 texture_coordinate;
    uniform sampler2DRect texture;
    void main() {
      gl_FragColor = vec4(texture2DRect(texture, texture_coordinate).rgb, 1.0);
    });

static const char* g_vertex_shader_white = SHADER_STRING_GLSL(
    void main() {
      gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;
    });

static const char* g_fragment_shader_white = SHADER_STRING_GLSL(
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
  GLint swapInterval;
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kDisableGpuVsync))
    swapInterval = 0;
  else
    swapInterval = 1;
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

  return new CompositingIOSurfaceMac(io_surface_support, glContext.release(),
                                     cglContext,
                                     shader_program_blit_rgb,
                                     blit_rgb_sampler_location,
                                     shader_program_white);
}

CompositingIOSurfaceMac::CompositingIOSurfaceMac(
    IOSurfaceSupport* io_surface_support,
    NSOpenGLContext* glContext,
    CGLContextObj cglContext,
    GLuint shader_program_blit_rgb,
    GLint blit_rgb_sampler_location,
    GLuint shader_program_white)
    : io_surface_support_(io_surface_support),
      glContext_(glContext),
      cglContext_(cglContext),
      io_surface_handle_(0),
      texture_(0),
      shader_program_blit_rgb_(shader_program_blit_rgb),
      blit_rgb_sampler_location_(blit_rgb_sampler_location),
      shader_program_white_(shader_program_white) {
}

CompositingIOSurfaceMac::~CompositingIOSurfaceMac() {
  UnrefIOSurface();
}

void CompositingIOSurfaceMac::SetIOSurface(uint64 io_surface_handle) {
  CGLSetCurrentContext(cglContext_);
  MapIOSurfaceToTexture(io_surface_handle);
  CGLSetCurrentContext(0);
}

void CompositingIOSurfaceMac::DrawIOSurface(NSView* view) {
  CGLSetCurrentContext(cglContext_);

  bool has_io_surface = MapIOSurfaceToTexture(io_surface_handle_);

  TRACE_EVENT1("browser", "CompositingIOSurfaceMac::DrawIOSurface",
               "has_io_surface", has_io_surface);

  [glContext_ setView:view];
  gfx::Size window_size([view frame].size.width, [view frame].size.height);
  glViewport(0, 0, window_size.width(), window_size.height());

  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
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

  CGLSetCurrentContext(0);
}

bool CompositingIOSurfaceMac::CopyTo(const gfx::Size& dst_size, void* out) {
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
               dst_size.width(),
               dst_size.height(),
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

  glViewport(0, 0, dst_size.width(), dst_size.height());

  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glOrtho(0, dst_size.width(), 0, dst_size.height(), -1, 1);
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
  quad.set_size(dst_size, io_surface_size_);
  DrawQuad(quad);

  glBindTexture(GL_TEXTURE_RECTANGLE_ARB, 0); CHECK_GL_ERROR();
  glUseProgram(0);

  CGLFlushDrawable(cglContext_);

  glReadPixels(0, 0, dst_size.width(), dst_size.height(),
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
  io_surface_size_.SetSize(
      io_surface_support_->IOSurfaceGetWidth(io_surface_),
      io_surface_support_->IOSurfaceGetHeight(io_surface_));

  quad_.set_size(io_surface_size_, io_surface_size_);

  GLenum target = GL_TEXTURE_RECTANGLE_ARB;
  glGenTextures(1, &texture_);
  glBindTexture(target, texture_);
  glTexParameterf(target, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameterf(target, GL_TEXTURE_MAG_FILTER, GL_NEAREST); CHECK_GL_ERROR();
  GLuint plane = 0;
  CGLError cglerror =
      io_surface_support_->CGLTexImageIOSurface2D(cglContext_,
                                                  target,
                                                  GL_RGBA,
                                                  io_surface_size_.width(),
                                                  io_surface_size_.height(),
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
