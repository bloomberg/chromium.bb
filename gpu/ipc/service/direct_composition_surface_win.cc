// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/service/direct_composition_surface_win.h"

#include "base/optional.h"
#include "gpu/ipc/service/gpu_channel_manager.h"
#include "gpu/ipc/service/gpu_channel_manager_delegate.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gl/egl_util.h"
#include "ui/gl/gl_angle_util_win.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_surface_egl.h"
#include "ui/gl/scoped_make_current.h"

#ifndef EGL_ANGLE_flexible_surface_compatibility
#define EGL_ANGLE_flexible_surface_compatibility 1
#define EGL_FLEXIBLE_SURFACE_COMPATIBILITY_SUPPORTED_ANGLE 0x33A6
#endif /* EGL_ANGLE_flexible_surface_compatibility */

#ifndef EGL_ANGLE_d3d_texture_client_buffer
#define EGL_ANGLE_d3d_texture_client_buffer 1
#define EGL_D3D_TEXTURE_ANGLE 0x33A3
#endif /* EGL_ANGLE_d3d_texture_client_buffer */

namespace gpu {
namespace {

// This class is used to make sure a specified surface isn't current, and upon
// destruction it will make the surface current again if it had been before.
class ScopedReleaseCurrent {
 public:
  explicit ScopedReleaseCurrent(gl::GLSurface* this_surface) {
    gl::GLContext* current_context = gl::GLContext::GetCurrent();
    bool was_current =
        current_context && current_context->IsCurrent(this_surface);
    if (was_current) {
      make_current_.emplace(current_context, this_surface);
      current_context->ReleaseCurrent(this_surface);
    }
  }

