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

#include "base/synchronization/lock.h"
#include "device/gamepad/public/cpp/gamepads.h"
#include "device/vr/public/mojom/isolated_xr_service.mojom.h"
#include "device/vr/public/mojom/vr_service.mojom.h"

namespace device {
struct ButtonData {
  bool pressed;
  bool touched;
  double value;

  double x_axis;
  double y_axis;
};

enum class ButtonName {
  kSelect,
  kGrip,
  kTouchpad,
  kThumbstick,
};

struct ParsedInputState {
  mojom::XRInputSourceStatePtr source_state;
  std::unordered_map<ButtonName, ButtonData> button_data;
  GamepadPose gamepad_pose;
  ParsedInputState();
  ~ParsedInputState();
  ParsedInputState(ParsedInputState&& other);
};

class MixedRealityInputHelper {
 public:
  MixedRealityInputHelper(HWND hwnd);
  virtual ~MixedRealityInputHelper();
  std::vector<mojom::XRInputSourceStatePtr> GetInputState(
      Microsoft::WRL::ComPtr<
          ABI::Windows::Perception::Spatial::ISpatialCoordinateSystem> origin,
      Microsoft::WRL::ComPtr<ABI::Windows::Perception::IPerceptionTimestamp>
          timestamp);

  mojom::XRGamepadDataPtr GetWebVRGamepadData(
      Microsoft::WRL::ComPtr<
          ABI::Windows::Perception::Spatial::ISpatialCoordinateSystem> origin,
      Microsoft::WRL::ComPtr<ABI::Windows::Perception::IPerceptionTimestamp>
          timestamp);

  void Dispose();

 private:
  bool EnsureSpatialInteractionManager();

  ParsedInputState LockedParseWindowsSourceState(
      Microsoft::WRL::ComPtr<
          ABI::Windows::UI::Input::Spatial::ISpatialInteractionSourceState>
          state,
      Microsoft::WRL::ComPtr<
          ABI::Windows::Perception::Spatial::ISpatialCoordinateSystem> origin);

  HRESULT OnSourcePressed(
      ABI::Windows::UI::Input::Spatial::ISpatialInteractionManager* sender,
      ABI::Windows::UI::Input::Spatial::ISpatialInteractionSourceEventArgs*
          args);
  HRESULT OnSourceReleased(
      ABI::Windows::UI::Input::Spatial::ISpatialInteractionManager* sender,
      ABI::Windows::UI::Input::Spatial::ISpatialInteractionSourceEventArgs*
          args);
  HRESULT ProcessSourceEvent(
      ABI::Windows::UI::Input::Spatial::ISpatialInteractionSourceEventArgs*
          raw_args,
      bool is_pressed);

  void SubscribeEvents();
  void UnsubscribeEvents();

  Microsoft::WRL::ComPtr<
      ABI::Windows::UI::Input::Spatial::ISpatialInteractionManager>
      spatial_interaction_manager_;
  EventRegistrationToken pressed_token_;
  EventRegistrationToken released_token_;

  struct ControllerState {
    bool pressed;
    bool clicked;
  };
  std::unordered_map<uint32_t, ControllerState> controller_states_;
  HWND hwnd_;

  std::vector<Microsoft::WRL::ComPtr<
      ABI::Windows::UI::Input::Spatial::ISpatialInteractionSourceState>>
      pending_voice_states_;

  base::Lock lock_;

  DISALLOW_COPY_AND_ASSIGN(MixedRealityInputHelper);
};
}  // namespace device

#endif  // DEVICE_VR_WINDOWS_MIXED_REALITY_MIXED_REALITY_INPUT_HELPER_H_
