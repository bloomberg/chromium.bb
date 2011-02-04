// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file implements the NativeViewGLContext and PbufferGLContext classes.

#include "app/gfx/gl/gl_context.h"

#include <GL/osmesa.h>

#include <algorithm>

#include "app/gfx/gl/gl_bindings.h"
#include "app/gfx/gl/gl_context_egl.h"
#include "app/gfx/gl/gl_context_osmesa.h"
#include "app/gfx/gl/gl_context_stub.h"
#include "app/gfx/gl/gl_implementation.h"

#include "base/logging.h"
#include "base/scoped_ptr.h"

namespace gfx {

typedef HGLRC GLContextHandle;
typedef HPBUFFERARB PbufferHandle;

class BaseWinGLContext : public GLContext {
 public:
  virtual std::string GetExtensions();

  virtual HDC GetDC() = 0;
};

// This class is a wrapper around a GL context that renders directly to a
// window.
class NativeViewGLContext : public BaseWinGLContext {
 public:
  explicit NativeViewGLContext(gfx::PluginWindowHandle window)
      : window_(window),
        device_context_(NULL),
        context_(NULL) {
    DCHECK(window);
  }

  // Initializes the GL context.
  bool Initialize(bool multisampled);

  virtual void Destroy();
  virtual bool MakeCurrent();
  virtual bool IsCurrent();
  virtual bool IsOffscreen();
  virtual bool SwapBuffers();
  virtual gfx::Size GetSize();
  virtual void* GetHandle();
  virtual void SetSwapInterval(int interval);

  virtual HDC GetDC();

 private:
  gfx::PluginWindowHandle window_;
  HDC device_context_;
  GLContextHandle context_;

  DISALLOW_COPY_AND_ASSIGN(NativeViewGLContext);
};

// This class is a wrapper around a GL context that uses OSMesa to render
// to an offscreen buffer and then blits it to a window.
class OSMesaViewGLContext : public GLContext {
 public:
  explicit OSMesaViewGLContext(gfx::PluginWindowHandle window)
      : window_(window),
        device_context_(NULL) {
    DCHECK(window);
  }

  // Initializes the GL context.
  bool Initialize();

  virtual void Destroy();
  virtual bool MakeCurrent();
  virtual bool IsCurrent();
  virtual bool IsOffscreen();
  virtual bool SwapBuffers();
  virtual gfx::Size GetSize();
  virtual void* GetHandle();
  virtual void SetSwapInterval(int interval);

 private:
  void UpdateSize();

  gfx::PluginWindowHandle window_;
  HDC device_context_;
  OSMesaGLContext osmesa_context_;

  DISALLOW_COPY_AND_ASSIGN(OSMesaViewGLContext);
};

// This class is a wrapper around a GL context used for offscreen rendering.
// It is initially backed by a 1x1 pbuffer. Use it to create an FBO to do useful
// rendering.
class PbufferGLContext : public GLContext {
 public:
  PbufferGLContext()
      : context_(NULL),
        device_context_(NULL),
        pbuffer_(NULL) {
  }

  // Initializes the GL context.
  bool Initialize(GLContext* shared_context);

  virtual void Destroy();
  virtual bool MakeCurrent();
  virtual bool IsCurrent();
  virtual bool IsOffscreen();
  virtual bool SwapBuffers();
  virtual gfx::Size GetSize();
  virtual void* GetHandle();
  virtual void SetSwapInterval(int interval);

  virtual HDC GetDC();

 private:
  GLContextHandle context_;
  HDC device_context_;
  PbufferHandle pbuffer_;

