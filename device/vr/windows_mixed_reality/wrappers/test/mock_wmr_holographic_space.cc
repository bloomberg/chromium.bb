// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/windows_mixed_reality/wrappers/test/mock_wmr_holographic_space.h"

#include <D3D11_1.h>
#include <dxgi.h>
#include <wrl.h>

#include "base/stl_util.h"

#include "device/vr/windows_mixed_reality/wrappers/test/mock_wmr_holographic_frame.h"

namespace device {

MockWMRHolographicSpace::MockWMRHolographicSpace() {}

MockWMRHolographicSpace::~MockWMRHolographicSpace() = default;

ABI::Windows::Graphics::Holographic::HolographicAdapterId
MockWMRHolographicSpace::PrimaryAdapterId() {
  ABI::Windows::Graphics::Holographic::HolographicAdapterId ret;

  // This is currently duplicated from the mock OpenVR implementation. It can
  // be de-duped once https://crrev.com/c/1575030, which moves the DX11.1
  // helper code into a standalone library.
  // Enumerate devices until we find one that supports 11.1.
  int32_t adapter_index = -1;
  Microsoft::WRL::ComPtr<IDXGIFactory1> dxgi_factory;
  Microsoft::WRL::ComPtr<IDXGIAdapter> adapter;
  bool success = SUCCEEDED(CreateDXGIFactory1(IID_PPV_ARGS(&dxgi_factory)));
  DCHECK(success);
  for (int i = 0; SUCCEEDED(dxgi_factory->EnumAdapters(i, &adapter)); ++i) {
    D3D_FEATURE_LEVEL feature_levels[] = {D3D_FEATURE_LEVEL_11_1};
    UINT flags = 0;
    D3D_FEATURE_LEVEL feature_level_out = D3D_FEATURE_LEVEL_11_1;

    Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> d3d11_device_context;
    if (SUCCEEDED(D3D11CreateDevice(
            adapter.Get(), D3D_DRIVER_TYPE_UNKNOWN, NULL, flags, feature_levels,
            base::size(feature_levels), D3D11_SDK_VERSION, &d3d11_device,
            &feature_level_out, &d3d11_device_context))) {
      adapter_index = i;
      break;
    }
  }

  if (adapter_index == -1)
    return ret;

  DXGI_ADAPTER_DESC description;
  success = SUCCEEDED(adapter->GetDesc(&description));
  if (!success) {
    return ret;
  }

  ret.LowPart = description.AdapterLuid.LowPart;
  ret.HighPart = description.AdapterLuid.HighPart;

  return ret;
}

std::unique_ptr<WMRHolographicFrame>
MockWMRHolographicSpace::TryCreateNextFrame() {
  return std::make_unique<MockWMRHolographicFrame>();
}

bool MockWMRHolographicSpace::TrySetDirect3D11Device(
    const Microsoft::WRL::ComPtr<
        ABI::Windows::Graphics::DirectX::Direct3D11::IDirect3DDevice>& device) {
  return true;
}

}  // namespace device
