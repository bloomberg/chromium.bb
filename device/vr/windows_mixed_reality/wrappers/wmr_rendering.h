// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef DEVICE_VR_WINDOWS_MIXED_REALITY_WRAPPERS_WMR_RENDERING_H_
#define DEVICE_VR_WINDOWS_MIXED_REALITY_WRAPPERS_WMR_RENDERING_H_

#include <d3d11.h>
#include <windows.graphics.holographic.h>
#include <wrl.h>

#include <memory>
#include <vector>

#include "base/macros.h"

namespace device {
class WMRCoordinateSystem;

class WMRCamera {
 public:
  explicit WMRCamera(
      Microsoft::WRL::ComPtr<
          ABI::Windows::Graphics::Holographic::IHolographicCamera> camera);
  virtual ~WMRCamera();

  virtual ABI::Windows::Foundation::Size RenderTargetSize();
  virtual bool IsStereo();

 protected:
  // Necessary so subclasses don't call the explicit constructor.
  WMRCamera();

 private:
  Microsoft::WRL::ComPtr<
      ABI::Windows::Graphics::Holographic::IHolographicCamera>
      camera_;

  DISALLOW_COPY_AND_ASSIGN(WMRCamera);
};

class WMRCameraPose {
 public:
  explicit WMRCameraPose(
      Microsoft::WRL::ComPtr<
          ABI::Windows::Graphics::Holographic::IHolographicCameraPose> pose);
  virtual ~WMRCameraPose();

  virtual ABI::Windows::Foundation::Rect Viewport();
  virtual std::unique_ptr<WMRCamera> HolographicCamera();
  virtual ABI::Windows::Graphics::Holographic::HolographicStereoTransform
  ProjectionTransform();
  virtual bool TryGetViewTransform(
      const WMRCoordinateSystem* origin,
      ABI::Windows::Graphics::Holographic::HolographicStereoTransform*
          transform);
  ABI::Windows::Graphics::Holographic::IHolographicCameraPose* GetRawPtr()
      const;

 protected:
  // Necessary so subclasses don't call the explicit constructor.
  WMRCameraPose();

 private:
  Microsoft::WRL::ComPtr<
      ABI::Windows::Graphics::Holographic::IHolographicCameraPose>
      pose_;

  DISALLOW_COPY_AND_ASSIGN(WMRCameraPose);
};

class WMRRenderingParameters {
 public:
  explicit WMRRenderingParameters(
      Microsoft::WRL::ComPtr<ABI::Windows::Graphics::Holographic::
                                 IHolographicCameraRenderingParameters>
          rendering_params);
  virtual ~WMRRenderingParameters();

  virtual Microsoft::WRL::ComPtr<ID3D11Texture2D> TryGetBackbufferAsTexture2D();

 protected:
  // Necessary so subclasses don't call the explicit constructor.
  WMRRenderingParameters();

 private:
  Microsoft::WRL::ComPtr<ABI::Windows::Graphics::Holographic::
                             IHolographicCameraRenderingParameters>
      rendering_params_;

  DISALLOW_COPY_AND_ASSIGN(WMRRenderingParameters);
};
}  // namespace device
#endif  // DEVICE_VR_WINDOWS_MIXED_REALITY_WRAPPERS_WMR_RENDERING_H_
