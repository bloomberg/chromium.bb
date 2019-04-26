// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "device/vr/windows_mixed_reality/wrappers/wmr_holographic_space.h"

#include <HolographicSpaceInterop.h>
#include <Windows.Graphics.DirectX.Direct3D11.interop.h>
#include <windows.graphics.holographic.h>
#include <wrl.h>

#include <vector>

#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/win/core_winrt_util.h"
#include "base/win/scoped_hstring.h"
#include "device/vr/windows_mixed_reality/wrappers/wmr_holographic_frame.h"

using ABI::Windows::Graphics::DirectX::Direct3D11::IDirect3DDevice;
using ABI::Windows::Graphics::Holographic::HolographicAdapterId;
using ABI::Windows::Graphics::Holographic::IHolographicFrame;
using ABI::Windows::Graphics::Holographic::IHolographicSpace;
using Microsoft::WRL::ComPtr;

namespace device {
std::unique_ptr<WMRHolographicSpace> WMRHolographicSpace::CreateForWindow(
    HWND hwnd) {
  if (!hwnd)
    return nullptr;

  ComPtr<IHolographicSpaceInterop> holographic_space_interop;
  base::win::ScopedHString holographic_space_string =
      base::win::ScopedHString::Create(
          RuntimeClass_Windows_Graphics_Holographic_HolographicSpace);
  HRESULT hr = base::win::RoGetActivationFactory(
      holographic_space_string.get(), IID_PPV_ARGS(&holographic_space_interop));

  if (FAILED(hr))
    return nullptr;

  ComPtr<IHolographicSpace> holographic_space;
  hr = holographic_space_interop->CreateForWindow(
      hwnd, IID_PPV_ARGS(&holographic_space));

  if (FAILED(hr))
    return nullptr;

  return std::make_unique<WMRHolographicSpace>(holographic_space);
}

WMRHolographicSpace::WMRHolographicSpace(ComPtr<IHolographicSpace> space)
    : space_(space) {
  DCHECK(space_);
}

WMRHolographicSpace::~WMRHolographicSpace() = default;

HolographicAdapterId WMRHolographicSpace::PrimaryAdapterId() {
  HolographicAdapterId id;
  HRESULT hr = space_->get_PrimaryAdapterId(&id);
  DCHECK(SUCCEEDED(hr));
  return id;
}

std::unique_ptr<WMRHolographicFrame> WMRHolographicSpace::TryCreateNextFrame() {
  ComPtr<IHolographicFrame> frame;
  HRESULT hr = space_->CreateNextFrame(&frame);
  if (FAILED(hr))
    return nullptr;
  return std::make_unique<WMRHolographicFrame>(frame);
}

bool WMRHolographicSpace::TrySetDirect3D11Device(
    const ComPtr<IDirect3DDevice>& device) {
  HRESULT hr = space_->SetDirect3D11Device(device.Get());
  return SUCCEEDED(hr);
}
}  // namespace device
