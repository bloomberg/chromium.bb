// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <EGL/egl.h>

#include "base/at_exit.h"
#include "gpu/command_buffer_gles2/command_buffer_gles2_export.h"
#include "ui/gl/gl_implementation.h"

base::AtExitManager exit_manager;

extern "C" {

EGLDisplay GPU_COMMAND_BUFFER_EGL_EXPORT
CommandBuffer_GetDisplay(EGLNativeDisplayType display_id) {
  return eglGetDisplay(display_id);
}

EGLBoolean GPU_COMMAND_BUFFER_EGL_EXPORT
CommandBuffer_Initialize(EGLDisplay dpy, EGLint* major, EGLint* minor) {
  EGLBoolean result = eglInitialize(dpy, major, minor);
  DCHECK(gfx::GetGLImplementation() != gfx::kGLImplementationNone);
  return result;
}

EGLBoolean GPU_COMMAND_BUFFER_EGL_EXPORT
CommandBuffer_Terminate(EGLDisplay dpy) {
  return eglTerminate(dpy);
}

EGLBoolean GPU_COMMAND_BUFFER_EGL_EXPORT
CommandBuffer_ChooseConfig(EGLDisplay dpy,
                           const EGLint* attrib_list,
                           EGLConfig* configs,
                           EGLint config_size,
                           EGLint* num_config) {
  return eglChooseConfig(dpy, attrib_list, configs, config_size, num_config);
}

EGLBoolean GPU_COMMAND_BUFFER_EGL_EXPORT
CommandBuffer_GetConfigAttrib(EGLDisplay dpy,
                              EGLConfig config,
                              EGLint attribute,
                              EGLint* value) {
  return eglGetConfigAttrib(dpy, config, attribute, value);
}

EGLSurface GPU_COMMAND_BUFFER_EGL_EXPORT
CommandBuffer_CreateWindowSurface(EGLDisplay dpy,
                                  EGLConfig config,
                                  EGLNativeWindowType win,
                                  const EGLint* attrib_list) {
  return eglCreateWindowSurface(dpy, config, win, attrib_list);
}

EGLSurface GPU_COMMAND_BUFFER_EGL_EXPORT
CommandBuffer_CreatePbufferSurface(EGLDisplay dpy,
                                   EGLConfig config,
                                   const EGLint* attrib_list) {
  return eglCreatePbufferSurface(dpy, config, attrib_list);
}

EGLBoolean GPU_COMMAND_BUFFER_EGL_EXPORT
CommandBuffer_DestroySurface(EGLDisplay dpy, EGLSurface surface) {
  return eglDestroySurface(dpy, surface);
}

EGLContext GPU_COMMAND_BUFFER_EGL_EXPORT
CommandBuffer_CreateContext(EGLDisplay dpy,
                            EGLConfig config,
                            EGLContext share_context,
                            const EGLint* attrib_list) {
  return eglCreateContext(dpy, config, share_context, attrib_list);
}

EGLBoolean GPU_COMMAND_BUFFER_EGL_EXPORT
CommandBuffer_DestroyContext(EGLDisplay dpy, EGLContext ctx) {
  return eglDestroyContext(dpy, ctx);
}

EGLBoolean GPU_COMMAND_BUFFER_EGL_EXPORT
CommandBuffer_MakeCurrent(EGLDisplay dpy,
                          EGLSurface draw,
                          EGLSurface read,
                          EGLContext ctx) {
  return eglMakeCurrent(dpy, draw, read, ctx);
}

EGLBoolean GPU_COMMAND_BUFFER_EGL_EXPORT
CommandBuffer_SwapBuffers(EGLDisplay dpy, EGLSurface surface) {
  return eglSwapBuffers(dpy, surface);
}

__eglMustCastToProperFunctionPointerType GPU_COMMAND_BUFFER_EGL_EXPORT
CommandBuffer_GetProcAddress(const char* procname) {
  return eglGetProcAddress(procname);
}
}  // extern "C"
