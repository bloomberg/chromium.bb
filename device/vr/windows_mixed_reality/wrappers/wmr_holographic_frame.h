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

  virtual std::unique_ptr<WMRTimestamp> Timestamp();
  virtual std::vector<std::unique_ptr<WMRCameraPose>> CameraPoses();

 protected:
  // Necessary so subclasses don't call the explicit constructor.
  WMRHolographicFramePrediction();

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

  virtual std::unique_ptr<WMRHolographicFramePrediction> CurrentPrediction();
  virtual std::unique_ptr<WMRRenderingParameters> TryGetRenderingParameters(
      const WMRCameraPose* pose);
  virtual bool TryPresentUsingCurrentPrediction();

 protected:
  // Necessary so subclasses don't call the explicit constructor.
  WMRHolographicFrame();

 private:
  Microsoft::WRL::ComPtr<ABI::Windows::Graphics::Holographic::IHolographicFrame>
      holographic_frame_;

  DISALLOW_COPY_AND_ASSIGN(WMRHolographicFrame);
};
}  // namespace device
#endif  // DEVICE_VR_WINDOWS_MIXED_REALITY_WRAPPERS_WMR_HOLOGRAPHIC_FRAME_H_
