// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/service/direct_composition_child_surface_win.h"

#include <d3d11_1.h>
#include <dcomptypes.h>

#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/synchronization/waitable_event.h"
#include "base/trace_event/trace_event.h"
#include "ui/display/display_switches.h"
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
// Only one DirectComposition surface can be rendered into at a time. Track
// here which IDCompositionSurface is being rendered into. If another context
// is made current, then this surface will be suspended.
IDCompositionSurface* g_current_surface;
}

DirectCompositionChildSurfaceWin::DirectCompositionChildSurfaceWin(
    const gfx::Size& size,
    bool is_hdr,
    bool has_alpha,
    bool enable_dc_layers)
    : gl::GLSurfaceEGL(),
      size_(size),
      is_hdr_(is_hdr),
      has_alpha_(has_alpha),
      enable_dc_layers_(enable_dc_layers) {}

DirectCompositionChildSurfaceWin::~DirectCompositionChildSurfaceWin() {
  Destroy();
}

bool DirectCompositionChildSurfaceWin::Initialize(gl::GLSurfaceFormat format) {
  d3d11_device_ = gl::QueryD3D11DeviceObjectFromANGLE();
  dcomp_device_ = gl::QueryDirectCompositionDevice(d3d11_device_);
  if (!dcomp_device_)
    return false;

  EGLDisplay display = GetDisplay();

  std::vector<EGLint> pbuffer_attribs;
  pbuffer_attribs.push_back(EGL_WIDTH);
  pbuffer_attribs.push_back(1);
  pbuffer_attribs.push_back(EGL_HEIGHT);
  pbuffer_attribs.push_back(1);

  pbuffer_attribs.push_back(EGL_NONE);
  default_surface_ =
      eglCreatePbufferSurface(display, GetConfig(), &pbuffer_attribs[0]);
  CHECK(!!default_surface_);

  return true;
}

void DirectCompositionChildSurfaceWin::ReleaseCurrentSurface() {
  ReleaseDrawTexture(true);
  dcomp_surface_.Reset();
  swap_chain_.Reset();
}

bool DirectCompositionChildSurfaceWin::InitializeSurface() {
  TRACE_EVENT1("gpu", "DirectCompositionChildSurfaceWin::InitializeSurface()",
               "enable_dc_layers_", enable_dc_layers_);
  DCHECK(!dcomp_surface_);
  DCHECK(!swap_chain_);
  DXGI_FORMAT output_format =
      is_hdr_ ? DXGI_FORMAT_R16G16B16A16_FLOAT : DXGI_FORMAT_B8G8R8A8_UNORM;
  if (enable_dc_layers_) {
    // Always treat as premultiplied, because an underlay could cause it to
    // become transparent.
    HRESULT hr = dcomp_device_->CreateSurface(
        size_.width(), size_.height(), output_format,
        DXGI_ALPHA_MODE_PREMULTIPLIED, dcomp_surface_.GetAddressOf());
    has_been_rendered_to_ = false;
    CHECK(SUCCEEDED(hr));
  } else {
    DXGI_ALPHA_MODE alpha_mode =
        has_alpha_ ? DXGI_ALPHA_MODE_PREMULTIPLIED : DXGI_ALPHA_MODE_IGNORE;
    Microsoft::WRL::ComPtr<IDXGIDevice> dxgi_device;
    d3d11_device_.CopyTo(dxgi_device.GetAddressOf());
    Microsoft::WRL::ComPtr<IDXGIAdapter> dxgi_adapter;
    dxgi_device->GetAdapter(dxgi_adapter.GetAddressOf());
    Microsoft::WRL::ComPtr<IDXGIFactory2> dxgi_factory;
    dxgi_adapter->GetParent(IID_PPV_ARGS(dxgi_factory.GetAddressOf()));

    DXGI_SWAP_CHAIN_DESC1 desc = {};
    desc.Width = size_.width();
    desc.Height = size_.height();
    desc.Format = output_format;
    desc.Stereo = FALSE;
    desc.SampleDesc.Count = 1;
    desc.BufferCount = 2;
    desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.Scaling = DXGI_SCALING_STRETCH;
    desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
    desc.AlphaMode = alpha_mode;
    desc.Flags = 0;
    HRESULT hr = dxgi_factory->CreateSwapChainForComposition(
        d3d11_device_.Get(), &desc, nullptr, swap_chain_.GetAddressOf());
    has_been_rendered_to_ = false;
    first_swap_ = true;
    return SUCCEEDED(hr);
  }
  return true;
}

