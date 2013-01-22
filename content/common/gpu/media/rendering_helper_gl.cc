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
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_context_stub.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_surface.h"

#if !defined(OS_WIN) && defined(ARCH_CPU_X86_FAMILY)
#define GL_VARIANT_GLX 1
typedef GLXWindow NativeWindowType;
typedef GLXContext NativeContextType;
struct ScopedPtrXFree {
  void operator()(void* x) const { ::XFree(x); }
};
#else
#define GL_VARIANT_EGL 1
typedef EGLNativeWindowType NativeWindowType;
typedef EGLContext NativeContextType;
typedef EGLSurface NativeSurfaceType;
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

namespace {

// Lightweight GLContext stub implementation that returns a constructed
// extensions string.  We use this to create a context that we can use to
// initialize GL extensions with, without actually creating a platform context.
class GLContextStubWithExtensions : public gfx::GLContextStub {
 public:
  GLContextStubWithExtensions() {}
  virtual std::string GetExtensions() OVERRIDE;

  void AddExtensionsString(const char* extensions);

 protected:
  virtual ~GLContextStubWithExtensions() {}

 private:
  std::string extensions_;

  DISALLOW_COPY_AND_ASSIGN(GLContextStubWithExtensions);
};

void GLContextStubWithExtensions::AddExtensionsString(const char* extensions) {
  if (extensions == NULL)
    return;

  if (extensions_.size() != 0)
    extensions_ += ' ';
  extensions_ += extensions;
}

std::string GLContextStubWithExtensions::GetExtensions() {
  return extensions_;
}

}  // anonymous

namespace content {

class RenderingHelperGL : public RenderingHelper {
 public:
  RenderingHelperGL();
  virtual ~RenderingHelperGL();

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

  static const gfx::GLImplementation kGLImplementation;

 private:
  void Clear();

  // Make window_id's surface current w/ the GL context, or release the context
  // if |window_id < 0|.
  void MakeCurrent(int window_id);


  MessageLoop* message_loop_;
  int width_;
  int height_;
  bool suppress_swap_to_display_;

