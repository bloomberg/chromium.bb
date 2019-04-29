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

#include "base/callback_list.h"
#include "base/macros.h"

namespace device {
class WMRInputSourceState;
class WMRInputSourceEventArgs {
 public:
  explicit WMRInputSourceEventArgs(
      Microsoft::WRL::ComPtr<
          ABI::Windows::UI::Input::Spatial::ISpatialInteractionSourceEventArgs>
          args);
  virtual ~WMRInputSourceEventArgs();

  ABI::Windows::UI::Input::Spatial::SpatialInteractionPressKind PressKind()
      const;
  WMRInputSourceState State() const;

 private:
  Microsoft::WRL::ComPtr<
      ABI::Windows::UI::Input::Spatial::ISpatialInteractionSourceEventArgs>
      args_;
  Microsoft::WRL::ComPtr<
      ABI::Windows::UI::Input::Spatial::ISpatialInteractionSourceEventArgs2>
      args2_;
};

class WMRInputManager {
 public:
  using InputEventCallbackList =
      base::CallbackList<void(const WMRInputSourceEventArgs&)>;
  using InputEventCallback =
      base::RepeatingCallback<void(const WMRInputSourceEventArgs&)>;

  static std::unique_ptr<WMRInputManager> GetForWindow(HWND hwnd);

  explicit WMRInputManager(
      Microsoft::WRL::ComPtr<
          ABI::Windows::UI::Input::Spatial::ISpatialInteractionManager>
          manager);
  virtual ~WMRInputManager();

  std::vector<WMRInputSourceState> GetDetectedSourcesAtTimestamp(
      Microsoft::WRL::ComPtr<ABI::Windows::Perception::IPerceptionTimestamp>
          timestamp) const;

  std::unique_ptr<InputEventCallbackList::Subscription> AddPressedCallback(
      const InputEventCallback& cb);

  std::unique_ptr<InputEventCallbackList::Subscription> AddReleasedCallback(
      const InputEventCallback& cb);

 private:
  void SubscribeEvents();
  void UnsubscribeEvents();

  HRESULT OnSourcePressed(
      ABI::Windows::UI::Input::Spatial::ISpatialInteractionManager* sender,
      ABI::Windows::UI::Input::Spatial::ISpatialInteractionSourceEventArgs*
          args);
  HRESULT OnSourceReleased(
      ABI::Windows::UI::Input::Spatial::ISpatialInteractionManager* sender,
      ABI::Windows::UI::Input::Spatial::ISpatialInteractionSourceEventArgs*
          args);

  Microsoft::WRL::ComPtr<
      ABI::Windows::UI::Input::Spatial::ISpatialInteractionManager>
      manager_;

  EventRegistrationToken pressed_token_;
  EventRegistrationToken released_token_;
  InputEventCallbackList pressed_callback_list_;
  InputEventCallbackList released_callback_list_;

  DISALLOW_COPY_AND_ASSIGN(WMRInputManager);
};
}  // namespace device

#endif  // DEVICE_VR_WINDOWS_MIXED_REALITY_WRAPPERS_WMR_INPUT_MANAGER_H_