 private:
  base::Optional<ui::ScopedMakeCurrent> make_current_;
};

// Only one DirectComposition surface can be rendered into at a time. Track
// here which IDCompositionSurface is being rendered into. If another context
// is made current, then this surface will be suspended.
IDCompositionSurface* g_current_surface;

}  // namespace

DirectCompositionSurfaceWin::DirectCompositionSurfaceWin(
    base::WeakPtr<ImageTransportSurfaceDelegate> delegate,
    HWND parent_window)
    : gl::GLSurfaceEGL(), child_window_(delegate, parent_window) {}

DirectCompositionSurfaceWin::~DirectCompositionSurfaceWin() {
  Destroy();
}

bool DirectCompositionSurfaceWin::InitializeNativeWindow() {
  if (window_)
    return true;

  bool result = child_window_.Initialize();
  window_ = child_window_.window();
  return result;
}

bool DirectCompositionSurfaceWin::Initialize(
    std::unique_ptr<gfx::VSyncProvider> vsync_provider) {
  vsync_provider_ = std::move(vsync_provider);
  return Initialize(gl::GLSurfaceFormat());
}

bool DirectCompositionSurfaceWin::Initialize(gl::GLSurfaceFormat format) {
  d3d11_device_ = gl::QueryD3D11DeviceObjectFromANGLE();
  dcomp_device_ = gl::QueryDirectCompositionDevice(d3d11_device_);
  if (!dcomp_device_)
    return false;

  EGLDisplay display = GetDisplay();
  if (!window_) {
    if (!InitializeNativeWindow()) {
      DLOG(ERROR) << "Failed to initialize native window";
      return false;
    }
  }

  base::win::ScopedComPtr<IDCompositionDesktopDevice> desktop_device;
  dcomp_device_.QueryInterface(desktop_device.Receive());

  HRESULT hr = desktop_device->CreateTargetForHwnd(window_, TRUE,
                                                   dcomp_target_.Receive());
  if (FAILED(hr))
    return false;

  hr = dcomp_device_->CreateVisual(visual_.Receive());
  if (FAILED(hr))
    return false;

  dcomp_target_->SetRoot(visual_.get());

  std::vector<EGLint> pbuffer_attribs;
  pbuffer_attribs.push_back(EGL_WIDTH);
  pbuffer_attribs.push_back(1);
  pbuffer_attribs.push_back(EGL_HEIGHT);
  pbuffer_attribs.push_back(1);

  pbuffer_attribs.push_back(EGL_NONE);
  default_surface_ =
      eglCreatePbufferSurface(display, GetConfig(), &pbuffer_attribs[0]);
  CHECK(!!default_surface_);

  InitializeSurface();

  return true;
}

void DirectCompositionSurfaceWin::InitializeSurface() {
  ScopedReleaseCurrent release_current(this);
  ReleaseDrawTexture();
  dcomp_surface_.Release();
  HRESULT hr = dcomp_device_->CreateSurface(
      size_.width(), size_.height(), DXGI_FORMAT_B8G8R8A8_UNORM,
      DXGI_ALPHA_MODE_PREMULTIPLIED, dcomp_surface_.Receive());
  has_been_rendered_to_ = false;

  CHECK(SUCCEEDED(hr));
}

void DirectCompositionSurfaceWin::ReleaseDrawTexture() {
  if (real_surface_) {
    eglDestroySurface(GetDisplay(), real_surface_);
    real_surface_ = nullptr;
  }
  if (draw_texture_) {
    draw_texture_.Release();
    HRESULT hr = dcomp_surface_->EndDraw();
    CHECK(SUCCEEDED(hr));
  }
  if (dcomp_surface_ == g_current_surface)
    g_current_surface = nullptr;
}

void DirectCompositionSurfaceWin::Destroy() {
  if (default_surface_) {
    if (!eglDestroySurface(GetDisplay(), default_surface_)) {
      DLOG(ERROR) << "eglDestroySurface failed with error "
                  << ui::GetLastEGLErrorString();
    }
    default_surface_ = nullptr;
  }
  if (real_surface_) {
    if (!eglDestroySurface(GetDisplay(), real_surface_)) {
      DLOG(ERROR) << "eglDestroySurface failed with error "
                  << ui::GetLastEGLErrorString();
    }
    real_surface_ = nullptr;
  }
  if (dcomp_surface_ == g_current_surface)
    g_current_surface = nullptr;
  draw_texture_.Release();
  dcomp_surface_.Release();
}

gfx::Size DirectCompositionSurfaceWin::GetSize() {
  return size_;
}

bool DirectCompositionSurfaceWin::IsOffscreen() {
  return false;
}

void* DirectCompositionSurfaceWin::GetHandle() {
  return real_surface_ ? real_surface_ : default_surface_;
}

bool DirectCompositionSurfaceWin::Resize(const gfx::Size& size,
                                         float scale_factor,
                                         bool has_alpha) {
  if (size == GetSize())
    return true;

  // Force a resize and redraw (but not a move, activate, etc.).
  if (!SetWindowPos(window_, nullptr, 0, 0, size.width(), size.height(),
                    SWP_NOMOVE | SWP_NOACTIVATE | SWP_NOCOPYBITS |
                        SWP_NOOWNERZORDER | SWP_NOZORDER)) {
    return false;
  }
  size_ = size;
  InitializeSurface();

  return true;
}

gfx::SwapResult DirectCompositionSurfaceWin::SwapBuffers() {
  {
    ScopedReleaseCurrent release_current(this);
    ReleaseDrawTexture();
    visual_->SetContent(dcomp_surface_.get());

    CommitAndClearPendingOverlays();
    dcomp_device_->Commit();
  }
  // Force the driver to finish drawing before clearing the contents to
  // transparent, to reduce or eliminate the period of time where the contents
  // have flashed black.
  if (first_swap_) {
    glFinish();
    first_swap_ = false;
  }
  child_window_.ClearInvalidContents();
  return gfx::SwapResult::SWAP_ACK;
}

gfx::SwapResult DirectCompositionSurfaceWin::PostSubBuffer(int x,
                                                           int y,
                                                           int width,
                                                           int height) {
  ScopedReleaseCurrent release_current(this);
  ReleaseDrawTexture();
  visual_->SetContent(dcomp_surface_.get());
  CommitAndClearPendingOverlays();
  dcomp_device_->Commit();
  child_window_.ClearInvalidContents();
  return gfx::SwapResult::SWAP_ACK;
}

gfx::VSyncProvider* DirectCompositionSurfaceWin::GetVSyncProvider() {
  return vsync_provider_.get();
}

bool DirectCompositionSurfaceWin::ScheduleOverlayPlane(
    int z_order,
    gfx::OverlayTransform transform,
    gl::GLImage* image,
    const gfx::Rect& bounds_rect,
    const gfx::RectF& crop_rect) {
  pending_overlays_.push_back(
      Overlay(z_order, transform, image, bounds_rect, crop_rect));
  return true;
}

bool DirectCompositionSurfaceWin::CommitAndClearPendingOverlays() {
  pending_overlays_.clear();
  return true;
}

bool DirectCompositionSurfaceWin::FlipsVertically() const {
  return true;
}

bool DirectCompositionSurfaceWin::SupportsPostSubBuffer() {
  return true;
}

bool DirectCompositionSurfaceWin::OnMakeCurrent(gl::GLContext* context) {
  if (g_current_surface != dcomp_surface_) {
    if (g_current_surface) {
      HRESULT hr = g_current_surface->SuspendDraw();
      CHECK(SUCCEEDED(hr));
      g_current_surface = nullptr;
    }
    if (draw_texture_) {
      HRESULT hr = dcomp_surface_->ResumeDraw();
      CHECK(SUCCEEDED(hr));
      g_current_surface = dcomp_surface_.get();
    }
  }
  return true;
}

bool DirectCompositionSurfaceWin::SupportsSetDrawRectangle() const {
  return true;
}

bool DirectCompositionSurfaceWin::SetDrawRectangle(const gfx::Rect& rectangle) {
  if (draw_texture_)
    return false;
  if (!gfx::Rect(size_).Contains(rectangle)) {
    DLOG(ERROR) << "Draw rectangle must be contained within size of surface";
    return false;
  }
  if (gfx::Rect(size_) != rectangle && !has_been_rendered_to_) {
    DLOG(ERROR) << "First draw to surface must draw to everything";
    return false;
  }

  DCHECK(!real_surface_);
  CHECK(!g_current_surface);
  ScopedReleaseCurrent release_current(this);

  RECT rect = rectangle.ToRECT();
  // TODO(jbauman): Use update_offset
  POINT update_offset;

  HRESULT hr = dcomp_surface_->BeginDraw(
      &rect, IID_PPV_ARGS(draw_texture_.Receive()), &update_offset);
  CHECK(SUCCEEDED(hr));
  has_been_rendered_to_ = true;

  g_current_surface = dcomp_surface_.get();

  std::vector<EGLint> pbuffer_attribs{
      EGL_WIDTH,
      size_.width(),
      EGL_HEIGHT,
      size_.height(),
      EGL_FLEXIBLE_SURFACE_COMPATIBILITY_SUPPORTED_ANGLE,
      EGL_TRUE,
      EGL_NONE};

  EGLClientBuffer buffer =
      reinterpret_cast<EGLClientBuffer>(draw_texture_.get());
  real_surface_ = eglCreatePbufferFromClientBuffer(
      GetDisplay(), EGL_D3D_TEXTURE_ANGLE, buffer, GetConfig(),
      &pbuffer_attribs[0]);

  return true;
}

DirectCompositionSurfaceWin::Overlay::Overlay(int z_order,
                                              gfx::OverlayTransform transform,
                                              scoped_refptr<gl::GLImage> image,
                                              gfx::Rect bounds_rect,
                                              gfx::RectF crop_rect)
    : z_order(z_order),
      transform(transform),
      image(image),
      bounds_rect(bounds_rect),
      crop_rect(crop_rect) {}

DirectCompositionSurfaceWin::Overlay::Overlay(const Overlay& overlay) = default;

DirectCompositionSurfaceWin::Overlay::~Overlay() {}

}  // namespace gpu