  NativeContextType gl_context_;
  std::map<uint32, int> texture_id_to_surface_index_;

#if defined(GL_VARIANT_EGL)
  EGLDisplay gl_display_;
  std::vector<NativeSurfaceType> gl_surfaces_;
#else
  XVisualInfo* x_visual_;
#endif

#if defined(OS_WIN)
  std::vector<HWND> windows_;
#else
  Display* x_display_;
  std::vector<Window> x_windows_;
#endif
};

// static
const gfx::GLImplementation RenderingHelperGL::kGLImplementation =
#if defined(GL_VARIANT_GLX)
    gfx::kGLImplementationDesktopGL;
#elif defined(GL_VARIANT_EGL)
    gfx::kGLImplementationEGLGLES2;
#else
    -1;
#error "Unknown GL implementation."
#endif

// static
RenderingHelper* RenderingHelper::Create() {
  return new RenderingHelperGL;
}

// static
void RenderingHelper::InitializePlatform() {
  gfx::InitializeGLBindings(RenderingHelperGL::kGLImplementation);
}

RenderingHelperGL::RenderingHelperGL() {
  Clear();
}

RenderingHelperGL::~RenderingHelperGL() {
  CHECK_EQ(width_, 0) << "Must call UnInitialize before dtor.";
  Clear();
}

void RenderingHelperGL::MakeCurrent(int window_id) {
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

void RenderingHelperGL::Initialize(bool suppress_swap_to_display,
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

  scoped_refptr<GLContextStubWithExtensions> stub_context(
      new GLContextStubWithExtensions());
  suppress_swap_to_display_ = suppress_swap_to_display;
  CHECK_GT(width, 0);
  CHECK_GT(height, 0);
  width_ = width;
  height_ = height;
  message_loop_ = MessageLoop::current();
  CHECK_GT(num_windows, 0);

#if GL_VARIANT_GLX
  x_display_ = base::MessagePumpForUI::GetDefaultXDisplay();
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
  scoped_ptr_malloc<GLXFBConfig, ScopedPtrXFree> glx_fb_configs(
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

#else // EGL
  EGLNativeDisplayType native_display;

#if defined(OS_WIN)
  native_display = EGL_DEFAULT_DISPLAY;
#else
  x_display_ = base::MessagePumpForUI::GetDefaultXDisplay();
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
#endif

  // Per-window/surface X11 & EGL initialization.
  for (int i = 0; i < num_windows; ++i) {
    // Arrange X windows whimsically, with some padding.
    int top_left_x = (width + 20) * (i % 4);
    int top_left_y = (height + 12) * (i % 3);

#if defined(OS_WIN)
    NativeWindowType window =
        CreateWindowEx(0, L"Static", L"VideoDecodeAcceleratorTest",
                       WS_OVERLAPPEDWINDOW | WS_VISIBLE, top_left_x,
                       top_left_y, width_, height_, NULL, NULL, NULL,
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
        top_left_x, top_left_y, width_, height_,
        0 /* border width */,
        depth, CopyFromParent /* class */, CopyFromParent /* visual */,
        (CWBackPixel | CWOverrideRedirect), &window_attributes);
    XStoreName(x_display_, window, "VideoDecodeAcceleratorTest");
    XSelectInput(x_display_, window, ExposureMask);
    XMapWindow(x_display_, window);
    x_windows_.push_back(window);
#endif

#if GL_VARIANT_EGL
    NativeSurfaceType egl_surface =
        eglCreateWindowSurface(gl_display_, egl_config, window, NULL);
    gl_surfaces_.push_back(egl_surface);
    CHECK_NE(egl_surface, EGL_NO_SURFACE);
#endif
    MakeCurrent(i);
  }

  // Must be done after a context is made current.
  gfx::InitializeGLExtensionBindings(kGLImplementation, stub_context.get());

  static const float kVertices[] =
      { -1.f, 1.f, -1.f, -1.f, 1.f, 1.f, 1.f, -1.f, };
  static const float kTextureCoords[] = { 0, 1, 0, 0, 1, 1, 1, 0, };
// On Windows the textures from Direct3D which renders them flipped.
#if GL_VARIANT_GLX || defined(OS_WIN)
  static const char kVertexShader[] = STRINGIZE(
      varying vec2 interp_tc;
      attribute vec4 in_pos;
      attribute vec2 in_tc;
      void main() {
        interp_tc = vec2(in_tc.x, 1.0 - in_tc.y);
        gl_Position = in_pos;
      });
#else
  static const char kVertexShader[] = STRINGIZE(
      varying vec2 interp_tc;
      attribute vec4 in_pos;
      attribute vec2 in_tc;
      void main() {
        interp_tc = in_tc;
        gl_Position = in_pos;
      });
#endif

#if GL_VARIANT_EGL
  static const char kFragmentShader[] = STRINGIZE(
      precision mediump float;
      varying vec2 interp_tc;
      uniform sampler2D tex;
      void main() {
        gl_FragColor = texture2D(tex, interp_tc);
      });
#else
  static const char kFragmentShader[] = STRINGIZE(
      varying vec2 interp_tc;
      uniform sampler2D tex;
      void main() {
        gl_FragColor = texture2D(tex, interp_tc);
      });
#endif
  GLuint program = glCreateProgram();
  CreateShader(program, GL_VERTEX_SHADER,
               kVertexShader, arraysize(kVertexShader));
  CreateShader(program, GL_FRAGMENT_SHADER,
               kFragmentShader, arraysize(kFragmentShader));
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
  glVertexAttribPointer(tc_location, 2, GL_FLOAT, GL_FALSE, 0, kTextureCoords);
  done->Signal();
}

void RenderingHelperGL::UnInitialize(base::WaitableEvent* done) {
  CHECK_EQ(MessageLoop::current(), message_loop_);
#if GL_VARIANT_GLX

  glXDestroyContext(x_display_, gl_context_);
#else // EGL
  MakeCurrent(-1);
  CHECK(eglDestroyContext(gl_display_, gl_context_));
  for (size_t i = 0; i < gl_surfaces_.size(); ++i)
    CHECK(eglDestroySurface(gl_display_, gl_surfaces_[i]));
  CHECK(eglTerminate(gl_display_));
#endif
  Clear();
  done->Signal();
}

void RenderingHelperGL::CreateTexture(int window_id,
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
  MakeCurrent(window_id);
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
  CHECK(texture_id_to_surface_index_.insert(
      std::make_pair(*texture_id, window_id)).second);
  done->Signal();
}

void RenderingHelperGL::RenderTexture(uint32 texture_id) {
  CHECK_EQ(MessageLoop::current(), message_loop_);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, texture_id);
  glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
  CHECK_EQ(static_cast<int>(glGetError()), GL_NO_ERROR);
  if (suppress_swap_to_display_)
    return;

  int window_id = texture_id_to_surface_index_[texture_id];
  MakeCurrent(window_id);
#if GL_VARIANT_GLX
  glXSwapBuffers(x_display_, x_windows_[window_id]);
#else  // EGL
  eglSwapBuffers(gl_display_, gl_surfaces_[window_id]);
  CHECK_EQ(static_cast<int>(eglGetError()), EGL_SUCCESS);
#endif
}

void RenderingHelperGL::DeleteTexture(uint32 texture_id) {
  glDeleteTextures(1, &texture_id);
  CHECK_EQ(static_cast<int>(glGetError()), GL_NO_ERROR);
}

void* RenderingHelperGL::GetGLContext() {
  return gl_context_;
}

void* RenderingHelperGL::GetGLDisplay() {
#if GL_VARIANT_GLX
  return x_display_;
#else  // EGL
  return gl_display_;
#endif
}

void RenderingHelperGL::Clear() {
  suppress_swap_to_display_ = false;
  width_ = 0;
  height_ = 0;
  texture_id_to_surface_index_.clear();
  message_loop_ = NULL;
  gl_context_ = NULL;
#if GL_VARIANT_EGL
  gl_display_ = EGL_NO_DISPLAY;
  gl_surfaces_.clear();
#endif

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

}  // namespace content
