// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef DEVICE_VR_WINDOWS_MIXED_REALITY_WRAPPERS_WMR_INPUT_MANAGER_H_
#define DEVICE_VR_WINDOWS_MIXED_REALITY_WRAPPERS_WMR_INPUT_MANAGER_H_

#include <windows.perception.h>
#include <windows.ui.input.spatial.h>
#include <wrl.h>

#include <memory>
#include <vector>

#include "base/macros.h"

namespace device {
class WMRInputSourceState;
class WMRInputManager {
 public:
  static std::unique_ptr<WMRInputManager> GetForWindow(HWND hwnd);

  explicit WMRInputManager(
      Microsoft::WRL::ComPtr<
          ABI::Windows::UI::Input::Spatial::ISpatialInteractionManager>
          manager);
  virtual ~WMRInputManager();

  std::vector<WMRInputSourceState> GetDetectedSourcesAtTimestamp(
      Microsoft::WRL::ComPtr<ABI::Windows::Perception::IPerceptionTimestamp>
          timestamp) const;

  // TODO(954413): Remove this once the events that are used have been wrapped.
  Microsoft::WRL::ComPtr<
      ABI::Windows::UI::Input::Spatial::ISpatialInteractionManager>
  GetComPtr() const;

 private:
  Microsoft::WRL::ComPtr<
      ABI::Windows::UI::Input::Spatial::ISpatialInteractionManager>
      manager_;

  DISALLOW_COPY_AND_ASSIGN(WMRInputManager);
};
}  // namespace device

#endif  // DEVICE_VR_WINDOWS_MIXED_REALITY_WRAPPERS_WMR_INPUT_MANAGER_H_