  DISALLOW_COPY_AND_ASSIGN(PbufferGLContext);
};

static HWND g_window;
static int g_regular_pixel_format = 0;
static int g_multisampled_pixel_format = 0;

// When using ANGLE we still need a window for D3D. This context creates the
// D3D device.
static BaseEGLContext* g_default_context;

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

// Helper routine that does one-off initialization like determining the
// pixel format and initializing the GL bindings.
bool GLContext::InitializeOneOff() {
  static bool initialized = false;
  if (initialized)
    return true;

  static const GLImplementation kAllowedGLImplementations[] = {
    kGLImplementationEGLGLES2,
    kGLImplementationDesktopGL,
    kGLImplementationOSMesaGL
  };

  if (!InitializeRequestedGLBindings(
           kAllowedGLImplementations,
           kAllowedGLImplementations + arraysize(kAllowedGLImplementations),
           kGLImplementationEGLGLES2)) {
    LOG(ERROR) << "InitializeRequestedGLBindings failed.";
    return false;
  }

  // We must initialize a GL context before we can determine the multi-
  // sampling supported on the current hardware, so we create an intermediate
  // window and context here.
  HINSTANCE module_handle;
  if (!::GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT |
                           GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
                           reinterpret_cast<wchar_t*>(IntermediateWindowProc),
                           &module_handle)) {
    LOG(ERROR) << "GetModuleHandleEx failed.";
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
    LOG(ERROR) << "RegisterClass failed.";
    return false;
  }

  g_window = ::CreateWindow(
      reinterpret_cast<wchar_t*>(class_registration),
      L"",
      WS_OVERLAPPEDWINDOW,
      0, 0,
      100, 100,
      NULL,
      NULL,
      NULL,
      NULL);

  if (!g_window) {
    ::UnregisterClass(reinterpret_cast<wchar_t*>(class_registration),
                      module_handle);
    LOG(ERROR) << "CreateWindow failed.";
    return false;
  }

