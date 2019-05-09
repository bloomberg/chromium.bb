// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef DEVICE_VR_WINDOWS_MIXED_REALITY_WRAPPERS_TEST_MOCK_WMR_INPUT_MANAGER_H_
#define DEVICE_VR_WINDOWS_MIXED_REALITY_WRAPPERS_TEST_MOCK_WMR_INPUT_MANAGER_H_

#include "device/vr/windows_mixed_reality/wrappers/wmr_input_manager.h"

namespace device {

class MockWMRInputManager : public WMRInputManager {
 public:
  MockWMRInputManager();
  ~MockWMRInputManager() override;

  std::vector<WMRInputSourceState> GetDetectedSourcesAtTimestamp(
      Microsoft::WRL::ComPtr<ABI::Windows::Perception::IPerceptionTimestamp>
          timestamp) const override;

 private:
  DISALLOW_COPY_AND_ASSIGN(MockWMRInputManager);
};

}  // namespace device

#endif  // DEVICE_VR_WINDOWS_MIXED_REALITY_WRAPPERS_TEST_MOCK_WMR_INPUT_MANAGER_H_
