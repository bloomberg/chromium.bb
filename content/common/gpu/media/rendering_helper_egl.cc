// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/gpu/media/rendering_helper.h"

#include <map>

#include "base/bind.h"
#include "base/mac/scoped_nsautorelease_pool.h"
#include "base/message_loop.h"
#include "base/stringize_macros.h"
#include "base/synchronization/waitable_event.h"
#include "third_party/angle/include/EGL/egl.h"

#if defined(OS_WIN)
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_surface.h"
#else  // OS_WIN
#include "third_party/angle/include/GLES2/gl2.h"
#endif  // OS_WIN

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

namespace video_test_util {

class RenderingHelperEGL : public RenderingHelper {
 public:
  RenderingHelperEGL();
  virtual ~RenderingHelperEGL();

  // Implement RenderingHelper.
  virtual void Initialize(bool suppress_swap_to_display,
                          int num_windows,
                          int width,
                          int height,
                          base::WaitableEvent* done) OVERRIDE;
  virtual void UnInitialize(base::WaitableEvent* done) OVERRIDE;
  virtual void CreateTexture(int window_id,
                             uint32 texture_target,
                             uint32* texture_id,
                             base::WaitableEvent* done) OVERRIDE;
  virtual void RenderTexture(uint32 texture_id) OVERRIDE;
  virtual void DeleteTexture(uint32 texture_id) OVERRIDE;
  virtual void* GetGLContext() OVERRIDE;
  virtual void* GetGLDisplay() OVERRIDE;

 private:
  void Clear();

  // Platform specific Init/Uninit.
  void PlatformInitialize();
  void PlatformUnInitialize();

  // Platform specific window creation.
  EGLNativeWindowType PlatformCreateWindow(int top_left_x, int top_left_y);

  // Platform specific display surface returned here.
  EGLDisplay PlatformGetDisplay();

  MessageLoop* message_loop_;
  int width_;
  int height_;
  bool suppress_swap_to_display_;

