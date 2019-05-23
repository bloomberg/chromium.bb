// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "device/vr/util/gamepad_builder.h"

#include "base/logging.h"
#include "base/stl_util.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "device/vr/util/copy_to_ustring.h"

namespace device {

namespace {
GamepadHand MojoToGamepadHandedness(device::mojom::XRHandedness handedness) {
  switch (handedness) {
    case device::mojom::XRHandedness::LEFT:
      return GamepadHand::kLeft;
    case device::mojom::XRHandedness::RIGHT:
      return GamepadHand::kRight;
    case device::mojom::XRHandedness::NONE:
      return GamepadHand::kNone;
  }

  NOTREACHED();
}

// TODO(crbug.com/955809): Once Gamepad uses an enum this method can be removed.
std::string GamepadMappingToString(GamepadBuilder::GamepadMapping mapping) {
  switch (mapping) {
    case GamepadBuilder::GamepadMapping::kNone:
      return "";
      break;
    case GamepadBuilder::GamepadMapping::kStandard:
      return "standard";
      break;
    case GamepadBuilder::GamepadMapping::kXRStandard:
      return "xr-standard";
      break;
  }

  NOTREACHED();
}

}  // anonymous namespace

GamepadBuilder::GamepadBuilder(const std::string& gamepad_id,
                               GamepadMapping mapping,
                               device::mojom::XRHandedness handedness)
    : mapping_(mapping) {
  DCHECK_LT(gamepad_id.size(), Gamepad::kIdLengthCap);

  auto mapping_str = GamepadMappingToString(mapping);
  DCHECK_LT(mapping_str.size(), Gamepad::kMappingLengthCap);

  gamepad_.connected = true;
  gamepad_.timestamp = base::TimeTicks::Now().since_origin().InMicroseconds();
  gamepad_.hand = MojoToGamepadHandedness(handedness);
  CopyToUString(base::UTF8ToUTF16(gamepad_id), gamepad_.id,
                Gamepad::kIdLengthCap);
  CopyToUString(base::UTF8ToUTF16(mapping_str), gamepad_.mapping,
                Gamepad::kMappingLengthCap);
}

GamepadBuilder::~GamepadBuilder() = default;

bool GamepadBuilder::IsValid() const {
  switch (mapping_) {
    case GamepadMapping::kXRStandard:
      // In order to satisfy the XRStandard mapping at least two buttons and one
      // set of axes need to have been added.
      return gamepad_.axes_length >= 2 && gamepad_.buttons_length >= 2;
    case GamepadMapping::kStandard:
    case GamepadMapping::kNone:
      // Neither standard requires any buttons to be set, and all other data
      // is set in the constructor.
      return true;
  }

  NOTREACHED();
}

base::Optional<Gamepad> GamepadBuilder::GetGamepad() const {
  if (IsValid())
    return gamepad_;

  return base::nullopt;
}

void GamepadBuilder::SetAxisDeadzone(double deadzone) {
  DCHECK_GE(deadzone, 0);
  axis_deadzone_ = deadzone;
}

void GamepadBuilder::AddButton(const GamepadButton& button) {
  DCHECK_LT(gamepad_.buttons_length, Gamepad::kButtonsLengthCap);
  gamepad_.buttons[gamepad_.buttons_length++] = button;
}

void GamepadBuilder::AddButton(const ButtonData& data) {
  AddButton(GamepadButton(data.pressed, data.touched, data.value));
  if (data.has_both_axes)
    AddAxes(data);
}

void GamepadBuilder::AddAxis(double value) {
  DCHECK_LT(gamepad_.axes_length, Gamepad::kAxesLengthCap);
  gamepad_.axes[gamepad_.axes_length++] = ApplyAxisDeadzoneToValue(value);
}

void GamepadBuilder::AddAxes(const ButtonData& data) {
  DCHECK(data.has_both_axes);
  AddAxis(data.x_axis);
  AddAxis(data.y_axis);
}

void GamepadBuilder::AddPlaceholderButton() {
  AddButton(GamepadButton());
}

void GamepadBuilder::RemovePlaceholderButton() {
  // Since this is a member array, it actually is full of default constructed
  // buttons, so all we have to do to remove a button is decrement the length
  // variable.  However, we should check before we do so that we actually have
  // a length and that there's not any data that's been set in the alleged
  // placeholder button.
  DCHECK_GT(gamepad_.buttons_length, 0u);
  GamepadButton button = gamepad_.buttons[gamepad_.buttons_length - 1];
  DCHECK(!button.pressed && !button.touched && button.value == 0);
  gamepad_.buttons_length--;
}

double GamepadBuilder::ApplyAxisDeadzoneToValue(double value) const {
  return std::fabs(value) < axis_deadzone_ ? 0 : value;
}

}  // namespace device
