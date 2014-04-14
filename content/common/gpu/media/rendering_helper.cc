// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/gpu/media/rendering_helper.h"

#include "base/bind.h"
#include "base/mac/scoped_nsautorelease_pool.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/stringize_macros.h"
#include "base/synchronization/waitable_event.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_context_stub_with_extensions.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_surface.h"

#if defined(USE_X11)
#include "ui/gfx/x/x11_types.h"
#endif

#ifdef GL_VARIANT_GLX
typedef GLXWindow NativeWindowType;
struct XFreeDeleter {
  void operator()(void* x) const { ::XFree(x); }
};
#else  // EGL
typedef EGLNativeWindowType NativeWindowType;
#endif

// Helper for Shader creation.
static void CreateShader(GLuint program,
                         GLenum type,
                         const char* source,
                         int size) {
  GLuint shader = glCreateShader(type);
  glShaderSource(shader, 1, &source, &size);
  glCompileShader(shader);
  int result = GL_FALSE;
  glGetShaderiv(shader, GL_COMPILE_STATUS, &result);
  if (!result) {
    char log[4096];
    glGetShaderInfoLog(shader, arraysize(log), NULL, log);
    LOG(FATAL) << log;
  }
  glAttachShader(program, shader);
  glDeleteShader(shader);
  CHECK_EQ(static_cast<int>(glGetError()), GL_NO_ERROR);
}

