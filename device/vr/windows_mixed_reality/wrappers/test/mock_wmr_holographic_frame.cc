// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/windows_mixed_reality/wrappers/test/mock_wmr_holographic_frame.h"

#include "device/vr/test/test_hook.h"
#include "device/vr/windows_mixed_reality/mixed_reality_statics.h"
#include "device/vr/windows_mixed_reality/wrappers/test/mock_wmr_rendering.h"
#include "device/vr/windows_mixed_reality/wrappers/test/mock_wmr_timestamp.h"
#include "device/vr/windows_mixed_reality/wrappers/wmr_rendering.h"

namespace device {

// MockWMRHolographicFramePrediction
MockWMRHolographicFramePrediction::MockWMRHolographicFramePrediction() {}

MockWMRHolographicFramePrediction::~MockWMRHolographicFramePrediction() =
    default;

std::unique_ptr<WMRTimestamp> MockWMRHolographicFramePrediction::Timestamp() {
  return std::make_unique<MockWMRTimestamp>();
}

std::vector<std::unique_ptr<WMRCameraPose>>
MockWMRHolographicFramePrediction::CameraPoses() {
  std::vector<std::unique_ptr<WMRCameraPose>> ret;
  // Production code only expects a single camera pose.
  ret.push_back(std::make_unique<MockWMRCameraPose>());
  return ret;
}

// MockWMRHolographicFrame
MockWMRHolographicFrame::MockWMRHolographicFrame(
    const Microsoft::WRL::ComPtr<ID3D11Device>& device)
    : d3d11_device_(device) {}

MockWMRHolographicFrame::~MockWMRHolographicFrame() = default;

std::unique_ptr<WMRHolographicFramePrediction>
MockWMRHolographicFrame::CurrentPrediction() {
  return std::make_unique<MockWMRHolographicFramePrediction>();
}

std::unique_ptr<WMRRenderingParameters>
MockWMRHolographicFrame::TryGetRenderingParameters(const WMRCameraPose* pose) {
  return std::make_unique<MockWMRRenderingParameters>(d3d11_device_);
}

bool MockWMRHolographicFrame::TryPresentUsingCurrentPrediction() {
  // TODO(https://crbug.com/926048): Actually pass in correct data.
  SubmittedFrameData data;
  auto locked_hook = MixedRealityDeviceStatics::GetLockedTestHook();
  if (locked_hook.GetHook()) {
    locked_hook.GetHook()->OnFrameSubmitted(data);
  }
  return true;
}

}  // namespace device
