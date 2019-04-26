// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef DEVICE_VR_WINDOWS_MIXED_REALITY_WRAPPERS_WMR_HOLOGRAPHIC_FRAME_H_
#define DEVICE_VR_WINDOWS_MIXED_REALITY_WRAPPERS_WMR_HOLOGRAPHIC_FRAME_H_

#include <windows.graphics.holographic.h>
#include <wrl.h>

#include <memory>
#include <vector>

#include "base/macros.h"

namespace device {
class WMRCameraPose;
class WMRRenderingParameters;
class WMRTimestamp;

class WMRHolographicFramePrediction {
 public:
  explicit WMRHolographicFramePrediction(
      Microsoft::WRL::ComPtr<
          ABI::Windows::Graphics::Holographic::IHolographicFramePrediction>
          prediction);
  virtual ~WMRHolographicFramePrediction();

  std::unique_ptr<WMRTimestamp> Timestamp();
  std::vector<std::unique_ptr<WMRCameraPose>> CameraPoses();

 private:
  Microsoft::WRL::ComPtr<
      ABI::Windows::Graphics::Holographic::IHolographicFramePrediction>
      prediction_;

  DISALLOW_COPY_AND_ASSIGN(WMRHolographicFramePrediction);
};

class WMRHolographicFrame {
 public:
  explicit WMRHolographicFrame(
      Microsoft::WRL::ComPtr<
          ABI::Windows::Graphics::Holographic::IHolographicFrame>
          holographic_frame);
  virtual ~WMRHolographicFrame();

  std::unique_ptr<WMRHolographicFramePrediction> CurrentPrediction();
  std::unique_ptr<WMRRenderingParameters> TryGetRenderingParameters(
      const WMRCameraPose* pose);
  bool TryPresentUsingCurrentPrediction();

 private:
  Microsoft::WRL::ComPtr<ABI::Windows::Graphics::Holographic::IHolographicFrame>
      holographic_frame_;

  DISALLOW_COPY_AND_ASSIGN(WMRHolographicFrame);
};
}  // namespace device
#endif  // DEVICE_VR_WINDOWS_MIXED_REALITY_WRAPPERS_WMR_HOLOGRAPHIC_FRAME_H_
