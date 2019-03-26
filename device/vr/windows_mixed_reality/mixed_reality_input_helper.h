// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef DEVICE_VR_WINDOWS_MIXED_REALITY_MIXED_REALITY_INPUT_HELPER_H_
#define DEVICE_VR_WINDOWS_MIXED_REALITY_MIXED_REALITY_INPUT_HELPER_H_

#include <windows.perception.spatial.h>
#include <windows.ui.input.spatial.h>
#include <wrl.h>

#include <unordered_map>
#include <vector>

#include "device/vr/public/mojom/vr_service.mojom.h"

namespace device {
class MixedRealityInputHelper {
 public:
  MixedRealityInputHelper(HWND hwnd);
  virtual ~MixedRealityInputHelper();
  std::vector<mojom::XRInputSourceStatePtr> GetInputState(
      Microsoft::WRL::ComPtr<
          ABI::Windows::Perception::Spatial::ISpatialCoordinateSystem> origin,
      Microsoft::WRL::ComPtr<ABI::Windows::Perception::IPerceptionTimestamp>
          timestamp);

 private:
  bool EnsureSpatialInteractionManager();

  Microsoft::WRL::ComPtr<
      ABI::Windows::UI::Input::Spatial::ISpatialInteractionManager>
      spatial_interaction_manager_;
  std::unordered_map<uint32_t, bool> controller_pressed_state_;
  HWND hwnd_;

  DISALLOW_COPY_AND_ASSIGN(MixedRealityInputHelper);
};
}  // namespace device

#endif  // DEVICE_VR_WINDOWS_MIXED_REALITY_MIXED_REALITY_INPUT_HELPER_H_