namespace content {

RenderingHelperParams::RenderingHelperParams() {}

RenderingHelperParams::~RenderingHelperParams() {}

static const gfx::GLImplementation kGLImplementation =
#if defined(GL_VARIANT_GLX)
    gfx::kGLImplementationDesktopGL;
#elif defined(GL_VARIANT_EGL)
    gfx::kGLImplementationEGLGLES2;
#else
    -1;
#error "Unknown GL implementation."
#endif

RenderingHelper::RenderingHelper() {
  Clear();
}

RenderingHelper::~RenderingHelper() {
  CHECK_EQ(window_dimensions_.size(), 0U) <<
    "Must call UnInitialize before dtor.";
  Clear();
}

void RenderingHelper::MakeCurrent(int window_id) {
#if GL_VARIANT_GLX
  if (window_id < 0) {
    CHECK(glXMakeContextCurrent(x_display_, GLX_NONE, GLX_NONE, NULL));
  } else {
    CHECK(glXMakeContextCurrent(
        x_display_, x_windows_[window_id], x_windows_[window_id], gl_context_));
  }
#else  // EGL
  if (window_id < 0) {
    CHECK(eglMakeCurrent(gl_display_, EGL_NO_SURFACE, EGL_NO_SURFACE,
                         EGL_NO_CONTEXT)) << eglGetError();
  } else {
    CHECK(eglMakeCurrent(gl_display_, gl_surfaces_[window_id],
                         gl_surfaces_[window_id], gl_context_))
        << eglGetError();
  }
#endif
}

void RenderingHelper::Initialize(const RenderingHelperParams& params,
                                 base::WaitableEvent* done) {
  // Use window_dimensions_.size() != 0 as a proxy for the class having already
  // been Initialize()'d, and UnInitialize() before continuing.
  if (window_dimensions_.size()) {
    base::WaitableEvent done(false, false);
    UnInitialize(&done);
    done.Wait();
  }

  gfx::InitializeStaticGLBindings(kGLImplementation);
  scoped_refptr<gfx::GLContextStubWithExtensions> stub_context(
      new gfx::GLContextStubWithExtensions());

  CHECK_GT(params.window_dimensions.size(), 0U);
  CHECK_EQ(params.frame_dimensions.size(), params.window_dimensions.size());
  window_dimensions_ = params.window_dimensions;
  frame_dimensions_ = params.frame_dimensions;
  render_as_thumbnails_ = params.render_as_thumbnails;
  message_loop_ = base::MessageLoop::current();
  CHECK_GT(params.num_windows, 0);

#if GL_VARIANT_GLX
  x_display_ = gfx::GetXDisplay();
  CHECK(x_display_);
  CHECK(glXQueryVersion(x_display_, NULL, NULL));
  const int fbconfig_attr[] = {
    GLX_DRAWABLE_TYPE, GLX_WINDOW_BIT,
    GLX_RENDER_TYPE, GLX_RGBA_BIT,
    GLX_BIND_TO_TEXTURE_TARGETS_EXT, GLX_TEXTURE_2D_BIT_EXT,
    GLX_BIND_TO_TEXTURE_RGB_EXT, GL_TRUE,
    GLX_DOUBLEBUFFER, True,
    GL_NONE,
  };
  int num_fbconfigs;
  scoped_ptr<GLXFBConfig, XFreeDeleter> glx_fb_configs(
      glXChooseFBConfig(x_display_, DefaultScreen(x_display_), fbconfig_attr,
                        &num_fbconfigs));
  CHECK(glx_fb_configs.get());
  CHECK_GT(num_fbconfigs, 0);
  x_visual_ = glXGetVisualFromFBConfig(x_display_, glx_fb_configs.get()[0]);
  CHECK(x_visual_);
  gl_context_ = glXCreateContext(x_display_, x_visual_, 0, true);
  CHECK(gl_context_);
  stub_context->AddExtensionsString(
      reinterpret_cast<const char*>(glGetString(GL_EXTENSIONS)));
  stub_context->SetGLVersionString(
      reinterpret_cast<const char*>(glGetString(GL_VERSION)));

#else // EGL
  EGLNativeDisplayType native_display;

#if defined(OS_WIN)
  native_display = EGL_DEFAULT_DISPLAY;
#else
  x_display_ = gfx::GetXDisplay();
  CHECK(x_display_);
  native_display = x_display_;
#endif

  gl_display_ = eglGetDisplay(native_display);
  CHECK(gl_display_);
  CHECK(eglInitialize(gl_display_, NULL, NULL)) << glGetError();

  static EGLint rgba8888[] = {
    EGL_RED_SIZE, 8,
    EGL_GREEN_SIZE, 8,
    EGL_BLUE_SIZE, 8,
    EGL_ALPHA_SIZE, 8,
    EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
    EGL_NONE,
  };
  EGLConfig egl_config;
  int num_configs;
  CHECK(eglChooseConfig(gl_display_, rgba8888, &egl_config, 1, &num_configs))
      << eglGetError();
  CHECK_GE(num_configs, 1);
  static EGLint context_attribs[] = {EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE};
  gl_context_ = eglCreateContext(
      gl_display_, egl_config, EGL_NO_CONTEXT, context_attribs);
  CHECK_NE(gl_context_, EGL_NO_CONTEXT) << eglGetError();
  stub_context->AddExtensionsString(
      reinterpret_cast<const char*>(glGetString(GL_EXTENSIONS)));
  stub_context->AddExtensionsString(
      eglQueryString(gl_display_, EGL_EXTENSIONS));
  stub_context->SetGLVersionString(
      reinterpret_cast<const char*>(glGetString(GL_VERSION)));
#endif

  // Per-window/surface X11 & EGL initialization.
  for (int i = 0; i < params.num_windows; ++i) {
    // Arrange X windows whimsically, with some padding.
    int j = i % window_dimensions_.size();
    int width = window_dimensions_[j].width();
    int height = window_dimensions_[j].height();
    CHECK_GT(width, 0);
    CHECK_GT(height, 0);
    int top_left_x = (width + 20) * (i % 4);
    int top_left_y = (height + 12) * (i % 3);

#if defined(OS_WIN)
    NativeWindowType window =
        CreateWindowEx(0, L"Static", L"VideoDecodeAcceleratorTest",
                       WS_OVERLAPPEDWINDOW | WS_VISIBLE, top_left_x,
                       top_left_y, width, height, NULL, NULL, NULL,
                       NULL);
    CHECK(window != NULL);
    windows_.push_back(window);
#else
    int depth = DefaultDepth(x_display_, DefaultScreen(x_display_));

#if defined(GL_VARIANT_GLX)
    CHECK_EQ(depth, x_visual_->depth);
#endif

    XSetWindowAttributes window_attributes;
    window_attributes.background_pixel =
        BlackPixel(x_display_, DefaultScreen(x_display_));
    window_attributes.override_redirect = true;

    NativeWindowType window = XCreateWindow(
        x_display_, DefaultRootWindow(x_display_),
        top_left_x, top_left_y, width, height,
        0 /* border width */,
        depth, CopyFromParent /* class */, CopyFromParent /* visual */,
        (CWBackPixel | CWOverrideRedirect), &window_attributes);
    XStoreName(x_display_, window, "VideoDecodeAcceleratorTest");
    XSelectInput(x_display_, window, ExposureMask);
    XMapWindow(x_display_, window);
    x_windows_.push_back(window);
#endif

#if GL_VARIANT_EGL
    EGLSurface egl_surface =
        eglCreateWindowSurface(gl_display_, egl_config, window, NULL);
    gl_surfaces_.push_back(egl_surface);
    CHECK_NE(egl_surface, EGL_NO_SURFACE);
#endif
    MakeCurrent(i);
  }

  // Must be done after a context is made current.
  gfx::InitializeDynamicGLBindings(kGLImplementation, stub_context.get());

  if (render_as_thumbnails_) {
    CHECK_EQ(window_dimensions_.size(), 1U);

    GLint max_texture_size;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &max_texture_size);
    CHECK_GE(max_texture_size, params.thumbnails_page_size.width());
    CHECK_GE(max_texture_size, params.thumbnails_page_size.height());

    thumbnails_fbo_size_ = params.thumbnails_page_size;
    thumbnail_size_ = params.thumbnail_size;

    glGenFramebuffersEXT(1, &thumbnails_fbo_id_);
    glGenTextures(1, &thumbnails_texture_id_);
    glBindTexture(GL_TEXTURE_2D, thumbnails_texture_id_);
    glTexImage2D(GL_TEXTURE_2D,
                 0,
                 GL_RGB,
                 thumbnails_fbo_size_.width(),
                 thumbnails_fbo_size_.height(),
                 0,
                 GL_RGB,
                 GL_UNSIGNED_SHORT_5_6_5,
                 NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

    glBindFramebufferEXT(GL_FRAMEBUFFER, thumbnails_fbo_id_);
    glFramebufferTexture2DEXT(GL_FRAMEBUFFER,
                              GL_COLOR_ATTACHMENT0,
                              GL_TEXTURE_2D,
                              thumbnails_texture_id_,
                              0);

    GLenum fb_status = glCheckFramebufferStatusEXT(GL_FRAMEBUFFER);
    CHECK(fb_status == GL_FRAMEBUFFER_COMPLETE) << fb_status;
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glBindFramebufferEXT(GL_FRAMEBUFFER, 0);
  }

  // These vertices and texture coords. map (0,0) in the texture to the
  // bottom left of the viewport.  Since we get the video frames with the
  // the top left at (0,0) we need to flip the texture y coordinate
  // in the vertex shader for this to be rendered the right way up.
  // In the case of thumbnail rendering we use the same vertex shader
  // to render the FBO the screen, where we do not want this flipping.
  static const float kVertices[] =
      { -1.f, 1.f, -1.f, -1.f, 1.f, 1.f, 1.f, -1.f, };
  static const float kTextureCoords[] = { 0, 1, 0, 0, 1, 1, 1, 0, };
  static const char kVertexShader[] = STRINGIZE(
      varying vec2 interp_tc;
      attribute vec4 in_pos;
      attribute vec2 in_tc;
      uniform bool tex_flip;
      void main() {
        if (tex_flip)
          interp_tc = vec2(in_tc.x, 1.0 - in_tc.y);
        else
          interp_tc = in_tc;
        gl_Position = in_pos;
      });

#if GL_VARIANT_EGL
  static const char kFragmentShader[] =
      "#extension GL_OES_EGL_image_external : enable\n"
      "precision mediump float;\n"
      "varying vec2 interp_tc;\n"
      "uniform sampler2D tex;\n"
      "#ifdef GL_OES_EGL_image_external\n"
      "uniform samplerExternalOES tex_external;\n"
      "#endif\n"
      "void main() {\n"
      "  vec4 color = texture2D(tex, interp_tc);\n"
      "#ifdef GL_OES_EGL_image_external\n"
      "  color += texture2D(tex_external, interp_tc);\n"
      "#endif\n"
      "  gl_FragColor = color;\n"
      "}\n";
#else
  static const char kFragmentShader[] = STRINGIZE(
      varying vec2 interp_tc;
      uniform sampler2D tex;
      void main() {
        gl_FragColor = texture2D(tex, interp_tc);
      });
#endif
  program_ = glCreateProgram();
  CreateShader(
      program_, GL_VERTEX_SHADER, kVertexShader, arraysize(kVertexShader));
  CreateShader(program_,
               GL_FRAGMENT_SHADER,
               kFragmentShader,
               arraysize(kFragmentShader));
  glLinkProgram(program_);
  int result = GL_FALSE;
  glGetProgramiv(program_, GL_LINK_STATUS, &result);
  if (!result) {
    char log[4096];
    glGetShaderInfoLog(program_, arraysize(log), NULL, log);
    LOG(FATAL) << log;
  }
  glUseProgram(program_);
  glDeleteProgram(program_);

  glUniform1i(glGetUniformLocation(program_, "tex_flip"), 0);
  glUniform1i(glGetUniformLocation(program_, "tex"), 0);
  GLint tex_external = glGetUniformLocation(program_, "tex_external");
  if (tex_external != -1) {
    glUniform1i(tex_external, 1);
  }
  int pos_location = glGetAttribLocation(program_, "in_pos");
  glEnableVertexAttribArray(pos_location);
  glVertexAttribPointer(pos_location, 2, GL_FLOAT, GL_FALSE, 0, kVertices);
  int tc_location = glGetAttribLocation(program_, "in_tc");
  glEnableVertexAttribArray(tc_location);
  glVertexAttribPointer(tc_location, 2, GL_FLOAT, GL_FALSE, 0, kTextureCoords);
  done->Signal();
}

void RenderingHelper::UnInitialize(base::WaitableEvent* done) {
  CHECK_EQ(base::MessageLoop::current(), message_loop_);
  if (render_as_thumbnails_) {
    glDeleteTextures(1, &thumbnails_texture_id_);
    glDeleteFramebuffersEXT(1, &thumbnails_fbo_id_);
  }
#if GL_VARIANT_GLX

  glXDestroyContext(x_display_, gl_context_);
#else // EGL
  MakeCurrent(-1);
  CHECK(eglDestroyContext(gl_display_, gl_context_));
  for (size_t i = 0; i < gl_surfaces_.size(); ++i)
    CHECK(eglDestroySurface(gl_display_, gl_surfaces_[i]));
  CHECK(eglTerminate(gl_display_));
#endif
  gfx::ClearGLBindings();
  Clear();
  done->Signal();
}

void RenderingHelper::CreateTexture(int window_id,
                                    uint32 texture_target,
                                    uint32* texture_id,
                                    base::WaitableEvent* done) {
  if (base::MessageLoop::current() != message_loop_) {
    message_loop_->PostTask(
        FROM_HERE,
        base::Bind(&RenderingHelper::CreateTexture, base::Unretained(this),
                   window_id, texture_target, texture_id, done));
    return;
  }
  MakeCurrent(window_id);
  glGenTextures(1, texture_id);
  glBindTexture(texture_target, *texture_id);
  int dimensions_id = window_id % frame_dimensions_.size();
  if (texture_target == GL_TEXTURE_2D) {
    glTexImage2D(GL_TEXTURE_2D,
                 0,
                 GL_RGBA,
                 frame_dimensions_[dimensions_id].width(),
                 frame_dimensions_[dimensions_id].height(),
                 0,
                 GL_RGBA,
                 GL_UNSIGNED_BYTE,
                 NULL);
  }
  glTexParameteri(texture_target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(texture_target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  // OpenGLES2.0.25 section 3.8.2 requires CLAMP_TO_EDGE for NPOT textures.
  glTexParameteri(texture_target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(texture_target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  CHECK_EQ(static_cast<int>(glGetError()), GL_NO_ERROR);
  CHECK(texture_id_to_surface_index_.insert(
      std::make_pair(*texture_id, window_id)).second);
  done->Signal();
}

void RenderingHelper::RenderTexture(uint32 texture_target, uint32 texture_id) {
  CHECK_EQ(base::MessageLoop::current(), message_loop_);
  size_t window_id = texture_id_to_surface_index_[texture_id];
  MakeCurrent(window_id);

  int dimensions_id = window_id % window_dimensions_.size();
  int width = window_dimensions_[dimensions_id].width();
  int height = window_dimensions_[dimensions_id].height();

  if (render_as_thumbnails_) {
    glBindFramebufferEXT(GL_FRAMEBUFFER, thumbnails_fbo_id_);
    const int thumbnails_in_row =
        thumbnails_fbo_size_.width() / thumbnail_size_.width();
    const int thumbnails_in_column =
        thumbnails_fbo_size_.height() / thumbnail_size_.height();
    const int row = (frame_count_ / thumbnails_in_row) % thumbnails_in_column;
    const int col = frame_count_ % thumbnails_in_row;
    const int x = col * thumbnail_size_.width();
    const int y = row * thumbnail_size_.height();

    glViewport(x, y, thumbnail_size_.width(), thumbnail_size_.height());
    glScissor(x, y, thumbnail_size_.width(), thumbnail_size_.height());
    glUniform1i(glGetUniformLocation(program_, "tex_flip"), 0);
  } else {
    glViewport(0, 0, width, height);
    glScissor(0, 0, width, height);
    glUniform1i(glGetUniformLocation(program_, "tex_flip"), 1);
  }

  // Unbound texture samplers default to (0, 0, 0, 1).  Use this fact to switch
  // between GL_TEXTURE_2D and GL_TEXTURE_EXTERNAL_OES as appopriate.
  if (texture_target == GL_TEXTURE_2D) {
    glActiveTexture(GL_TEXTURE0 + 0);
    glBindTexture(GL_TEXTURE_2D, texture_id);
    glActiveTexture(GL_TEXTURE0 + 1);
    glBindTexture(texture_target, 0);
  } else if (texture_target == GL_TEXTURE_EXTERNAL_OES) {
    glActiveTexture(GL_TEXTURE0 + 0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glActiveTexture(GL_TEXTURE0 + 1);
    glBindTexture(texture_target, texture_id);
  }
  glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
  CHECK_EQ(static_cast<int>(glGetError()), GL_NO_ERROR);

  ++frame_count_;

  if (render_as_thumbnails_) {
    // Copy from FBO to screen
    glUniform1i(glGetUniformLocation(program_, "tex_flip"), 1);
    glBindFramebufferEXT(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, width, height);
    glScissor(0, 0, width, height);
    glActiveTexture(GL_TEXTURE0 + 0);
    glBindTexture(GL_TEXTURE_2D, thumbnails_texture_id_);
    glActiveTexture(GL_TEXTURE0 + 1);
    glBindTexture(texture_target, 0);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
  }

#if GL_VARIANT_GLX
  glXSwapBuffers(x_display_, x_windows_[window_id]);
#else  // EGL
  eglSwapBuffers(gl_display_, gl_surfaces_[window_id]);
  CHECK_EQ(static_cast<int>(eglGetError()), EGL_SUCCESS);
#endif
}

void RenderingHelper::DeleteTexture(uint32 texture_id) {
  glDeleteTextures(1, &texture_id);
  CHECK_EQ(static_cast<int>(glGetError()), GL_NO_ERROR);
}

NativeContextType RenderingHelper::GetGLContext() { return gl_context_; }

void* RenderingHelper::GetGLDisplay() {
#if GL_VARIANT_GLX
  return x_display_;
#else  // EGL
  return gl_display_;
#endif
}

void RenderingHelper::Clear() {
  window_dimensions_.clear();
  frame_dimensions_.clear();
  texture_id_to_surface_index_.clear();
  message_loop_ = NULL;
  gl_context_ = NULL;
#if GL_VARIANT_EGL
  gl_display_ = EGL_NO_DISPLAY;
  gl_surfaces_.clear();
#endif
  render_as_thumbnails_ = false;
  frame_count_ = 0;
  thumbnails_fbo_id_ = 0;
  thumbnails_texture_id_ = 0;

#if defined(OS_WIN)
  for (size_t i = 0; i < windows_.size(); ++i) {
    DestroyWindow(windows_[i]);
  }
  windows_.clear();
#else
  // Destroy resources acquired in Initialize, in reverse-acquisition order.
  for (size_t i = 0; i < x_windows_.size(); ++i) {
    CHECK(XUnmapWindow(x_display_, x_windows_[i]));
    CHECK(XDestroyWindow(x_display_, x_windows_[i]));
  }
  // Mimic newly created object.
  x_display_ = NULL;
  x_windows_.clear();
#endif
}

void RenderingHelper::GetThumbnailsAsRGB(std::vector<unsigned char>* rgb,
                                         bool* alpha_solid,
                                         base::WaitableEvent* done) {
  CHECK(render_as_thumbnails_);

  const size_t num_pixels = thumbnails_fbo_size_.GetArea();
  std::vector<unsigned char> rgba;
  rgba.resize(num_pixels * 4);
  glBindFramebufferEXT(GL_FRAMEBUFFER, thumbnails_fbo_id_);
  glPixelStorei(GL_PACK_ALIGNMENT, 1);
  // We can only count on GL_RGBA/GL_UNSIGNED_BYTE support.
  glReadPixels(0,
               0,
               thumbnails_fbo_size_.width(),
               thumbnails_fbo_size_.height(),
               GL_RGBA,
               GL_UNSIGNED_BYTE,
               &rgba[0]);
  glBindFramebufferEXT(GL_FRAMEBUFFER, 0);
  rgb->resize(num_pixels * 3);
  // Drop the alpha channel, but check as we go that it is all 0xff.
  bool solid = true;
  unsigned char* rgb_ptr = &((*rgb)[0]);
  unsigned char* rgba_ptr = &rgba[0];
  for (size_t i = 0; i < num_pixels; ++i) {
    *rgb_ptr++ = *rgba_ptr++;
    *rgb_ptr++ = *rgba_ptr++;
    *rgb_ptr++ = *rgba_ptr++;
    solid = solid && (*rgba_ptr == 0xff);
    rgba_ptr++;
  }
  *alpha_solid = solid;

  done->Signal();
}

}  // namespace content
