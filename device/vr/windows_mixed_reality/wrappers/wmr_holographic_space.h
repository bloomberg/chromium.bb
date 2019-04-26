// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef DEVICE_VR_WINDOWS_MIXED_REALITY_WRAPPERS_WMR_HOLOGRAPHIC_SPACE_H_
#define DEVICE_VR_WINDOWS_MIXED_REALITY_WRAPPERS_WMR_HOLOGRAPHIC_SPACE_H_

#include <windows.graphics.holographic.h>
#include <wrl.h>

#include <memory>

#include "base/macros.h"

namespace device {
class WMRHolographicFrame;
class WMRHolographicSpace {
 public:
  static std::unique_ptr<WMRHolographicSpace> CreateForWindow(HWND hwnd);
  explicit WMRHolographicSpace(
      Microsoft::WRL::ComPtr<
          ABI::Windows::Graphics::Holographic::IHolographicSpace> space);
  virtual ~WMRHolographicSpace();

  ABI::Windows::Graphics::Holographic::HolographicAdapterId PrimaryAdapterId();
  std::unique_ptr<WMRHolographicFrame> TryCreateNextFrame();
  bool TrySetDirect3D11Device(
      const Microsoft::WRL::ComPtr<
          ABI::Windows::Graphics::DirectX::Direct3D11::IDirect3DDevice>&
          device);

 private:
  Microsoft::WRL::ComPtr<ABI::Windows::Graphics::Holographic::IHolographicSpace>
      space_;

  DISALLOW_COPY_AND_ASSIGN(WMRHolographicSpace);
};
}  // namespace device

#endif  // DEVICE_VR_WINDOWS_MIXED_REALITY_WRAPPERS_WMR_HOLOGRAPHIC_SPACE_H_
