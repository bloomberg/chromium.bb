// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file implements the ViewGLContext and PbufferGLContext classes.

#include "gpu/command_buffer/service/gl_context.h"
#include "gpu/command_buffer/common/logging.h"

namespace gpu {

#if !defined(UNIT_TEST)

static int g_regular_pixel_format = 0;
static int g_multisampled_pixel_format = 0;

const PIXELFORMATDESCRIPTOR kPixelFormatDescriptor = {
  sizeof(kPixelFormatDescriptor),    // Size of structure.
  1,                       // Default version.
  PFD_DRAW_TO_WINDOW |     // Window drawing support.
  PFD_SUPPORT_OPENGL |     // OpenGL support.
  PFD_DOUBLEBUFFER,        // Double buffering support (not stereo).
  PFD_TYPE_RGBA,           // RGBA color mode (not indexed).
  24,                      // 24 bit color mode.
  0, 0, 0, 0, 0, 0,        // Don't set RGB bits & shifts.
  8, 0,                    // 8 bit alpha
  0,                       // No accumulation buffer.
  0, 0, 0, 0,              // Ignore accumulation bits.
  24,                      // 24 bit z-buffer size.
  8,                       // 8-bit stencil buffer.
  0,                       // No aux buffer.
  PFD_MAIN_PLANE,          // Main drawing plane (not overlay).
  0,                       // Reserved.
  0, 0, 0,                 // Layer masks ignored.
};

LRESULT CALLBACK IntermediateWindowProc(HWND window,
                                        UINT message,
                                        WPARAM w_param,
                                        LPARAM l_param) {
  return ::DefWindowProc(window, message, w_param, l_param);
}

#endif  // UNIT_TEST

// Helper routine that does one-off initialization like determining the
// pixel format and initializing glew.
static bool InitializeOneOff() {
#if !defined(UNIT_TEST)
  static bool initialized = false;
  if (initialized)
    return true;

  // We must initialize a GL context before we can determine the multi-sampling
  // supported on the current hardware, so we create an intermediate window
  // and context here.
  HINSTANCE module_handle;
  if (!::GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT |
                           GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
                           reinterpret_cast<wchar_t*>(IntermediateWindowProc),
                           &module_handle)) {
    return false;
  }

  WNDCLASS intermediate_class;
  intermediate_class.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
  intermediate_class.lpfnWndProc = IntermediateWindowProc;
  intermediate_class.cbClsExtra = 0;
  intermediate_class.cbWndExtra = 0;
  intermediate_class.hInstance = module_handle;
  intermediate_class.hIcon = LoadIcon(NULL, IDI_APPLICATION);
  intermediate_class.hCursor = LoadCursor(NULL, IDC_ARROW);
  intermediate_class.hbrBackground = NULL;
  intermediate_class.lpszMenuName = NULL;
  intermediate_class.lpszClassName = L"Intermediate GL Window";

  ATOM class_registration = ::RegisterClass(&intermediate_class);
  if (!class_registration) {
    return false;
  }

  HWND intermediate_window = ::CreateWindow(
      reinterpret_cast<wchar_t*>(class_registration),
      L"",
      WS_OVERLAPPEDWINDOW,
      0, 0,
      CW_USEDEFAULT, CW_USEDEFAULT,
      NULL,
      NULL,
      NULL,
      NULL);

  if (!intermediate_window) {
    ::UnregisterClass(reinterpret_cast<wchar_t*>(class_registration),
                      module_handle);
    return false;
  }

  HDC intermediate_dc = ::GetDC(intermediate_window);
  g_regular_pixel_format = ::ChoosePixelFormat(intermediate_dc,
                                               &kPixelFormatDescriptor);
  if (g_regular_pixel_format == 0) {
    DLOG(ERROR) << "Unable to get the pixel format for GL context.";
    ::ReleaseDC(intermediate_window, intermediate_dc);
    ::DestroyWindow(intermediate_window);
    ::UnregisterClass(reinterpret_cast<wchar_t*>(class_registration),
                      module_handle);
    return false;
  }
  if (!::SetPixelFormat(intermediate_dc, g_regular_pixel_format,
                        &kPixelFormatDescriptor)) {
    DLOG(ERROR) << "Unable to set the pixel format for GL context.";
    ::ReleaseDC(intermediate_window, intermediate_dc);
    ::DestroyWindow(intermediate_window);
    ::UnregisterClass(reinterpret_cast<wchar_t*>(class_registration),
                      module_handle);
    return false;
  }

