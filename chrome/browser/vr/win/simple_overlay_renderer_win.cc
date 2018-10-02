// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <dxgi1_2.h>

#include "chrome/browser/vr/win/simple_overlay_renderer_win.h"
#include "mojo/public/cpp/system/platform_handle.h"

namespace vr {

SimpleOverlayRenderer::SimpleOverlayRenderer() {}
SimpleOverlayRenderer::~SimpleOverlayRenderer() {}

void SimpleOverlayRenderer::Initialize() {
  D3D_FEATURE_LEVEL feature_levels[] = {D3D_FEATURE_LEVEL_11_1};
  D3D_FEATURE_LEVEL feature_level_out = D3D_FEATURE_LEVEL_11_1;
  D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, NULL, 0, feature_levels,
                    base::size(feature_levels), D3D11_SDK_VERSION,
                    device_.GetAddressOf(), &feature_level_out,
                    context_.GetAddressOf());
}

void SimpleOverlayRenderer::Render() {
  if (!device_)
    return;
  if (!texture_) {
    D3D11_TEXTURE2D_DESC desc = {
        128,
        128,
        1,
        1,
        DXGI_FORMAT_R8G8B8A8_UNORM,
        {1, 0},
        D3D11_USAGE_DEFAULT,
        D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET,
        0,
        D3D11_RESOURCE_MISC_SHARED_NTHANDLE |
            D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX};

    device_->CreateTexture2D(&desc, nullptr, texture_.GetAddressOf());
    rtv_ = nullptr;
  }

  if (texture_ && !rtv_) {
    D3D11_RENDER_TARGET_VIEW_DESC render_target_view_desc;
    render_target_view_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    render_target_view_desc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
    render_target_view_desc.Texture2D.MipSlice = 0;
    HRESULT hr = device_->CreateRenderTargetView(
        texture_.Get(), &render_target_view_desc, rtv_.GetAddressOf());
    if (FAILED(hr)) {
      texture_ = nullptr;
    }
  }

  if (texture_ && rtv_) {
    Microsoft::WRL::ComPtr<IDXGIKeyedMutex> mutex;
    HRESULT hr = texture_.As(&mutex);
    if (SUCCEEDED(hr)) {
      mutex->AcquireSync(0, INFINITE);

      static constexpr float color[4] = {1, 0, 0, 0.5};
      context_->ClearRenderTargetView(rtv_.Get(), color);

      mutex->ReleaseSync(1);
    } else {
      rtv_ = nullptr;
      texture_ = nullptr;
    }
  }
}

mojo::ScopedHandle SimpleOverlayRenderer::GetTexture() {
  mojo::ScopedHandle handle;

  Microsoft::WRL::ComPtr<IDXGIResource1> dxgi_resource;
  if (FAILED(texture_.CopyTo(dxgi_resource.GetAddressOf())))
    return handle;

  HANDLE texture_handle;
  if (FAILED(dxgi_resource->CreateSharedHandle(
          nullptr, DXGI_SHARED_RESOURCE_READ | DXGI_SHARED_RESOURCE_WRITE,
          nullptr, &texture_handle)))
    return handle;

  return mojo::WrapPlatformFile(texture_handle);
}

gfx::RectF SimpleOverlayRenderer::GetLeft() {
  return gfx::RectF(0, 0, 0.5, 1);
}

gfx::RectF SimpleOverlayRenderer::GetRight() {
  return gfx::RectF(0.5, 0, 0.5, 1);
}

}  // namespace vr
