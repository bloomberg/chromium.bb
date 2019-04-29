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

#include "base/callback_list.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/win/core_winrt_util.h"
#include "base/win/scoped_hstring.h"
#include "device/vr/windows_mixed_reality/wrappers/wmr_input_source_state.h"

using ABI::Windows::Foundation::ITypedEventHandler;
using ABI::Windows::Foundation::Collections::IVectorView;
using ABI::Windows::Perception::IPerceptionTimestamp;
using ABI::Windows::UI::Input::Spatial::ISpatialInteractionManager;
using ABI::Windows::UI::Input::Spatial::ISpatialInteractionSourceEventArgs;
using ABI::Windows::UI::Input::Spatial::ISpatialInteractionSourceEventArgs2;
using ABI::Windows::UI::Input::Spatial::ISpatialInteractionSourceState;
using ABI::Windows::UI::Input::Spatial::SpatialInteractionManager;
using ABI::Windows::UI::Input::Spatial::SpatialInteractionSourceEventArgs;
using ABI::Windows::UI::Input::Spatial::SpatialInteractionSourceState;
using Microsoft::WRL::Callback;
using Microsoft::WRL::ComPtr;

using WMRPressKind =
    ABI::Windows::UI::Input::Spatial::SpatialInteractionPressKind;

typedef ITypedEventHandler<SpatialInteractionManager*,
                           SpatialInteractionSourceEventArgs*>
    SpatialInteractionSourceEventHandler;

namespace device {
WMRInputSourceEventArgs::WMRInputSourceEventArgs(
    ComPtr<ISpatialInteractionSourceEventArgs> args)
    : args_(args) {
  DCHECK(args_);
  HRESULT hr = args_.As(&args2_);
  DCHECK(SUCCEEDED(hr));
}

WMRInputSourceEventArgs::~WMRInputSourceEventArgs() = default;

WMRPressKind WMRInputSourceEventArgs::PressKind() const {
  WMRPressKind press_kind;
  HRESULT hr = args2_->get_PressKind(&press_kind);
  DCHECK(SUCCEEDED(hr));
  return press_kind;
}

WMRInputSourceState WMRInputSourceEventArgs::State() const {
  ComPtr<ISpatialInteractionSourceState> wmr_state;
  HRESULT hr = args_->get_State(&wmr_state);
  DCHECK(SUCCEEDED(hr));
  return WMRInputSourceState(wmr_state);
}

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
  pressed_token_.value = 0;
  released_token_.value = 0;
  SubscribeEvents();
}

WMRInputManager::~WMRInputManager() {
  UnsubscribeEvents();
}

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

std::unique_ptr<WMRInputManager::InputEventCallbackList::Subscription>
WMRInputManager::AddPressedCallback(const InputEventCallback& cb) {
  return pressed_callback_list_.Add(cb);
}

std::unique_ptr<WMRInputManager::InputEventCallbackList::Subscription>
WMRInputManager::AddReleasedCallback(const InputEventCallback& cb) {
  return released_callback_list_.Add(cb);
}

HRESULT WMRInputManager::OnSourcePressed(
    ISpatialInteractionManager* sender,
    ISpatialInteractionSourceEventArgs* raw_args) {
  ComPtr<ISpatialInteractionSourceEventArgs> wmr_args(raw_args);
  WMRInputSourceEventArgs args(wmr_args);
  pressed_callback_list_.Notify(args);
  return S_OK;
}

HRESULT WMRInputManager::OnSourceReleased(
    ISpatialInteractionManager* sender,
    ISpatialInteractionSourceEventArgs* raw_args) {
  ComPtr<ISpatialInteractionSourceEventArgs> wmr_args(raw_args);
  WMRInputSourceEventArgs args(wmr_args);
  released_callback_list_.Notify(args);
  return S_OK;
}

void WMRInputManager::SubscribeEvents() {
  DCHECK(manager_);
  DCHECK(pressed_token_.value == 0);
  DCHECK(released_token_.value == 0);

  // The destructor ensures that we're unsubscribed so raw this is fine.
  auto pressed_callback = Callback<SpatialInteractionSourceEventHandler>(
      this, &WMRInputManager::OnSourcePressed);
  HRESULT hr =
      manager_->add_SourcePressed(pressed_callback.Get(), &pressed_token_);
  DCHECK(SUCCEEDED(hr));

  // The destructor ensures that we're unsubscribed so raw this is fine.
  auto released_callback = Callback<SpatialInteractionSourceEventHandler>(
      this, &WMRInputManager::OnSourceReleased);
  hr = manager_->add_SourceReleased(released_callback.Get(), &released_token_);
  DCHECK(SUCCEEDED(hr));
}

void WMRInputManager::UnsubscribeEvents() {
  if (!manager_)
    return;

  HRESULT hr = S_OK;
  if (pressed_token_.value != 0) {
    hr = manager_->remove_SourcePressed(pressed_token_);
    pressed_token_.value = 0;
    DCHECK(SUCCEEDED(hr));
  }

  if (released_token_.value != 0) {
    hr = manager_->remove_SourceReleased(released_token_);
    released_token_.value = 0;
    DCHECK(SUCCEEDED(hr));
  }
}

}  // namespace device