  // Early out if OSMesa offscreen renderer or EGL is present.
  switch (GetGLImplementation()) {
    case kGLImplementationDesktopGL: {
      HDC intermediate_dc = ::GetDC(g_window);
      g_regular_pixel_format = ::ChoosePixelFormat(intermediate_dc,
                                                   &kPixelFormatDescriptor);
      if (g_regular_pixel_format == 0) {
        LOG(ERROR) << "Unable to get the pixel format for GL context.";
        ::ReleaseDC(g_window, intermediate_dc);
        ::DestroyWindow(g_window);
        ::UnregisterClass(reinterpret_cast<wchar_t*>(class_registration),
                          module_handle);
        return false;
      }
      if (!::SetPixelFormat(intermediate_dc, g_regular_pixel_format,
                            &kPixelFormatDescriptor)) {
        LOG(ERROR) << "Unable to set the pixel format for GL context.";
        ::ReleaseDC(g_window, intermediate_dc);
        ::DestroyWindow(g_window);
        ::UnregisterClass(reinterpret_cast<wchar_t*>(class_registration),
                          module_handle);
        return false;
      }

      // Create a temporary GL context to query for multisampled pixel formats.
      HGLRC gl_context = wglCreateContext(intermediate_dc);
      if (wglMakeCurrent(intermediate_dc, gl_context)) {
        // Get bindings to extension functions that cannot be acquired without a
        // current context.
        InitializeGLBindingsGL();
        InitializeGLBindingsWGL();

        // If the multi-sample extensions are present, query the api to
        // determine the pixel format.
        if (wglGetExtensionsStringARB) {
          std::string extensions =
              std::string(wglGetExtensionsStringARB(intermediate_dc));
          extensions += std::string(" ");
          if (extensions.find("WGL_ARB_pixel_format ")) {
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
              if (wglChoosePixelFormatARB(intermediate_dc,
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
      }

      wglMakeCurrent(intermediate_dc, NULL);
      wglDeleteContext(gl_context);
      ReleaseDC(g_window, intermediate_dc);
      UnregisterClass(reinterpret_cast<wchar_t*>(class_registration),
                      module_handle);
      break;
    }
    case kGLImplementationEGLGLES2:
      if (!BaseEGLContext::InitializeOneOff()) {
        LOG(ERROR) << "BaseEGLContext::InitializeOneOff failed.";
        return false;
      }
      break;
  }

  initialized = true;
  return true;
}


std::string BaseWinGLContext::GetExtensions() {
  if (wglGetExtensionsStringARB) {
    const char* extensions = wglGetExtensionsStringARB(GetDC());
    if (extensions) {
      return GLContext::GetExtensions() + " " + extensions;
    }
  }

  return GLContext::GetExtensions();
}

bool NativeViewGLContext::Initialize(bool multisampled) {
  // The GL context will render to this window.
  device_context_ = ::GetDC(window_);

  int pixel_format =
      multisampled ? g_multisampled_pixel_format : g_regular_pixel_format;
  if (!SetPixelFormat(device_context_,
                      pixel_format,
                      &kPixelFormatDescriptor)) {
    LOG(ERROR) << "Unable to set the pixel format for GL context.";
    Destroy();
    return false;
  }

  context_ = wglCreateContext(device_context_);
  if (!context_) {
    LOG(ERROR) << "Failed to create GL context.";
    Destroy();
    return false;
  }

  if (!MakeCurrent()) {
    LOG(ERROR) << "MakeCurrent failed.";
    Destroy();
    return false;
  }

  if (!InitializeCommon()) {
    LOG(ERROR) << "GLContext::InitializeCommon failed.";
    Destroy();
    return false;
  }

  return true;
}

void NativeViewGLContext::Destroy() {
  if (context_) {
    wglDeleteContext(context_);
    context_ = NULL;
  }

  if (window_ && device_context_)
    ReleaseDC(window_, device_context_);

  window_ = NULL;
  device_context_ = NULL;
}

bool NativeViewGLContext::MakeCurrent() {
  if (IsCurrent()) {
    return true;
  }
  if (!wglMakeCurrent(device_context_, context_)) {
    LOG(ERROR) << "Unable to make gl context current.";
    return false;
  }

  return true;
}

bool NativeViewGLContext::IsCurrent() {
  return wglGetCurrentDC() == device_context_ &&
      wglGetCurrentContext() == context_;
}

bool NativeViewGLContext::IsOffscreen() {
  return false;
}

bool NativeViewGLContext::SwapBuffers() {
  DCHECK(device_context_);
  return ::SwapBuffers(device_context_) == TRUE;
}

gfx::Size NativeViewGLContext::GetSize() {
  RECT rect;
  CHECK(GetClientRect(window_, &rect));
  return gfx::Size(rect.right - rect.left, rect.bottom - rect.top);
}

void* NativeViewGLContext::GetHandle() {
  return context_;
}

void NativeViewGLContext::SetSwapInterval(int interval) {
  DCHECK(IsCurrent());
  if (HasExtension("WGL_EXT_swap_control") && wglSwapIntervalEXT) {
    wglSwapIntervalEXT(interval);
  }
}

HDC NativeViewGLContext::GetDC() {
  return device_context_;
}

bool OSMesaViewGLContext::Initialize() {
  // The GL context will render to this window.
  device_context_ = GetDC(window_);

  if (!osmesa_context_.Initialize(OSMESA_RGBA, NULL)) {
    LOG(ERROR) << "OSMesaGLContext::Initialize failed.";
    Destroy();
    return false;
  }

  UpdateSize();

  return true;
}

void OSMesaViewGLContext::Destroy() {
  osmesa_context_.Destroy();

  if (window_ && device_context_)
    ReleaseDC(window_, device_context_);

  window_ = NULL;
  device_context_ = NULL;
}

bool OSMesaViewGLContext::MakeCurrent() {
  // TODO(apatrick): This is a bit of a hack. The window might have had zero
  // size when the context was initialized. Assume it has a valid size when
  // MakeCurrent is called and resize the back buffer if necessary.
  UpdateSize();
  return osmesa_context_.MakeCurrent();
}

bool OSMesaViewGLContext::IsCurrent() {
  return osmesa_context_.IsCurrent();
}

bool OSMesaViewGLContext::IsOffscreen() {
  return false;
}

bool OSMesaViewGLContext::SwapBuffers() {
  DCHECK(device_context_);

  // Update the size before blitting so that the blit size is exactly the same
  // as the window.
  UpdateSize();

  gfx::Size size = osmesa_context_.GetSize();

  // Note: negating the height below causes GDI to treat the bitmap data as row
  // 0 being at the top.
  BITMAPV4HEADER info = { sizeof(BITMAPV4HEADER) };
  info.bV4Width = size.width();
  info.bV4Height = -size.height();
  info.bV4Planes = 1;
  info.bV4BitCount = 32;
  info.bV4V4Compression = BI_BITFIELDS;
  info.bV4RedMask = 0x000000FF;
  info.bV4GreenMask = 0x0000FF00;
  info.bV4BlueMask = 0x00FF0000;
  info.bV4AlphaMask = 0xFF000000;

  // Copy the back buffer to the window's device context. Do not check whether
  // StretchDIBits succeeds or not. It will fail if the window has been
  // destroyed but it is preferable to allow rendering to silently fail if the
  // window is destroyed. This is because the primary application of this
  // class of GLContext is for testing and we do not want every GL related ui /
  // browser test to become flaky if there is a race condition between GL
  // context destruction and window destruction.
  StretchDIBits(device_context_,
                0, 0, size.width(), size.height(),
                0, 0, size.width(), size.height(),
                osmesa_context_.buffer(),
                reinterpret_cast<BITMAPINFO*>(&info),
                DIB_RGB_COLORS,
                SRCCOPY);

  return true;
}

gfx::Size OSMesaViewGLContext::GetSize() {
  return osmesa_context_.GetSize();
}

void* OSMesaViewGLContext::GetHandle() {
  return osmesa_context_.GetHandle();
}

void OSMesaViewGLContext::SetSwapInterval(int interval) {
  DCHECK(IsCurrent());
  // Fail silently. It is legitimate to set the swap interval on a view context
  // but GDI does not have those semantics.
}

void OSMesaViewGLContext::UpdateSize() {
  // Change back buffer size to that of window. If window handle is invalid, do
  // not change the back buffer size.
  RECT rect;
  if (!GetClientRect(window_, &rect))
    return;

  gfx::Size window_size = gfx::Size(
    std::max(1, static_cast<int>(rect.right - rect.left)),
    std::max(1, static_cast<int>(rect.bottom - rect.top)));
  osmesa_context_.Resize(window_size);
}

GLContext* GLContext::CreateViewGLContext(gfx::PluginWindowHandle window,
                                          bool multisampled) {
  switch (GetGLImplementation()) {
    case kGLImplementationOSMesaGL: {
      scoped_ptr<OSMesaViewGLContext> context(new OSMesaViewGLContext(window));
      if (!context->Initialize())
        return NULL;

      return context.release();
    }
    case kGLImplementationEGLGLES2: {
      scoped_ptr<NativeViewEGLContext> context(
          new NativeViewEGLContext(window));
      if (!context->Initialize())
        return NULL;

      return context.release();
    }
    case kGLImplementationDesktopGL: {
      scoped_ptr<NativeViewGLContext> context(new NativeViewGLContext(window));
      if (!context->Initialize(multisampled))
        return NULL;

      return context.release();
    }
    case kGLImplementationMockGL:
      return new StubGLContext;
    default:
      NOTREACHED();
      return NULL;
  }
}

bool PbufferGLContext::Initialize(GLContext* shared_context) {
  // Create a device context compatible with the primary display.
  HDC display_device_context = ::CreateDC(L"DISPLAY", NULL, NULL, NULL);

  // Create a 1 x 1 pbuffer suitable for use with the device. This is just
  // a stepping stone towards creating a frame buffer object. It doesn't
  // matter what size it is.
  if (!wglCreatePbufferARB) {
    LOG(ERROR) << "wglCreatePbufferARB not available.";
    Destroy();
    return false;
  }
  const int kNoAttributes[] = { 0 };
  pbuffer_ = wglCreatePbufferARB(display_device_context,
                                 g_regular_pixel_format,
                                 1, 1,
                                 kNoAttributes);
  ::DeleteDC(display_device_context);
  if (!pbuffer_) {
    LOG(ERROR) << "Unable to create pbuffer.";
    Destroy();
    return false;
  }

  device_context_ = wglGetPbufferDCARB(pbuffer_);
  if (!device_context_) {
    LOG(ERROR) << "Unable to get pbuffer device context.";
    Destroy();
    return false;
  }

  context_ = wglCreateContext(device_context_);
  if (!context_) {
    LOG(ERROR) << "Failed to create GL context.";
    Destroy();
    return false;
  }

  if (shared_context) {
    if (!wglShareLists(
        static_cast<GLContextHandle>(shared_context->GetHandle()), context_)) {
      LOG(ERROR) << "Could not share GL contexts.";
      Destroy();
      return false;
    }
  }

  if (!MakeCurrent()) {
    LOG(ERROR) << "MakeCurrent failed.";
    Destroy();
    return false;
  }

  if (!InitializeCommon()) {
    LOG(ERROR) << "GLContext::InitializeCommon failed.";
    Destroy();
    return false;
  }

  return true;
}

void PbufferGLContext::Destroy() {
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
}

bool PbufferGLContext::MakeCurrent() {
  if (IsCurrent()) {
    return true;
  }
  if (!wglMakeCurrent(device_context_, context_)) {
    LOG(ERROR) << "Unable to make gl context current.";
    return false;
  }

  return true;
}

bool PbufferGLContext::IsCurrent() {
  return wglGetCurrentDC() == device_context_ &&
      wglGetCurrentContext() == context_;
}

bool PbufferGLContext::IsOffscreen() {
  return true;
}

bool PbufferGLContext::SwapBuffers() {
  NOTREACHED() << "Attempted to call SwapBuffers on a pbuffer.";
  return false;
}

gfx::Size PbufferGLContext::GetSize() {
  NOTREACHED() << "Should not be requesting size of this pbuffer.";
  return gfx::Size(1, 1);
}

void* PbufferGLContext::GetHandle() {
  return context_;
}

void PbufferGLContext::SetSwapInterval(int interval) {
  DCHECK(IsCurrent());
  NOTREACHED() << "Attempt to call SetSwapInterval on a PbufferGLContext.";
}

HDC PbufferGLContext::GetDC() {
  return device_context_;
}

GLContext* GLContext::CreateOffscreenGLContext(GLContext* shared_context) {
  switch (GetGLImplementation()) {
    case kGLImplementationOSMesaGL: {
      scoped_ptr<OSMesaGLContext> context(new OSMesaGLContext);
      if (!context->Initialize(OSMESA_RGBA, shared_context))
        return NULL;

      return context.release();
    }
    case kGLImplementationEGLGLES2: {
      if (!shared_context) {
        if (!g_default_context) {
          scoped_ptr<NativeViewEGLContext> default_context(
              new NativeViewEGLContext(g_window));
          if (!default_context->Initialize())
            return NULL;

          g_default_context = default_context.release();
        }
        shared_context = g_default_context;
      }

      scoped_ptr<SecondaryEGLContext> context(
          new SecondaryEGLContext());
      if (!context->Initialize(shared_context))
        return NULL;

      return context.release();
    }
    case kGLImplementationDesktopGL: {
      scoped_ptr<PbufferGLContext> context(new PbufferGLContext);
      if (!context->Initialize(shared_context))
        return NULL;

      return context.release();
    }
    case kGLImplementationMockGL:
      return new StubGLContext;
    default:
      NOTREACHED();
      return NULL;
  }
}

}  // namespace gfx
