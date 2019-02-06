// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/windows_mixed_reality/mixed_reality_renderloop.h"

#include <vector>

#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/com_init_util.h"
#include "base/win/core_winrt_util.h"
#include "base/win/scoped_hstring.h"
#include "ui/gfx/geometry/angle_conversions.h"
#include "ui/gfx/geometry/vector3d_f.h"
#include "ui/gfx/transform.h"
#include "ui/gfx/transform_util.h"

#if defined(OS_WIN)
#include "device/vr/windows/d3d11_texture_helper.h"
#endif

#include <HolographicSpaceInterop.h>
#include <Windows.Graphics.DirectX.Direct3D11.interop.h>
#include <windows.graphics.holographic.h>
#include <windows.perception.h>

namespace device {

using namespace ABI::Windows::Foundation;
using namespace ABI::Windows::Graphics::Holographic;
using namespace ABI::Windows::Perception::Spatial;
using namespace Microsoft::WRL;
using namespace Microsoft::WRL::Wrappers;

BOOL MixedRealityWindow::ProcessWindowMessage(HWND window,
                                              UINT message,
                                              WPARAM w_param,
                                              LPARAM l_param,
                                              LRESULT& result,
                                              DWORD msg_map_id) {
  return false;  // We don't currently handle any messages ourselves.
}

MixedRealityRenderLoop::MixedRealityRenderLoop() : XRCompositorCommon() {}

MixedRealityRenderLoop::~MixedRealityRenderLoop() {
  Stop();
}

bool MixedRealityRenderLoop::PreComposite() {
  // Not yet implemented.
  return false;
}

bool MixedRealityRenderLoop::SubmitCompositedFrame() {
  // Not yet implemented.
  return false;
}

bool MixedRealityRenderLoop::StartRuntime() {
  initializer_ = std::make_unique<base::win::ScopedWinrtInitializer>();

  InitializeSpace();
  if (!holographic_space_)
    return false;

  ABI::Windows::Graphics::Holographic::HolographicAdapterId id;
  HRESULT hr = holographic_space_->get_PrimaryAdapterId(&id);
  if (FAILED(hr))
    return false;

  LUID adapter_luid;
  adapter_luid.HighPart = id.HighPart;
  adapter_luid.LowPart = id.LowPart;
  texture_helper_.SetUseBGRA(true);
  if (!texture_helper_.SetAdapterLUID(adapter_luid) ||
      !texture_helper_.EnsureInitialized()) {
    return false;
  }

  // Associate our holographic space with our directx device.
  ComPtr<IDXGIDevice> dxgi_device;
  hr = texture_helper_.GetDevice().As(&dxgi_device);
  if (FAILED(hr))
    return false;

  ComPtr<IInspectable> spInsp;
  hr = CreateDirect3D11DeviceFromDXGIDevice(dxgi_device.Get(), &spInsp);
  if (FAILED(hr))
    return false;

  ComPtr<ABI::Windows::Graphics::DirectX::Direct3D11::IDirect3DDevice> device;
  hr = spInsp.As(&device);
  if (FAILED(hr))
    return false;

  hr = holographic_space_->SetDirect3D11Device(device.Get());
  return SUCCEEDED(hr);
}

void MixedRealityRenderLoop::StopRuntime() {
  ShowWindow(window_->hwnd(), SW_HIDE);
  holographic_space_ = nullptr;
  DestroyWindow(window_->hwnd());
  window_ = nullptr;

  if (initializer_)
    initializer_ = nullptr;
}

void MixedRealityRenderLoop::OnSessionStart() {
  StartPresenting();
}

void MixedRealityRenderLoop::InitializeSpace() {
  // Create a Window, which is required to get an IHolographicSpace.
  // TODO(billorr): Finalize what is rendered to this window, its title, and
  // where the window is shown.
  window_ = std::make_unique<MixedRealityWindow>();
  window_->Init(NULL, gfx::Rect());

  // Create a holographic space from that Window.
  ComPtr<IHolographicSpaceInterop> holographic_space_interop;
  base::win::ScopedHString holographic_space_string =
      base::win::ScopedHString::Create(
          RuntimeClass_Windows_Graphics_Holographic_HolographicSpace);
  HRESULT hr = base::win::RoGetActivationFactory(
      holographic_space_string.get(), IID_PPV_ARGS(&holographic_space_interop));

  if (SUCCEEDED(hr)) {
    hr = holographic_space_interop->CreateForWindow(
        window_->hwnd(), IID_PPV_ARGS(&holographic_space_));
  }
}

void MixedRealityRenderLoop::StartPresenting() {
  ShowWindow(window_->hwnd(), SW_SHOW);
}

mojom::XRGamepadDataPtr MixedRealityRenderLoop::GetNextGamepadData() {
  // Not yet implemented.
  return nullptr;
}

mojom::XRFrameDataPtr MixedRealityRenderLoop::GetNextFrameData() {
  // Not yet implemented.
  return nullptr;
}

void MixedRealityRenderLoop::GetEnvironmentIntegrationProvider(
    mojom::XREnvironmentIntegrationProviderAssociatedRequest
        environment_provider) {
  // Environment integration is not supported. This call should not
  // be made on this device.
  mojo::ReportBadMessage("Environment integration is not supported.");
  return;
}

std::vector<mojom::XRInputSourceStatePtr>
MixedRealityRenderLoop::GetInputState() {
  // Not yet implemented.
  return {};
}

}  // namespace device