  // Create a temporary GL context to query for multisampled pixel formats.
  HGLRC gl_context = ::wglCreateContext(intermediate_dc);
  if (::wglMakeCurrent(intermediate_dc, gl_context)) {
    // GL context was successfully created and applied to the window's DC.
    // Startup GLEW, the GL extensions wrangler.
    if (InitializeGLEW()) {
      DLOG(INFO) << "Initialized GLEW " << glewGetString(GLEW_VERSION);
    } else {
      ::wglMakeCurrent(intermediate_dc, NULL);
      ::wglDeleteContext(gl_context);
      ::ReleaseDC(intermediate_window, intermediate_dc);
      ::DestroyWindow(intermediate_window);
      ::UnregisterClass(reinterpret_cast<wchar_t*>(class_registration),
                        module_handle);
      return false;
    }

    // If the multi-sample extensions are present, query the api to determine
    // the pixel format.
    if (WGLEW_ARB_pixel_format && WGLEW_ARB_multisample) {
      int pixel_attributes[] = {
        WGL_SAMPLES_ARB, 4,
        WGL_DRAW_TO_WINDOW_ARB, GL_TRUE,
        WGL_SUPPORT_OPENGL_ARB, GL_TRUE,
        WGL_ACCELERATION_ARB, WGL_FULL_ACCELERATION_ARB,
        WGL_COLOR_BITS_ARB, 24,
        WGL_ALPHA_BITS_ARB, 8,
        WGL_DEPTH_BITS_ARB, 24,
        WGL_STENCIL_BITS_ARB, 8,
        WGL_DOUBLE_BUFFER_ARB, GL_TRUE,
        WGL_SAMPLE_BUFFERS_ARB, GL_TRUE,
        0, 0};

      float pixel_attributes_f[] = {0, 0};
      unsigned int num_formats;

      // Query for the highest sampling rate supported, starting at 4x.
      static const int kSampleCount[] = {4, 2};
      static const int kNumSamples = 2;
      for (int sample = 0; sample < kNumSamples; ++sample) {
        pixel_attributes[1] = kSampleCount[sample];
        if (GL_TRUE == ::wglChoosePixelFormatARB(intermediate_dc,
                                                 pixel_attributes,
                                                 pixel_attributes_f,
                                                 1,
                                                 &g_multisampled_pixel_format,
                                                 &num_formats)) {
          break;
        }
      }
    }
  }

  ::wglMakeCurrent(intermediate_dc, NULL);
  ::wglDeleteContext(gl_context);
  ::ReleaseDC(intermediate_window, intermediate_dc);
  ::DestroyWindow(intermediate_window);
  ::UnregisterClass(reinterpret_cast<wchar_t*>(class_registration),
                    module_handle);

  initialized = true;
#endif  // UNIT_TEST

  return true;
}

bool ViewGLContext::Initialize(bool multisampled) {
#if !defined(UNIT_TEST)
  InitializeOneOff();

  // The GL context will render to this window.
  device_context_ = GetDC(window_);

  int pixel_format =
      multisampled ? g_multisampled_pixel_format : g_regular_pixel_format;
  if (!SetPixelFormat(device_context_,
                        pixel_format,
                        &kPixelFormatDescriptor)) {
    DLOG(ERROR) << "Unable to set the pixel format for GL context.";
    Destroy();
    return false;
  }

  context_ = wglCreateContext(device_context_);
  if (!context_) {
    DLOG(ERROR) << "Failed to create GL context.";
    Destroy();
    return false;
  }
#endif  // UNIT_TEST

  return true;
}

void ViewGLContext::Destroy() {
#if !defined(UNIT_TEST)
  if (context_) {
    wglDeleteContext(context_);
    context_ = NULL;
  }

  if (window_ && device_context_)
    ReleaseDC(window_, device_context_);

  window_ = NULL;
  device_context_ = NULL;
#endif  // UNIT_TEST
}

bool ViewGLContext::MakeCurrent() {
#if !defined(UNIT_TEST)
  if (IsCurrent()) {
    return true;
  }
  if (!wglMakeCurrent(device_context_, context_)) {
    DLOG(ERROR) << "Unable to make gl context current.";
    return false;
  }
#endif  // UNIT_TEST

  return true;
}

