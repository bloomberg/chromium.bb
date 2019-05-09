// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "device/vr/windows_mixed_reality/wrappers/wmr_rendering.h"

#include <Windows.Graphics.DirectX.Direct3D11.interop.h>
#include <windows.graphics.holographic.h>
#include <wrl.h>

#include <memory>
#include <vector>

#include "device/vr/windows/d3d11_texture_helper.h"
#include "device/vr/windows_mixed_reality/wrappers/wmr_origins.h"

namespace WF = ABI::Windows::Foundation;
using ABI::Windows::Foundation::IReference;
using ABI::Windows::Graphics::DirectX::Direct3D11::IDirect3DSurface;
using ABI::Windows::Graphics::Holographic::HolographicStereoTransform;
using ABI::Windows::Graphics::Holographic::IHolographicCamera;
using ABI::Windows::Graphics::Holographic::IHolographicCameraPose;
using ABI::Windows::Graphics::Holographic::
    IHolographicCameraRenderingParameters;
using Microsoft::WRL::ComPtr;
using Windows::Graphics::DirectX::Direct3D11::IDirect3DDxgiInterfaceAccess;

namespace device {
// WMRCamera
WMRCamera::WMRCamera(ComPtr<IHolographicCamera> camera) : camera_(camera) {
  DCHECK(camera_);
}

WMRCamera::WMRCamera() {}

WMRCamera::~WMRCamera() = default;

WF::Size WMRCamera::RenderTargetSize() {
  WF::Size val;
  HRESULT hr = camera_->get_RenderTargetSize(&val);
  DCHECK(SUCCEEDED(hr));
  return val;
}

bool WMRCamera::IsStereo() {
  boolean val;
  HRESULT hr = camera_->get_IsStereo(&val);
  DCHECK(SUCCEEDED(hr));
  return val;
}

// WMRCameraPose
WMRCameraPose::WMRCameraPose(ComPtr<IHolographicCameraPose> pose)
    : pose_(pose) {
  DCHECK(pose_);
}

WMRCameraPose::WMRCameraPose() {}

WMRCameraPose::~WMRCameraPose() = default;

WF::Rect WMRCameraPose::Viewport() {
  WF::Rect val;
  HRESULT hr = pose_->get_Viewport(&val);
  DCHECK(SUCCEEDED(hr));
  return val;
}

std::unique_ptr<WMRCamera> WMRCameraPose::HolographicCamera() {
  ComPtr<IHolographicCamera> camera;
  HRESULT hr = pose_->get_HolographicCamera(&camera);
  DCHECK(SUCCEEDED(hr));
  return std::make_unique<WMRCamera>(camera);
}

HolographicStereoTransform WMRCameraPose::ProjectionTransform() {
  HolographicStereoTransform val;
  HRESULT hr = pose_->get_ProjectionTransform(&val);
  DCHECK(SUCCEEDED(hr));
  return val;
}

bool WMRCameraPose::TryGetViewTransform(const WMRCoordinateSystem* origin,
                                        HolographicStereoTransform* transform) {
  ComPtr<IReference<HolographicStereoTransform>> transform_ref;
  if (FAILED(pose_->TryGetViewTransform(origin->GetRawPtr(), &transform_ref)) ||
      !transform_ref)
    return false;

  HRESULT hr = transform_ref->get_Value(transform);
  return SUCCEEDED(hr);
}

IHolographicCameraPose* WMRCameraPose::GetRawPtr() const {
  return pose_.Get();
}

// WMRRenderingParameters
WMRRenderingParameters::WMRRenderingParameters(
    ComPtr<IHolographicCameraRenderingParameters> rendering_params)
    : rendering_params_(rendering_params) {
  DCHECK(rendering_params_);
}

WMRRenderingParameters::WMRRenderingParameters() {}

WMRRenderingParameters::~WMRRenderingParameters() = default;

ComPtr<ID3D11Texture2D> WMRRenderingParameters::TryGetBackbufferAsTexture2D() {
  ComPtr<IDirect3DSurface> surface;
  if (FAILED(rendering_params_->get_Direct3D11BackBuffer(&surface)))
    return nullptr;

  ComPtr<IDirect3DDxgiInterfaceAccess> dxgi_interface_access;
  if (FAILED(surface.As(&dxgi_interface_access)))
    return nullptr;

  ComPtr<ID3D11Resource> native_resource;
  if (FAILED(
          dxgi_interface_access->GetInterface(IID_PPV_ARGS(&native_resource))))
    return nullptr;

  ComPtr<ID3D11Texture2D> texture;
  if (FAILED(native_resource.As(&texture)))
    return nullptr;

  return texture;
}
}  // namespace device
