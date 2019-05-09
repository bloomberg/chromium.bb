// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/windows_mixed_reality/wrappers/test/mock_wmr_input_manager.h"
#include "device/vr/windows_mixed_reality/wrappers/wmr_input_source_state.h"

namespace device {

// MockWMRInputManager
MockWMRInputManager::MockWMRInputManager() {}

MockWMRInputManager::~MockWMRInputManager() = default;

std::vector<WMRInputSourceState>
MockWMRInputManager::GetDetectedSourcesAtTimestamp(
    Microsoft::WRL::ComPtr<ABI::Windows::Perception::IPerceptionTimestamp>
        timestamp) const {
  return {};
}

}  // namespace device
