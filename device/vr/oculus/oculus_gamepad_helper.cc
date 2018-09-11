// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/oculus/oculus_gamepad_helper.h"

#include <memory>

#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "device/gamepad/public/cpp/gamepads.h"
#include "device/vr/vr_device.h"
#include "third_party/libovr/src/Include/OVR_CAPI.h"
#include "ui/gfx/transform.h"
#include "ui/gfx/transform_util.h"

namespace device {

namespace {

enum GamepadIndex : unsigned int {
  kLeftTouchController = 0x0,
  kRightTouchController = 0x1,
  kRemote = 0x2,
};

float ApplyTriggerDeadzone(float value) {
  // Trigger value should be between 0 and 1.  We apply a deadzone for small
  // values so a loose controller still reports a value of 0 when not in use.
  float kTriggerDeadzone = 0.01f;

  return (value < kTriggerDeadzone) ? 0 : value;
}

mojom::XRGamepadButtonPtr GetGamepadButton(const ovrInputState& input_state,
                                           ovrButton button_id) {
  bool pressed = (input_state.Buttons & button_id) != 0;
  bool touched = (input_state.Touches & button_id) != 0;

  auto button = mojom::XRGamepadButton::New();
  button->pressed = pressed;
  button->touched = touched;
  button->value = pressed ? 1.0f : 0.0f;

  return button;
}

mojom::XRGamepadButtonPtr GetGamepadTouchTrigger(
    const ovrInputState& input_state,
    ovrTouch touch_id,
    float value) {
  bool touched = (input_state.Touches & touch_id) != 0;
  value = ApplyTriggerDeadzone(value);

  auto button = mojom::XRGamepadButton::New();
  button->pressed = value != 0;
  button->touched = touched;
  button->value = value;

  return button;
}

mojom::XRGamepadButtonPtr GetGamepadTrigger(float value) {
  value = ApplyTriggerDeadzone(value);
  auto button = mojom::XRGamepadButton::New();
  button->pressed = value != 0;
  button->touched = value != 0;
  button->value = value;

  return button;
}

mojom::XRGamepadButtonPtr GetGamepadTouch(const ovrInputState& input_state,
                                          ovrTouch touch_id) {
  bool touched = (input_state.Touches & touch_id) != 0;

  auto button = mojom::XRGamepadButton::New();
  button->pressed = false;
  button->touched = touched;
  button->value = 0.0f;

  return button;
}

void AddTouchData(mojom::XRGamepadDataPtr& data,
                  const ovrTrackingState& tracking,
                  const ovrInputState& input_state,
                  ovrHandType hand) {
  auto gamepad = mojom::XRGamepad::New();
  // This gamepad layout is the defacto standard, but can be adjusted for WebXR.
  switch (hand) {
    case ovrHand_Left:
      gamepad->hand = device::mojom::XRHandedness::LEFT;
      gamepad->controller_id = kLeftTouchController;
      gamepad->buttons.push_back(
          GetGamepadButton(input_state, ovrButton_LThumb));
      break;
    case ovrHand_Right:
      gamepad->hand = device::mojom::XRHandedness::RIGHT;
      gamepad->controller_id = kRightTouchController;
      gamepad->buttons.push_back(
          GetGamepadButton(input_state, ovrButton_RThumb));
      break;
    default:
      NOTREACHED();
      return;
  }

  gamepad->axes.push_back(input_state.Thumbstick[hand].x);
  gamepad->axes.push_back(-input_state.Thumbstick[hand].y);

  gamepad->buttons.push_back(GetGamepadTouchTrigger(
      input_state,
      hand == ovrHand_Left ? ovrTouch_LIndexTrigger : ovrTouch_RIndexTrigger,
      input_state.IndexTrigger[hand]));
  gamepad->buttons.push_back(GetGamepadTrigger(input_state.HandTrigger[hand]));

  switch (hand) {
    case ovrHand_Left:
      gamepad->buttons.push_back(GetGamepadButton(input_state, ovrButton_X));
      gamepad->buttons.push_back(GetGamepadButton(input_state, ovrButton_Y));
      gamepad->buttons.push_back(
          GetGamepadTouch(input_state, ovrTouch_LThumbRest));
      break;
    case ovrHand_Right:
      gamepad->buttons.push_back(GetGamepadButton(input_state, ovrButton_A));
      gamepad->buttons.push_back(GetGamepadButton(input_state, ovrButton_B));
      gamepad->buttons.push_back(
          GetGamepadTouch(input_state, ovrTouch_RThumbRest));
      break;
    default:
      NOTREACHED();
      break;
  }

  auto dst_pose = mojom::VRPose::New();
  const ovrPoseStatef& src_pose = tracking.HandPoses[hand];

  dst_pose->orientation = std::vector<float>(
      {src_pose.ThePose.Orientation.x, src_pose.ThePose.Orientation.y,
       src_pose.ThePose.Orientation.z, src_pose.ThePose.Orientation.w});

  dst_pose->position = std::vector<float>({src_pose.ThePose.Position.x,
                                           src_pose.ThePose.Position.y,
                                           src_pose.ThePose.Position.z});

  dst_pose->angularVelocity = std::vector<float>({src_pose.AngularVelocity.x,
                                                  src_pose.AngularVelocity.y,
                                                  src_pose.AngularVelocity.z});

  dst_pose->linearVelocity =
      std::vector<float>({src_pose.LinearVelocity.x, src_pose.LinearVelocity.y,
                          src_pose.LinearVelocity.z});

  dst_pose->angularAcceleration = std::vector<float>(
      {src_pose.AngularAcceleration.x, src_pose.AngularAcceleration.y,
       src_pose.AngularAcceleration.z});

  dst_pose->linearAcceleration = std::vector<float>(
      {src_pose.LinearAcceleration.x, src_pose.LinearAcceleration.y,
       src_pose.LinearAcceleration.z});

  gamepad->pose = std::move(dst_pose);
  gamepad->can_provide_position = true;
  gamepad->can_provide_orientation = true;
  gamepad->timestamp = base::TimeTicks::Now();
  data->gamepads.push_back(std::move(gamepad));
}

void AddRemoteData(mojom::XRGamepadDataPtr& data,
                   const ovrInputState& input_remote) {
  auto remote = mojom::XRGamepad::New();
  remote->buttons.push_back(GetGamepadButton(input_remote, ovrButton_Enter));
  remote->buttons.push_back(GetGamepadButton(input_remote, ovrButton_Back));
  remote->buttons.push_back(GetGamepadButton(input_remote, ovrButton_Up));
  remote->buttons.push_back(GetGamepadButton(input_remote, ovrButton_Down));
  remote->buttons.push_back(GetGamepadButton(input_remote, ovrButton_Left));
  remote->buttons.push_back(GetGamepadButton(input_remote, ovrButton_Right));
  remote->controller_id = kRemote;
  remote->hand = device::mojom::XRHandedness::NONE;
  data->gamepads.push_back(std::move(remote));
}

}  // namespace

mojom::XRGamepadDataPtr OculusGamepadHelper::GetGamepadData(
    ovrSession session) {
  ovrInputState input_touch;
  bool have_touch = OVR_SUCCESS(
      ovr_GetInputState(session, ovrControllerType_Touch, &input_touch));

  ovrInputState input_remote;
  bool have_remote = OVR_SUCCESS(
      ovr_GetInputState(session, ovrControllerType_Remote, &input_remote));

  ovrTrackingState tracking = ovr_GetTrackingState(session, 0, false);

  mojom::XRGamepadDataPtr data = mojom::XRGamepadData::New();

  if (have_touch) {
    AddTouchData(data, tracking, input_touch, ovrHand_Left);
    AddTouchData(data, tracking, input_touch, ovrHand_Right);
  }

  if (have_remote)
    AddRemoteData(data, input_remote);

  return data;
}

}  // namespace device