bool ViewGLContext::IsCurrent() {
#if !defined(UNIT_TEST)
  return wglGetCurrentDC() == device_context_ &&
      wglGetCurrentContext() == context_;
#else
  return true;
#endif
}

bool ViewGLContext::IsOffscreen() {
  return false;
}

void ViewGLContext::SwapBuffers() {
#if !defined(UNIT_TEST)
  DCHECK(device_context_);
  ::SwapBuffers(device_context_);
#endif  // UNIT_TEST
}

gfx::Size ViewGLContext::GetSize() {
#if !defined(UNIT_TEST)
  RECT rect;
  CHECK(GetClientRect(window_, &rect));
  return gfx::Size(rect.right - rect.left, rect.bottom - rect.top);
#else
  return gfx::Size();
#endif  // UNIT_TEST
}

GLContextHandle ViewGLContext::GetHandle() {
#if !defined(UNIT_TEST)
  return context_;
#else
  return NULL;
#endif  // UNIT_TEST
}

bool PbufferGLContext::Initialize(GLContext* shared_context) {
  return Initialize(shared_context ? shared_context->GetHandle() : NULL);
}

bool PbufferGLContext::Initialize(GLContextHandle shared_handle) {
#if !defined(UNIT_TEST)
  InitializeOneOff();

  // Create a device context compatible with the primary display.
  HDC display_device_context = ::CreateDC(L"DISPLAY", NULL, NULL, NULL);

  // Create a 1 x 1 pbuffer suitable for use with the device. This is just
  // a stepping stone towards creating a frame buffer object. It doesn't
  // matter what size it is.
  const int kNoAttributes[] = { 0 };
  pbuffer_ = ::wglCreatePbufferARB(display_device_context,
                                   g_regular_pixel_format,
                                   1, 1,
                                   kNoAttributes);
  ::DeleteDC(display_device_context);
  if (!pbuffer_) {
    DLOG(ERROR) << "Unable to create pbuffer.";
    Destroy();
    return false;
  }

  device_context_ = ::wglGetPbufferDCARB(pbuffer_);
  if (!device_context_) {
    DLOG(ERROR) << "Unable to get pbuffer device context.";
    Destroy();
    return false;
  }

  context_ = ::wglCreateContext(device_context_);
  if (!context_) {
    DLOG(ERROR) << "Failed to create GL context.";
    Destroy();
    return false;
  }

  if (shared_handle) {
    if (!wglShareLists(shared_handle, context_)) {
      DLOG(ERROR) << "Could not share GL contexts.";
      Destroy();
      return false;
    }
  }
#endif  // UNIT_TEST

  return true;
}

void PbufferGLContext::Destroy() {
#if !defined(UNIT_TEST)
  if (context_) {
    wglDeleteContext(context_);
    context_ = NULL;
  }

  if (pbuffer_ && device_context_)
    wglReleasePbufferDCARB(pbuffer_, device_context_);

  device_context_ = NULL;

  if (pbuffer_) {
    wglDestroyPbufferARB(pbuffer_);
    pbuffer_ = NULL;
  }
#endif  // UNIT_TEST
}

bool PbufferGLContext::MakeCurrent() {
#if !defined(UNIT_TEST)
  if (IsCurrent()) {
    return true;
  }
  if (!wglMakeCurrent(device_context_, context_)) {
    DLOG(ERROR) << "Unable to make gl context current.";
    return false;
  }
#endif  // UNIT_TEST

  return true;
}

bool PbufferGLContext::IsCurrent() {
#if !defined(UNIT_TEST)
  return wglGetCurrentDC() == device_context_ &&
      wglGetCurrentContext() == context_;
#else
  return true;
#endif
}

bool PbufferGLContext::IsOffscreen() {
  return true;
}

void PbufferGLContext::SwapBuffers() {
  NOTREACHED() << "Attempted to call SwapBuffers on a pbuffer.";
}

gfx::Size PbufferGLContext::GetSize() {
  NOTREACHED() << "Should not be requesting size of this pbuffer.";
  return gfx::Size(1, 1);
}

GLContextHandle PbufferGLContext::GetHandle() {
#if !defined(UNIT_TEST)
  return context_;
#else
  return NULL;
#endif  // UNIT_TEST
}

}  // namespace gpu
