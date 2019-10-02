// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/openxr/openxr_input_helper.h"

#include "device/vr/openxr/openxr_util.h"
#include "device/vr/util/xr_standard_gamepad_builder.h"

namespace device {

namespace {
base::Optional<GamepadBuilder::ButtonData> GetAxisButtonData(
    OpenXrAxisType openxr_button_type,
    base::Optional<GamepadButton> button_data,
    std::vector<double> axis) {
  GamepadBuilder::ButtonData data;
  if (!button_data || axis.size() != 2) {
    return base::nullopt;
  }

  switch (openxr_button_type) {
    case OpenXrAxisType::kThumbstick:
      data.type = GamepadBuilder::ButtonData::Type::kThumbstick;
      break;
    case OpenXrAxisType::kTrackpad:
      data.type = GamepadBuilder::ButtonData::Type::kTouchpad;
      break;
  }
  data.touched = button_data->touched;
  data.pressed = button_data->pressed;
  data.value = button_data->value;
  // Invert the y axis because -1 is up in the Gamepad API, but down in
  // OpenXR.
  data.x_axis = axis.at(0);
  data.y_axis = -axis.at(1);
  return data;
}
}  // namespace

XrResult OpenXRInputHelper::CreateOpenXRInputHelper(
    XrInstance instance,
    XrSession session,
    XrSpace local_space,
    std::unique_ptr<OpenXRInputHelper>* helper) {
  XrResult xr_result;

  std::unique_ptr<OpenXRInputHelper> new_helper =
      std::make_unique<OpenXRInputHelper>(session, local_space);

  // This map is used to store bindings for different kinds of interaction
  // profiles. This allows the runtime to choose a different input sources based
  // on availability.
  std::map<XrPath, std::vector<XrActionSuggestedBinding>> bindings;

  for (size_t i = 0; i < new_helper->controller_states_.size(); i++) {
    RETURN_IF_XR_FAILED(new_helper->controller_states_[i].controller.Initialize(
        static_cast<OpenXrHandednessType>(i), instance, session, &bindings));
    new_helper->controller_states_[i].primary_button_pressed = false;
  }

  for (auto it = bindings.begin(); it != bindings.end(); it++) {
    XrInteractionProfileSuggestedBinding profile_suggested_bindings = {
        XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING};
    profile_suggested_bindings.interactionProfile = it->first;
    profile_suggested_bindings.suggestedBindings = it->second.data();
    profile_suggested_bindings.countSuggestedBindings = it->second.size();

    RETURN_IF_XR_FAILED(xrSuggestInteractionProfileBindings(
        instance, &profile_suggested_bindings));
  }

  std::vector<XrActionSet> action_sets(new_helper->controller_states_.size());
  for (size_t i = 0; i < new_helper->controller_states_.size(); i++) {
    action_sets[i] =
        new_helper->controller_states_[i].controller.GetActionSet();
  }

  XrSessionActionSetsAttachInfo attach_info = {
      XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO};
  attach_info.countActionSets = action_sets.size();
  attach_info.actionSets = action_sets.data();
  RETURN_IF_XR_FAILED(xrAttachSessionActionSets(session, &attach_info));

  *helper = std::move(new_helper);
  return xr_result;
}

OpenXRInputHelper::OpenXRInputHelper(XrSession session, XrSpace local_space)
    : session_(session), local_space_(local_space) {}

OpenXRInputHelper::~OpenXRInputHelper() = default;

mojom::XRGamepadDataPtr OpenXRInputHelper::GetGamepadData(
    XrTime predicted_display_time) {
  if (XR_FAILED(SyncActions(predicted_display_time)))
    return nullptr;

  mojom::XRGamepadDataPtr gamepad_data_ptr = mojom::XRGamepadData::New();
  for (const OpenXrControllerState& controller_state : controller_states_) {
    const OpenXrController& controller = controller_state.controller;
    mojom::XRGamepadPtr gamepad_ptr = mojom::XRGamepad::New();
    gamepad_ptr->controller_id = controller.GetId();
    gamepad_ptr->timestamp = base::TimeTicks::Now();
    gamepad_ptr->hand = controller.GetHandness();

    std::vector<mojom::XRGamepadButtonPtr> buttons =
        controller.GetWebVrButtons();
    if (buttons.empty())
      continue;
    gamepad_ptr->buttons = std::move(buttons);

    std::vector<double> axes = controller.GetWebVrAxes();
    if (axes.empty())
      continue;
    gamepad_ptr->axes = std::move(axes);

    gamepad_ptr->pose =
        controller.GetPose(predicted_display_time, local_space_);
    if (!gamepad_ptr->pose)
      continue;
    gamepad_ptr->can_provide_position = gamepad_ptr->pose->position.has_value();
    gamepad_ptr->can_provide_orientation =
        gamepad_ptr->pose->orientation.has_value();

    gamepad_data_ptr->gamepads.push_back(std::move(gamepad_ptr));
  }

  return gamepad_data_ptr;
}

std::vector<mojom::XRInputSourceStatePtr> OpenXRInputHelper::GetInputState(
    XrTime predicted_display_time) {
  std::vector<mojom::XRInputSourceStatePtr> input_states;
  if (XR_FAILED(SyncActions(predicted_display_time))) {
    for (OpenXrControllerState& state : controller_states_) {
      state.primary_button_pressed = false;
    }
    return input_states;
  }

  for (uint32_t i = 0; i < controller_states_.size(); i++) {
    device::OpenXrController* controller = &controller_states_[i].controller;

    base::Optional<GamepadButton> trigger_button =
        controller->GetButton(OpenXrButtonType::kTrigger);

    // Having a trigger button is the minimum for an webxr input.
    // No trigger button indicates input is not connected.
    if (!trigger_button) {
      continue;
    }

    device::mojom::XRInputSourceStatePtr state =
        device::mojom::XRInputSourceState::New();

    // ID 0 will cause a DCHECK in the hash table used on the blink side.
    // To ensure that we don't have any collisions with other ids, increment
    // all of the ids by one.
    state->source_id = i + 1;

    state->description = controller->GetDescription(predicted_display_time);

    state->grip = controller->GetMojoFromGripTransform(predicted_display_time,
                                                       local_space_);
    state->emulated_position = false;
    state->primary_input_pressed = trigger_button.value().pressed;
    state->primary_input_clicked =
        controller_states_[i].primary_button_pressed &&
        !state->primary_input_pressed;
    controller_states_[i].primary_button_pressed = state->primary_input_pressed;
    state->gamepad = GetWebXRGamepad(*controller);
    input_states.push_back(std::move(state));
  }

  return input_states;
}

base::Optional<Gamepad> OpenXRInputHelper::GetWebXRGamepad(
    const OpenXrController& controller) const {
  XRStandardGamepadBuilder builder(controller.GetHandness());

  base::Optional<GamepadButton> trigger_button =
      controller.GetButton(OpenXrButtonType::kTrigger);
  if (!trigger_button)
    return base::nullopt;

  builder.SetPrimaryButton(trigger_button.value());

  base::Optional<GamepadButton> squeeze_button =
      controller.GetButton(OpenXrButtonType::kSqueeze);
  if (squeeze_button)
    builder.SetSecondaryButton(squeeze_button.value());

  base::Optional<GamepadButton> trackpad_button =
      controller.GetButton(OpenXrButtonType::kTrackpad);
  std::vector<double> trackpad_axis =
      controller.GetAxis(OpenXrAxisType::kTrackpad);
  base::Optional<GamepadBuilder::ButtonData> trackpad_button_data =
      GetAxisButtonData(OpenXrAxisType::kTrackpad, trackpad_button,
                        trackpad_axis);

  if (trackpad_button_data)
    builder.SetTouchpadData(trackpad_button_data.value());

  base::Optional<GamepadButton> thumbstick_button =
      controller.GetButton(OpenXrButtonType::kThumbstick);
  std::vector<double> thumbstick_axis =
      controller.GetAxis(OpenXrAxisType::kThumbstick);
  base::Optional<GamepadBuilder::ButtonData> thumbstick_button_data =
      GetAxisButtonData(OpenXrAxisType::kThumbstick, thumbstick_button,
                        thumbstick_axis);

  if (thumbstick_button_data)
    builder.SetThumbstickData(thumbstick_button_data.value());

  return builder.GetGamepad();
}

XrResult OpenXRInputHelper::SyncActions(XrTime predicted_display_time) {
  std::vector<XrActiveActionSet> active_action_sets(controller_states_.size());

  for (size_t i = 0; i < controller_states_.size(); i++) {
    active_action_sets[i].actionSet =
        controller_states_[i].controller.GetActionSet();
    active_action_sets[i].subactionPath = XR_NULL_PATH;
  }

  XrActionsSyncInfo sync_info = {XR_TYPE_ACTIONS_SYNC_INFO};
  sync_info.countActiveActionSets = active_action_sets.size();
  sync_info.activeActionSets = active_action_sets.data();
  return xrSyncActions(session_, &sync_info);
}

}  // namespace device
