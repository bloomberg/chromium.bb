// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <EGL/egl.h>
#include <stdint.h>

#include "base/command_line.h"
#include "base/environment.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "gpu/command_buffer/client/gles2_lib.h"
#include "gpu/command_buffer/service/gpu_switches.h"
#include "gpu/config/gpu_info_collector.h"
#include "gpu/config/gpu_util.h"
#include "gpu/gles2_conform_support/egl/display.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_surface.h"

#if REGAL_STATIC_EGL
extern "C" {

typedef EGLContext RegalSystemContext;
#define REGAL_DECL
REGAL_DECL void RegalMakeCurrent( RegalSystemContext ctx );

}  // extern "C"
#endif

namespace {
void SetCurrentError(EGLint error_code) {
}

template<typename T>
T EglError(EGLint error_code, T return_value) {
  SetCurrentError(error_code);
  return return_value;
}

template<typename T>
T EglSuccess(T return_value) {
  SetCurrentError(EGL_SUCCESS);
  return return_value;
}

EGLint ValidateDisplay(EGLDisplay dpy) {
  if (dpy == EGL_NO_DISPLAY)
    return EGL_BAD_DISPLAY;

  egl::Display* display = static_cast<egl::Display*>(dpy);
  if (!display->is_initialized())
    return EGL_NOT_INITIALIZED;

  return EGL_SUCCESS;
}

EGLint ValidateDisplayConfig(EGLDisplay dpy, EGLConfig config) {
  EGLint error_code = ValidateDisplay(dpy);
  if (error_code != EGL_SUCCESS)
    return error_code;

  egl::Display* display = static_cast<egl::Display*>(dpy);
  if (!display->IsValidConfig(config))
    return EGL_BAD_CONFIG;

  return EGL_SUCCESS;
}

EGLint ValidateDisplaySurface(EGLDisplay dpy, EGLSurface surface) {
  EGLint error_code = ValidateDisplay(dpy);
  if (error_code != EGL_SUCCESS)
    return error_code;

  egl::Display* display = static_cast<egl::Display*>(dpy);
  if (!display->IsValidSurface(surface))
    return EGL_BAD_SURFACE;

  return EGL_SUCCESS;
}

EGLint ValidateDisplayContext(EGLDisplay dpy, EGLContext context) {
  EGLint error_code = ValidateDisplay(dpy);
  if (error_code != EGL_SUCCESS)
    return error_code;

  egl::Display* display = static_cast<egl::Display*>(dpy);
  if (!display->IsValidContext(context))
    return EGL_BAD_CONTEXT;

  return EGL_SUCCESS;
}
}  // namespace