  EGLDisplay egl_display_;
  EGLContext egl_context_;
  std::vector<EGLSurface> egl_surfaces_;
  std::map<uint32, int> texture_id_to_surface_index_;

#if defined(OS_WIN)
  std::vector<HWND> windows_;
#else  // OS_WIN
  Display* x_display_;
  std::vector<Window> x_windows_;
#endif  // OS_WIN
};

// static
RenderingHelper* RenderingHelper::Create() {
  return new RenderingHelperEGL;
}

// static
void RenderingHelper::InitializePlatform() {
#if defined(OS_WIN)
  gfx::InitializeGLBindings(gfx::kGLImplementationEGLGLES2);
  gfx::GLSurface::InitializeOneOff();
  {
    // Hack to ensure that EGL extension function pointers are initialized.
    scoped_refptr<gfx::GLSurface> surface(
        gfx::GLSurface::CreateOffscreenGLSurface(false, gfx::Size(1, 1)));
    scoped_refptr<gfx::GLContext> context(
        gfx::GLContext::CreateGLContext(NULL, surface.get(),
                                        gfx::PreferIntegratedGpu));
    context->MakeCurrent(surface.get());
  }
#endif  // OS_WIN
}

RenderingHelperEGL::RenderingHelperEGL() {
  Clear();
}

RenderingHelperEGL::~RenderingHelperEGL() {
  CHECK_EQ(width_, 0) << "Must call UnInitialize before dtor.";
  Clear();
}

void RenderingHelperEGL::Initialize(bool suppress_swap_to_display,
                                    int num_windows,
                                    int width,
                                    int height,
                                    base::WaitableEvent* done) {
  // Use width_ != 0 as a proxy for the class having already been
  // Initialize()'d, and UnInitialize() before continuing.
  if (width_) {
    base::WaitableEvent done(false, false);
    UnInitialize(&done);
    done.Wait();
  }

  suppress_swap_to_display_ = suppress_swap_to_display;
  CHECK_GT(width, 0);
  CHECK_GT(height, 0);
  width_ = width;
  height_ = height;
  message_loop_ = MessageLoop::current();
  CHECK_GT(num_windows, 0);

  PlatformInitialize();

  egl_display_ = PlatformGetDisplay();

  EGLint major;
  EGLint minor;
  CHECK(eglInitialize(egl_display_, &major, &minor)) << eglGetError();
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
  CHECK(eglChooseConfig(egl_display_, rgba8888, &egl_config, 1, &num_configs))
      << eglGetError();
  CHECK_GE(num_configs, 1);
  static EGLint context_attribs[] = {EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE};
  egl_context_ = eglCreateContext(
      egl_display_, egl_config, EGL_NO_CONTEXT, context_attribs);
  CHECK_NE(egl_context_, EGL_NO_CONTEXT) << eglGetError();

  // Per-window/surface X11 & EGL initialization.
  for (int i = 0; i < num_windows; ++i) {
    // Arrange X windows whimsically, with some padding.
    int top_left_x = (width + 20) * (i % 4);
    int top_left_y = (height + 12) * (i % 3);

    EGLNativeWindowType window = PlatformCreateWindow(top_left_x, top_left_y);
    EGLSurface egl_surface =
        eglCreateWindowSurface(egl_display_, egl_config, window, NULL);
    egl_surfaces_.push_back(egl_surface);
    CHECK_NE(egl_surface, EGL_NO_SURFACE);
  }
  CHECK(eglMakeCurrent(egl_display_, egl_surfaces_[0],
                       egl_surfaces_[0], egl_context_)) << eglGetError();

  static const float kVertices[] =
      { -1.f, 1.f, -1.f, -1.f, 1.f, 1.f, 1.f, -1.f, };
  static const float kTextureCoordsEgl[] = { 0, 1, 0, 0, 1, 1, 1, 0, };
  static const char kVertexShader[] = STRINGIZE(
      varying vec2 interp_tc;
      attribute vec4 in_pos;
      attribute vec2 in_tc;
      void main() {
        interp_tc = in_tc;
        gl_Position = in_pos;
      });
  static const char kFragmentShaderEgl[] = STRINGIZE(
      precision mediump float;
      varying vec2 interp_tc;
      uniform sampler2D tex;
      void main() {
        gl_FragColor = texture2D(tex, interp_tc);
      });
  GLuint program = glCreateProgram();
  CreateShader(program, GL_VERTEX_SHADER,
               kVertexShader, arraysize(kVertexShader));
  CreateShader(program, GL_FRAGMENT_SHADER,
               kFragmentShaderEgl, arraysize(kFragmentShaderEgl));
  glLinkProgram(program);
  int result = GL_FALSE;
  glGetProgramiv(program, GL_LINK_STATUS, &result);
  if (!result) {
    char log[4096];
    glGetShaderInfoLog(program, arraysize(log), NULL, log);
    LOG(FATAL) << log;
  }
  glUseProgram(program);
  glDeleteProgram(program);

  glUniform1i(glGetUniformLocation(program, "tex"), 0);
  int pos_location = glGetAttribLocation(program, "in_pos");
  glEnableVertexAttribArray(pos_location);
  glVertexAttribPointer(pos_location, 2, GL_FLOAT, GL_FALSE, 0, kVertices);
  int tc_location = glGetAttribLocation(program, "in_tc");
  glEnableVertexAttribArray(tc_location);
  glVertexAttribPointer(tc_location, 2, GL_FLOAT, GL_FALSE, 0,
                        kTextureCoordsEgl);
  done->Signal();
}

void RenderingHelperEGL::UnInitialize(base::WaitableEvent* done) {
  CHECK_EQ(MessageLoop::current(), message_loop_);
  CHECK(eglMakeCurrent(egl_display_, EGL_NO_SURFACE, EGL_NO_SURFACE,
                       EGL_NO_CONTEXT)) << eglGetError();
  CHECK(eglDestroyContext(egl_display_, egl_context_));
  for (size_t i = 0; i < egl_surfaces_.size(); ++i)
    CHECK(eglDestroySurface(egl_display_, egl_surfaces_[i]));
  CHECK(eglTerminate(egl_display_));
  Clear();
  done->Signal();
}

void RenderingHelperEGL::CreateTexture(int window_id,
                                       uint32 texture_target,
                                       uint32* texture_id,
                                       base::WaitableEvent* done) {
  if (MessageLoop::current() != message_loop_) {
    message_loop_->PostTask(
        FROM_HERE,
        base::Bind(&RenderingHelper::CreateTexture, base::Unretained(this),
                   window_id, texture_target, texture_id, done));
    return;
  }
  CHECK_EQ(static_cast<uint32>(GL_TEXTURE_2D), texture_target);
  CHECK(eglMakeCurrent(egl_display_, egl_surfaces_[window_id],
                       egl_surfaces_[window_id], egl_context_))
      << eglGetError();
  glGenTextures(1, texture_id);
  glBindTexture(GL_TEXTURE_2D, *texture_id);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width_, height_, 0, GL_RGBA,
               GL_UNSIGNED_BYTE, NULL);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  // OpenGLES2.0.25 section 3.8.2 requires CLAMP_TO_EDGE for NPOT textures.
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  CHECK_EQ(static_cast<int>(glGetError()), GL_NO_ERROR);
  CHECK_EQ(static_cast<int>(eglGetError()), EGL_SUCCESS);
  CHECK(texture_id_to_surface_index_.insert(
      std::make_pair(*texture_id, window_id)).second);
  done->Signal();
}

