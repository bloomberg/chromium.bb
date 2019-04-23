// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "device/vr/windows_mixed_reality/wrappers/wmr_input_manager.h"

#include <SpatialInteractionManagerInterop.h>
#include <windows.perception.h>
#include <windows.ui.input.spatial.h>
#include <wrl.h>

#include <memory>
#include <vector>

#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/win/core_winrt_util.h"
#include "base/win/scoped_hstring.h"
#include "device/vr/windows_mixed_reality/wrappers/wmr_input_source_state.h"

using ABI::Windows::Foundation::Collections::IVectorView;
using ABI::Windows::Perception::IPerceptionTimestamp;
using ABI::Windows::UI::Input::Spatial::ISpatialInteractionManager;
using ABI::Windows::UI::Input::Spatial::ISpatialInteractionSourceState;
using ABI::Windows::UI::Input::Spatial::SpatialInteractionSourceState;
using Microsoft::WRL::ComPtr;

namespace device {
std::unique_ptr<WMRInputManager> WMRInputManager::GetForWindow(HWND hwnd) {
  if (!hwnd)
    return nullptr;

  ComPtr<ISpatialInteractionManagerInterop> spatial_interaction_interop;
  base::win::ScopedHString spatial_interaction_interop_string =
      base::win::ScopedHString::Create(
          RuntimeClass_Windows_UI_Input_Spatial_SpatialInteractionManager);
  HRESULT hr = base::win::RoGetActivationFactory(
      spatial_interaction_interop_string.get(),
      IID_PPV_ARGS(&spatial_interaction_interop));
  if (FAILED(hr))
    return nullptr;

  ComPtr<ISpatialInteractionManager> manager;
  hr = spatial_interaction_interop->GetForWindow(hwnd, IID_PPV_ARGS(&manager));
  if (FAILED(hr))
    return nullptr;

  return std::make_unique<WMRInputManager>(manager);
}

WMRInputManager::WMRInputManager(ComPtr<ISpatialInteractionManager> manager)
    : manager_(manager) {
  DCHECK(manager_);
}

WMRInputManager::~WMRInputManager() = default;

std::vector<WMRInputSourceState> WMRInputManager::GetDetectedSourcesAtTimestamp(
    ComPtr<IPerceptionTimestamp> timestamp) const {
  std::vector<WMRInputSourceState> input_states;
  ComPtr<IVectorView<SpatialInteractionSourceState*>> source_states;
  if (FAILED(manager_->GetDetectedSourcesAtTimestamp(timestamp.Get(),
                                                     &source_states)))
    return input_states;

  uint32_t size;
  HRESULT hr = source_states->get_Size(&size);
  DCHECK(SUCCEEDED(hr));

  for (uint32_t i = 0; i < size; i++) {
    ComPtr<ISpatialInteractionSourceState> source_state_wmr;
    hr = source_states->GetAt(i, &source_state_wmr);
    DCHECK(SUCCEEDED(hr));
    input_states.emplace_back(source_state_wmr);
  }

  return input_states;
}

ComPtr<ISpatialInteractionManager> WMRInputManager::GetComPtr() const {
  return manager_;
}
}  // namespace device