extern "C" {
EGLAPI EGLint EGLAPIENTRY eglGetError() {
  // TODO(alokp): Fix me.
  return EGL_SUCCESS;
}

EGLAPI EGLDisplay EGLAPIENTRY eglGetDisplay(EGLNativeDisplayType display_id) {
  return new egl::Display(display_id);
}

EGLAPI EGLBoolean EGLAPIENTRY eglInitialize(EGLDisplay dpy,
                                            EGLint* major,
                                            EGLint* minor) {
  if (dpy == EGL_NO_DISPLAY)
    return EglError(EGL_BAD_DISPLAY, EGL_FALSE);

  egl::Display* display = static_cast<egl::Display*>(dpy);
  if (!display->Initialize())
    return EglError(EGL_NOT_INITIALIZED, EGL_FALSE);

  // eglInitialize can be called multiple times, prevent InitializeOneOff from
  // being called multiple times.
  if (gfx::GetGLImplementation() == gfx::kGLImplementationNone) {
    base::CommandLine::StringVector argv;
    scoped_ptr<base::Environment> env(base::Environment::Create());
    std::string env_string;
    env->GetVar("CHROME_COMMAND_BUFFER_GLES2_ARGS", &env_string);
#if defined(OS_WIN)
    argv = base::SplitString(base::UTF8ToUTF16(env_string),
                             base::kWhitespaceUTF16, base::TRIM_WHITESPACE,
                             base::SPLIT_WANT_NONEMPTY);
    argv.insert(argv.begin(), base::UTF8ToUTF16("dummy"));
#else
    argv = base::SplitString(env_string,
                             base::kWhitespaceASCII, base::TRIM_WHITESPACE,
                             base::SPLIT_WANT_NONEMPTY);
    argv.insert(argv.begin(), "dummy");
#endif
    base::CommandLine::Init(0, nullptr);
    base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
    // Need to call both Init and InitFromArgv, since Windows does not use
    // argc, argv in CommandLine::Init(argc, argv).
    command_line->InitFromArgv(argv);
    if (!command_line->HasSwitch(switches::kDisableGpuDriverBugWorkarounds)) {
      gpu::GPUInfo gpu_info;
      gpu::CollectBasicGraphicsInfo(&gpu_info);
      gpu::ApplyGpuDriverBugWorkarounds(gpu_info, command_line);
    }

    gfx::GLSurface::InitializeOneOff();
  }

  *major = 1;
  *minor = 4;
  return EglSuccess(EGL_TRUE);
}

EGLAPI EGLBoolean EGLAPIENTRY eglTerminate(EGLDisplay dpy) {
  EGLint error_code = ValidateDisplay(dpy);
  if (error_code != EGL_SUCCESS)
    return EglError(error_code, EGL_FALSE);

  egl::Display* display = static_cast<egl::Display*>(dpy);
  delete display;

  // TODO: EGL specifies that the objects are marked for deletion and they will
  // remain alive as long as "contexts or surfaces associated with display is
  // current to any thread".
  // Currently we delete the display here, and may also call exit handlers.

  return EglSuccess(EGL_TRUE);
}

EGLAPI const char* EGLAPIENTRY eglQueryString(EGLDisplay dpy, EGLint name) {
  EGLint error_code = ValidateDisplay(dpy);
  if (error_code != EGL_SUCCESS)
    return EglError(error_code, static_cast<const char*>(NULL));

  switch (name) {
    case EGL_CLIENT_APIS:
      return EglSuccess("OpenGL_ES");
    case EGL_EXTENSIONS:
      return EglSuccess("");
    case EGL_VENDOR:
      return EglSuccess("Google Inc.");
    case EGL_VERSION:
      return EglSuccess("1.4");
    default:
      return EglError(EGL_BAD_PARAMETER, static_cast<const char*>(NULL));
  }
}

EGLAPI EGLBoolean EGLAPIENTRY eglChooseConfig(EGLDisplay dpy,
                                              const EGLint* attrib_list,
                                              EGLConfig* configs,
                                              EGLint config_size,
                                              EGLint* num_config) {
  EGLint error_code = ValidateDisplay(dpy);
  if (error_code != EGL_SUCCESS)
    return EglError(error_code, EGL_FALSE);

  if (num_config == NULL)
    return EglError(EGL_BAD_PARAMETER, EGL_FALSE);

  egl::Display* display = static_cast<egl::Display*>(dpy);
  if (!display->ChooseConfigs(configs, config_size, num_config))
    return EglError(EGL_BAD_ATTRIBUTE, EGL_FALSE);

  return EglSuccess(EGL_TRUE);
}

EGLAPI EGLBoolean EGLAPIENTRY eglGetConfigs(EGLDisplay dpy,
                                            EGLConfig* configs,
                                            EGLint config_size,
                                            EGLint* num_config) {
  EGLint error_code = ValidateDisplay(dpy);
  if (error_code != EGL_SUCCESS)
    return EglError(error_code, EGL_FALSE);

  if (num_config == NULL)
    return EglError(EGL_BAD_PARAMETER, EGL_FALSE);

  egl::Display* display = static_cast<egl::Display*>(dpy);
  if (!display->GetConfigs(configs, config_size, num_config))
    return EglError(EGL_BAD_ATTRIBUTE, EGL_FALSE);

  return EglSuccess(EGL_TRUE);
}

EGLAPI EGLBoolean EGLAPIENTRY eglGetConfigAttrib(EGLDisplay dpy,
                                                 EGLConfig config,
                                                 EGLint attribute,
                                                 EGLint* value) {
  EGLint error_code = ValidateDisplayConfig(dpy, config);
  if (error_code != EGL_SUCCESS)
    return EglError(error_code, EGL_FALSE);

  egl::Display* display = static_cast<egl::Display*>(dpy);
  if (!display->GetConfigAttrib(config, attribute, value))
    return EglError(EGL_BAD_ATTRIBUTE, EGL_FALSE);

  return EglSuccess(EGL_TRUE);
}

EGLAPI EGLSurface EGLAPIENTRY
eglCreateWindowSurface(EGLDisplay dpy,
                       EGLConfig config,
                       EGLNativeWindowType win,
                       const EGLint* attrib_list) {
  EGLint error_code = ValidateDisplayConfig(dpy, config);
  if (error_code != EGL_SUCCESS)
    return EglError(error_code, EGL_NO_SURFACE);

  egl::Display* display = static_cast<egl::Display*>(dpy);
  if (!display->IsValidNativeWindow(win))
    return EglError(EGL_BAD_NATIVE_WINDOW, EGL_NO_SURFACE);

  EGLSurface surface = display->CreateWindowSurface(config, win, attrib_list);
  if (surface == EGL_NO_SURFACE)
    return EglError(EGL_BAD_ALLOC, EGL_NO_SURFACE);

  return EglSuccess(surface);
}

EGLAPI EGLSurface EGLAPIENTRY
eglCreatePbufferSurface(EGLDisplay dpy,
                        EGLConfig config,
                        const EGLint* attrib_list) {
  EGLint error_code = ValidateDisplayConfig(dpy, config);
  if (error_code != EGL_SUCCESS)
    return EglError(error_code, EGL_NO_SURFACE);

  egl::Display* display = static_cast<egl::Display*>(dpy);
  int width = 1;
  int height = 1;
  if (attrib_list) {
    for (const int32_t* attr = attrib_list; attr[0] != EGL_NONE; attr += 2) {
      switch (attr[0]) {
        case EGL_WIDTH:
          width = attr[1];
          break;
        case EGL_HEIGHT:
          height = attr[1];
          break;
      }
    }
  }
  display->SetCreateOffscreen(width, height);

  EGLSurface surface = display->CreateWindowSurface(config, 0, attrib_list);
  if (surface == EGL_NO_SURFACE)
    return EglError(EGL_BAD_ALLOC, EGL_NO_SURFACE);

  return EglSuccess(surface);
}

EGLAPI EGLSurface EGLAPIENTRY
eglCreatePixmapSurface(EGLDisplay dpy,
                       EGLConfig config,
                       EGLNativePixmapType pixmap,
                       const EGLint* attrib_list) {
  return EGL_NO_SURFACE;
}

EGLAPI EGLBoolean EGLAPIENTRY eglDestroySurface(EGLDisplay dpy,
                                                EGLSurface surface) {
  EGLint error_code = ValidateDisplaySurface(dpy, surface);
  if (error_code != EGL_SUCCESS)
    return EglError(error_code, EGL_FALSE);

  egl::Display* display = static_cast<egl::Display*>(dpy);
  display->DestroySurface(surface);
  return EglSuccess(EGL_TRUE);
}

EGLAPI EGLBoolean EGLAPIENTRY eglQuerySurface(EGLDisplay dpy,
                                              EGLSurface surface,
                                              EGLint attribute,
                                              EGLint* value) {
  return EGL_FALSE;
}

EGLAPI EGLBoolean EGLAPIENTRY eglBindAPI(EGLenum api) {
  return EGL_FALSE;
}

EGLAPI EGLenum EGLAPIENTRY eglQueryAPI() {
  return EGL_OPENGL_ES_API;
}

EGLAPI EGLBoolean EGLAPIENTRY eglWaitClient(void) {
  return EGL_FALSE;
}

EGLAPI EGLBoolean EGLAPIENTRY eglReleaseThread(void) {
  return EGL_FALSE;
}

EGLAPI EGLSurface EGLAPIENTRY
eglCreatePbufferFromClientBuffer(EGLDisplay dpy,
                                 EGLenum buftype,
                                 EGLClientBuffer buffer,
                                 EGLConfig config,
                                 const EGLint* attrib_list) {
  return EGL_NO_SURFACE;
}

EGLAPI EGLBoolean EGLAPIENTRY eglSurfaceAttrib(EGLDisplay dpy,
                                               EGLSurface surface,
                                               EGLint attribute,
                                               EGLint value) {
  return EGL_FALSE;
}

EGLAPI EGLBoolean EGLAPIENTRY eglBindTexImage(EGLDisplay dpy,
                                              EGLSurface surface,
                                              EGLint buffer) {
  return EGL_FALSE;
}

EGLAPI EGLBoolean EGLAPIENTRY eglReleaseTexImage(EGLDisplay dpy,
                                                 EGLSurface surface,
                                                 EGLint buffer) {
  return EGL_FALSE;
}

EGLAPI EGLBoolean EGLAPIENTRY eglSwapInterval(EGLDisplay dpy, EGLint interval) {
  return EGL_FALSE;
}

EGLAPI EGLContext EGLAPIENTRY eglCreateContext(EGLDisplay dpy,
                                               EGLConfig config,
                                               EGLContext share_context,
                                               const EGLint* attrib_list) {
  EGLint error_code = ValidateDisplayConfig(dpy, config);
  if (error_code != EGL_SUCCESS)
    return EglError(error_code, EGL_NO_CONTEXT);

  if (share_context != EGL_NO_CONTEXT) {
    error_code = ValidateDisplayContext(dpy, share_context);
    if (error_code != EGL_SUCCESS)
      return EglError(error_code, EGL_NO_CONTEXT);
  }

  egl::Display* display = static_cast<egl::Display*>(dpy);
  EGLContext context = display->CreateContext(
      config, share_context, attrib_list);
  if (context == EGL_NO_CONTEXT)
    return EglError(EGL_BAD_ALLOC, EGL_NO_CONTEXT);

  return EglSuccess(context);
}

EGLAPI EGLBoolean EGLAPIENTRY eglDestroyContext(EGLDisplay dpy,
                                                EGLContext ctx) {
  EGLint error_code = ValidateDisplayContext(dpy, ctx);
  if (error_code != EGL_SUCCESS)
    return EglError(error_code, EGL_FALSE);

  egl::Display* display = static_cast<egl::Display*>(dpy);
  display->DestroyContext(ctx);
  return EGL_TRUE;
}

EGLAPI EGLBoolean EGLAPIENTRY eglMakeCurrent(EGLDisplay dpy,
                                             EGLSurface draw,
                                             EGLSurface read,
                                             EGLContext ctx) {
  if (ctx != EGL_NO_CONTEXT) {
    EGLint error_code = ValidateDisplaySurface(dpy, draw);
    if (error_code != EGL_SUCCESS)
      return EglError(error_code, EGL_FALSE);
    error_code = ValidateDisplaySurface(dpy, read);
    if (error_code != EGL_SUCCESS)
      return EglError(error_code, EGL_FALSE);
    error_code = ValidateDisplayContext(dpy, ctx);
    if (error_code != EGL_SUCCESS)
      return EglError(error_code, EGL_FALSE);
  }

  egl::Display* display = static_cast<egl::Display*>(dpy);
  if (!display->MakeCurrent(draw, read, ctx))
    return EglError(EGL_CONTEXT_LOST, EGL_FALSE);

#if REGAL_STATIC_EGL
  RegalMakeCurrent(ctx);
#endif

  return EGL_TRUE;
}

EGLAPI EGLContext EGLAPIENTRY eglGetCurrentContext() {
  return EGL_NO_CONTEXT;
}

EGLAPI EGLSurface EGLAPIENTRY eglGetCurrentSurface(EGLint readdraw) {
  return EGL_NO_SURFACE;
}

EGLAPI EGLDisplay EGLAPIENTRY eglGetCurrentDisplay() {
  return EGL_NO_DISPLAY;
}

EGLAPI EGLBoolean EGLAPIENTRY eglQueryContext(EGLDisplay dpy,
                                              EGLContext ctx,
                                              EGLint attribute,
                                              EGLint* value) {
  return EGL_FALSE;
}

EGLAPI EGLBoolean EGLAPIENTRY eglWaitGL() {
  return EGL_FALSE;
}

EGLAPI EGLBoolean EGLAPIENTRY eglWaitNative(EGLint engine) {
  return EGL_FALSE;
}

EGLAPI EGLBoolean EGLAPIENTRY eglSwapBuffers(EGLDisplay dpy,
                                             EGLSurface surface) {
  EGLint error_code = ValidateDisplaySurface(dpy, surface);
  if (error_code != EGL_SUCCESS)
    return EglError(error_code, EGL_FALSE);

  egl::Display* display = static_cast<egl::Display*>(dpy);
  display->SwapBuffers(surface);
  return EglSuccess(EGL_TRUE);
}

EGLAPI EGLBoolean EGLAPIENTRY eglCopyBuffers(EGLDisplay dpy,
                                             EGLSurface surface,
                                             EGLNativePixmapType target) {
  return EGL_FALSE;
}

/* Now, define eglGetProcAddress using the generic function ptr. type */
EGLAPI __eglMustCastToProperFunctionPointerType EGLAPIENTRY
eglGetProcAddress(const char* procname) {
  return reinterpret_cast<__eglMustCastToProperFunctionPointerType>(
      gles2::GetGLFunctionPointer(procname));
}
}  // extern "C"