void RenderingHelperEGL::RenderTexture(uint32 texture_id) {
  CHECK_EQ(MessageLoop::current(), message_loop_);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, texture_id);
  glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
  CHECK_EQ(static_cast<int>(glGetError()), GL_NO_ERROR);
  CHECK_EQ(static_cast<int>(eglGetError()), EGL_SUCCESS);
  if (!suppress_swap_to_display_) {
    int window_id = texture_id_to_surface_index_[texture_id];
    CHECK(eglMakeCurrent(egl_display_, egl_surfaces_[window_id],
                         egl_surfaces_[window_id], egl_context_))
        << eglGetError();
    eglSwapBuffers(egl_display_, egl_surfaces_[window_id]);
  }
  CHECK_EQ(static_cast<int>(eglGetError()), EGL_SUCCESS);
}

void RenderingHelperEGL::DeleteTexture(uint32 texture_id) {
  glDeleteTextures(1, &texture_id);
  CHECK_EQ(static_cast<int>(glGetError()), GL_NO_ERROR);
}

void* RenderingHelperEGL::GetGLContext() {
  return egl_context_;
}

void* RenderingHelperEGL::GetGLDisplay() {
  return egl_display_;
}

void RenderingHelperEGL::Clear() {
  suppress_swap_to_display_ = false;
  width_ = 0;
  height_ = 0;
  texture_id_to_surface_index_.clear();
  message_loop_ = NULL;
  egl_display_ = EGL_NO_DISPLAY;
  egl_context_ = EGL_NO_CONTEXT;
  egl_surfaces_.clear();
  PlatformUnInitialize();
}

#if defined(OS_WIN)
void RenderingHelperEGL::PlatformInitialize() {}

void RenderingHelperEGL::PlatformUnInitialize() {
  for (size_t i = 0; i < windows_.size(); ++i) {
    DestroyWindow(windows_[i]);
  }
  windows_.clear();
}

EGLNativeWindowType RenderingHelperEGL::PlatformCreateWindow(
    int top_left_x, int top_left_y) {
  HWND window = CreateWindowEx(0, L"Static", L"VideoDecodeAcceleratorTest",
                               WS_OVERLAPPEDWINDOW | WS_VISIBLE, top_left_x,
                               top_left_y, width_, height_, NULL, NULL, NULL,
                               NULL);
  CHECK(window != NULL);
  windows_.push_back(window);
  return window;
}

EGLDisplay RenderingHelperEGL::PlatformGetDisplay() {
  return eglGetDisplay(EGL_DEFAULT_DISPLAY);
}

#else  // OS_WIN

void RenderingHelperEGL::PlatformInitialize() {
  CHECK(x_display_ = base::MessagePumpForUI::GetDefaultXDisplay());
}

void RenderingHelperEGL::PlatformUnInitialize() {
  // Destroy resources acquired in Initialize, in reverse-acquisition order.
  for (size_t i = 0; i < x_windows_.size(); ++i) {
    CHECK(XUnmapWindow(x_display_, x_windows_[i]));
    CHECK(XDestroyWindow(x_display_, x_windows_[i]));
  }
  // Mimic newly created object.
  x_display_ = NULL;
  x_windows_.clear();
}

EGLDisplay RenderingHelperEGL::PlatformGetDisplay() {
  return eglGetDisplay(x_display_);
}

EGLNativeWindowType RenderingHelperEGL::PlatformCreateWindow(int top_left_x,
                                                             int top_left_y) {
  int depth = DefaultDepth(x_display_, DefaultScreen(x_display_));

  XSetWindowAttributes window_attributes;
  window_attributes.background_pixel =
      BlackPixel(x_display_, DefaultScreen(x_display_));
  window_attributes.override_redirect = true;

  Window x_window = XCreateWindow(
      x_display_, DefaultRootWindow(x_display_),
      top_left_x, top_left_y, width_, height_,
      0 /* border width */,
      depth, CopyFromParent /* class */, CopyFromParent /* visual */,
      (CWBackPixel | CWOverrideRedirect), &window_attributes);
  x_windows_.push_back(x_window);
  XStoreName(x_display_, x_window, "VideoDecodeAcceleratorTest");
  XSelectInput(x_display_, x_window, ExposureMask);
  XMapWindow(x_display_, x_window);
  return x_window;
}

#endif  // OS_WIN

}  // namespace video_test_util
