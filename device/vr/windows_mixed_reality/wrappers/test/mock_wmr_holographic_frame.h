// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef DEVICE_VR_WINDOWS_MIXED_REALITY_WRAPPERS_TEST_MOCK_WMR_HOLOGRAPHIC_FRAME_H_
#define DEVICE_VR_WINDOWS_MIXED_REALITY_WRAPPERS_TEST_MOCK_WMR_HOLOGRAPHIC_FRAME_H_

#include "device/vr/windows_mixed_reality/wrappers/wmr_holographic_frame.h"

namespace device {

class MockWMRHolographicFramePrediction : public WMRHolographicFramePrediction {
 public:
  MockWMRHolographicFramePrediction();
  ~MockWMRHolographicFramePrediction() override;

  std::unique_ptr<WMRTimestamp> Timestamp() override;
  std::vector<std::unique_ptr<WMRCameraPose>> CameraPoses() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(MockWMRHolographicFramePrediction);
};

class MockWMRHolographicFrame : public WMRHolographicFrame {
 public:
  MockWMRHolographicFrame();
  ~MockWMRHolographicFrame() override;

  std::unique_ptr<WMRHolographicFramePrediction> CurrentPrediction() override;
  std::unique_ptr<WMRRenderingParameters> TryGetRenderingParameters(
      const WMRCameraPose* pose) override;
  bool TryPresentUsingCurrentPrediction() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(MockWMRHolographicFrame);
};

}  // namespace device

#endif  // DEVICE_VR_WINDOWS_MIXED_REALITY_WRAPPERS_TEST_MOCK_WMR_HOLOGRAPHIC_FRAME_H_