void DirectCompositionChildSurfaceWin::ReleaseDrawTexture(bool will_discard) {
  if (real_surface_) {
    eglDestroySurface(GetDisplay(), real_surface_);
    real_surface_ = nullptr;
  }
  if (draw_texture_) {
    draw_texture_.Reset();
    if (dcomp_surface_) {
      HRESULT hr = dcomp_surface_->EndDraw();
      CHECK(SUCCEEDED(hr));
      dcomp_surface_serial_++;
    } else if (!will_discard) {
      DXGI_PRESENT_PARAMETERS params = {};
      RECT dirty_rect = swap_rect_.ToRECT();
      params.DirtyRectsCount = 1;
      params.pDirtyRects = &dirty_rect;
      swap_chain_->Present1(first_swap_ ? 0 : 1, 0, &params);
      if (first_swap_) {
        // Wait for the GPU to finish executing its commands before
        // committing the DirectComposition tree, or else the swapchain
        // may flicker black when it's first presented.
        Microsoft::WRL::ComPtr<IDXGIDevice2> dxgi_device2;
        HRESULT hr = d3d11_device_.CopyTo(dxgi_device2.GetAddressOf());
        DCHECK(SUCCEEDED(hr));
        base::WaitableEvent event(
            base::WaitableEvent::ResetPolicy::AUTOMATIC,
            base::WaitableEvent::InitialState::NOT_SIGNALED);
        dxgi_device2->EnqueueSetEvent(event.handle());
        event.Wait();
        first_swap_ = false;
      }
    }
  }
  if (dcomp_surface_.Get() == g_current_surface)
    g_current_surface = nullptr;
}

void DirectCompositionChildSurfaceWin::Destroy() {
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
  if (dcomp_surface_ && (dcomp_surface_.Get() == g_current_surface)) {
    HRESULT hr = dcomp_surface_->EndDraw();
    CHECK(SUCCEEDED(hr));
    g_current_surface = nullptr;
  }
  draw_texture_.Reset();
  dcomp_surface_.Reset();
}

gfx::Size DirectCompositionChildSurfaceWin::GetSize() {
  return size_;
}

bool DirectCompositionChildSurfaceWin::IsOffscreen() {
  return false;
}

void* DirectCompositionChildSurfaceWin::GetHandle() {
  return real_surface_ ? real_surface_ : default_surface_;
}

gfx::SwapResult DirectCompositionChildSurfaceWin::SwapBuffers() {
  ReleaseDrawTexture(false);
  return gfx::SwapResult::SWAP_ACK;
}

bool DirectCompositionChildSurfaceWin::FlipsVertically() const {
  return true;
}

bool DirectCompositionChildSurfaceWin::SupportsPostSubBuffer() {
  return true;
}

bool DirectCompositionChildSurfaceWin::OnMakeCurrent(gl::GLContext* context) {
  if (g_current_surface != dcomp_surface_.Get()) {
    if (g_current_surface) {
      HRESULT hr = g_current_surface->SuspendDraw();
      CHECK(SUCCEEDED(hr));
      g_current_surface = nullptr;
    }
    if (draw_texture_) {
      HRESULT hr = dcomp_surface_->ResumeDraw();
      CHECK(SUCCEEDED(hr));
      g_current_surface = dcomp_surface_.Get();
    }
  }
  return true;
}

bool DirectCompositionChildSurfaceWin::SupportsDCLayers() const {
  return true;
}

bool DirectCompositionChildSurfaceWin::SetDrawRectangle(
    const gfx::Rect& rectangle) {
  if (draw_texture_)
    return false;
  DCHECK(!real_surface_);
  ui::ScopedReleaseCurrent release_current(this);

  if ((enable_dc_layers_ && !dcomp_surface_) ||
      (!enable_dc_layers_ && !swap_chain_)) {
    ReleaseCurrentSurface();
    if (!InitializeSurface()) {
      LOG(ERROR) << "InitializeSurface failed";
      return false;
    }
  }

  if (!gfx::Rect(size_).Contains(rectangle)) {
    DLOG(ERROR) << "Draw rectangle must be contained within size of surface";
    return false;
  }
  if (gfx::Rect(size_) != rectangle && !has_been_rendered_to_) {
    DLOG(ERROR) << "First draw to surface must draw to everything";
    return false;
  }

  CHECK(!g_current_surface);

  RECT rect = rectangle.ToRECT();
  if (dcomp_surface_) {
    POINT update_offset;
    HRESULT hr = dcomp_surface_->BeginDraw(
        &rect, IID_PPV_ARGS(draw_texture_.GetAddressOf()), &update_offset);
    draw_offset_ = gfx::Point(update_offset) - gfx::Rect(rect).origin();
    CHECK(SUCCEEDED(hr));
  } else {
    HRESULT hr =
        swap_chain_->GetBuffer(0, IID_PPV_ARGS(draw_texture_.GetAddressOf()));
    swap_rect_ = rectangle;
    draw_offset_ = gfx::Vector2d();
    CHECK(SUCCEEDED(hr));
  }
  has_been_rendered_to_ = true;

  g_current_surface = dcomp_surface_.Get();

  std::vector<EGLint> pbuffer_attribs{
      EGL_WIDTH,
      size_.width(),
      EGL_HEIGHT,
      size_.height(),
      EGL_FLEXIBLE_SURFACE_COMPATIBILITY_SUPPORTED_ANGLE,
      EGL_TRUE,
      EGL_NONE};

  EGLClientBuffer buffer =
      reinterpret_cast<EGLClientBuffer>(draw_texture_.Get());
  real_surface_ = eglCreatePbufferFromClientBuffer(
      GetDisplay(), EGL_D3D_TEXTURE_ANGLE, buffer, GetConfig(),
      &pbuffer_attribs[0]);

  return true;
}

gfx::Vector2d DirectCompositionChildSurfaceWin::GetDrawOffset() const {
  return draw_offset_;
}

}  // namespace gpu
