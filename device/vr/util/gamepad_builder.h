// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef DEVICE_VR_UTIL_GAMEPAD_BUILDER_H_
#define DEVICE_VR_UTIL_GAMEPAD_BUILDER_H_

#include <string>

#include "base/macros.h"
#include "device/gamepad/public/cpp/gamepads.h"
#include "device/vr/public/mojom/isolated_xr_service.mojom.h"

namespace device {

class GamepadBuilder {
 public:

  // Helper struct that we don't want to pollute the device namespace
  struct ButtonData {
    bool touched = false;
    bool pressed = false;
    double value = 0.0;

    bool has_both_axes = false;
    double x_axis = 0.0;
    double y_axis = 0.0;
  };

  GamepadBuilder(const std::string& gamepad_id,
                 GamepadMapping mapping,
                 device::mojom::XRHandedness handedness);
  virtual ~GamepadBuilder();

  virtual bool IsValid() const;
  base::Optional<Gamepad> GetGamepad() const;
  void SetAxisDeadzone(double value);

  void AddButton(const GamepadButton& button);
  void AddButton(const ButtonData& data);
  void AddAxis(double value);
  void AddPlaceholderButton();
  void RemovePlaceholderButton();

 protected:
  void AddAxes(const ButtonData& data);
  double ApplyAxisDeadzoneToValue(double value) const;

  GamepadHand GetHandedness() const { return gamepad_.hand; }
  GamepadMapping GetMapping() const { return gamepad_.mapping; }

 private:
  double axis_deadzone_ = 0.0;
  Gamepad gamepad_;

  DISALLOW_COPY_AND_ASSIGN(GamepadBuilder);
};

}  // namespace device
#endif  // DEVICE_VR_UTIL_GAMEPAD_BUILDER_H_
