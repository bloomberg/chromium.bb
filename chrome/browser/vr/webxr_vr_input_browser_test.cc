// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/vr/test/mock_xr_device_hook_base.h"
#include "chrome/browser/vr/test/webvr_browser_test.h"
#include "chrome/browser/vr/test/webxr_vr_browser_test.h"
#include "device/vr/public/mojom/browser_test_interfaces.mojom.h"
#include "third_party/openvr/src/headers/openvr.h"

// Browser test equivalent of
// chrome/android/javatests/src/.../browser/vr/WebXrVrInputTest.java.
// End-to-end tests for user input interaction with WebXR/WebVR.

namespace vr {

// Test that focus is locked to the presenting display for the purposes of VR/XR
// input.
void TestPresentationLocksFocusImpl(WebXrVrBrowserTestBase* t,
                                    std::string filename) {
  t->LoadUrlAndAwaitInitialization(t->GetFileUrlForHtmlTestFile(filename));
  t->EnterSessionWithUserGestureOrFail();
  t->ExecuteStepAndWait("stepSetupFocusLoss()");
  t->EndTest();
}

IN_PROC_BROWSER_TEST_F(WebVrBrowserTestStandard, TestPresentationLocksFocus) {
  TestPresentationLocksFocusImpl(this, "test_presentation_locks_focus");
}
IN_PROC_BROWSER_TEST_F(WebXrVrBrowserTestStandard, TestPresentationLocksFocus) {
  TestPresentationLocksFocusImpl(this, "webxr_test_presentation_locks_focus");
}

class WebXrControllerInputMock : public MockXRDeviceHookBase {
 public:
  void OnFrameSubmitted(
      device_test::mojom::SubmittedFrameDataPtr frame_data,
      device_test::mojom::XRTestHook::OnFrameSubmittedCallback callback) final;

  void WaitNumFrames(unsigned int num_frames) {
    DCHECK(!wait_loop_);
    target_submitted_frames_ = num_submitted_frames_ + num_frames;
    wait_loop_ = new base::RunLoop(base::RunLoop::Type::kNestableTasksAllowed);
    wait_loop_->Run();
    delete wait_loop_;
    wait_loop_ = nullptr;
  }

  // TODO(https://crbug.com/887726): Figure out why waiting for OpenVR to grab
  // the updated state instead of waiting for a number of frames causes frames
  // to be submitted at an extremely slow rate. Once fixed, switch away from
  // waiting on number of frames.
  void UpdateControllerAndWait(
      unsigned int index,
      const device::ControllerFrameData& controller_data) {
    UpdateController(index, controller_data);
    WaitNumFrames(30);
  }

  void ToggleButtonTouches(unsigned int index, uint64_t button_mask) {
    auto controller_data = GetCurrentControllerData(index);

    controller_data.packet_number++;
    controller_data.buttons_touched ^= button_mask;

    UpdateControllerAndWait(index, controller_data);
  }

  void ToggleButtons(unsigned int index, uint64_t button_mask) {
    auto controller_data = GetCurrentControllerData(index);

    controller_data.packet_number++;
    controller_data.buttons_pressed ^= button_mask;
    controller_data.buttons_touched ^= button_mask;
    UpdateControllerAndWait(index, controller_data);
  }

  void ToggleTriggerButton(unsigned int index, vr::EVRButtonId button_id) {
    auto controller_data = GetCurrentControllerData(index);
    uint64_t button_mask = vr::ButtonMaskFromId(button_id);

    controller_data.packet_number++;
    controller_data.buttons_pressed ^= button_mask;
    controller_data.buttons_touched ^= button_mask;

    bool is_pressed = ((controller_data.buttons_pressed & button_mask) != 0);

    unsigned int axis_offset = GetAxisOffset(button_id);
    DCHECK(controller_data.axis_data[axis_offset].axis_type ==
           vr::k_eControllerAxis_Trigger);
    controller_data.axis_data[axis_offset].x = is_pressed ? 1.0 : 0.0;
    UpdateControllerAndWait(index, controller_data);
  }

  void SetAxes(unsigned int index,
               vr::EVRButtonId button_id,
               float x,
               float y) {
    auto controller_data = GetCurrentControllerData(index);
    unsigned int axis_offset = GetAxisOffset(button_id);
    DCHECK(controller_data.axis_data[axis_offset].axis_type != 0);

    controller_data.packet_number++;
    controller_data.axis_data[axis_offset].x = x;
    controller_data.axis_data[axis_offset].y = y;
    UpdateControllerAndWait(index, controller_data);
  }

  void TogglePrimaryTrigger(unsigned int index) {
    ToggleTriggerButton(index, vr::k_EButton_SteamVR_Trigger);
  }

  void PressReleasePrimaryTrigger(unsigned int index) {
    TogglePrimaryTrigger(index);
    TogglePrimaryTrigger(index);
  }

  unsigned int CreateAndConnectMinimalGamepad() {
    // Create a controller that only supports select and one TrackPad, i.e. it
    // has just enough data to be considered a gamepad.
    uint64_t supported_buttons =
        vr::ButtonMaskFromId(vr::k_EButton_SteamVR_Trigger) |
        vr::ButtonMaskFromId(vr::k_EButton_Axis0);

    std::map<vr::EVRButtonId, unsigned int> axis_types = {
        {vr::k_EButton_Axis0, vr::k_eControllerAxis_TrackPad},
        {vr::k_EButton_SteamVR_Trigger, vr::k_eControllerAxis_Trigger},
    };

    return CreateAndConnectController(
        device::ControllerRole::kControllerRoleRight, axis_types,
        supported_buttons);
  }

  unsigned int CreateAndConnectController(
      device::ControllerRole role,
      std::map<vr::EVRButtonId, unsigned int> axis_types = {},
      uint64_t supported_buttons = UINT64_MAX) {
    auto controller = CreateValidController(role);
    controller.supported_buttons = supported_buttons;
    for (const auto& axis_type : axis_types) {
      unsigned int axis_offset = GetAxisOffset(axis_type.first);
      controller.axis_data[axis_offset].axis_type = axis_type.second;
    }

    return ConnectController(controller);
  }

  void UpdateControllerSupport(
      unsigned int controller_index,
      const std::map<vr::EVRButtonId, unsigned int>& axis_types,
      uint64_t supported_buttons) {
    auto controller_data = GetCurrentControllerData(controller_index);

    for (unsigned int i = 0; i < device::kMaxNumAxes; i++) {
      auto button_id = GetAxisId(i);
      auto it = axis_types.find(button_id);
      unsigned int new_axis_type = k_eControllerAxis_None;
      if (it != axis_types.end())
        new_axis_type = it->second;
      controller_data.axis_data[i].axis_type = new_axis_type;
    }

    controller_data.supported_buttons = supported_buttons;

    UpdateControllerAndWait(controller_index, controller_data);
  }

 private:
  vr::EVRButtonId GetAxisId(unsigned int offset) {
    return static_cast<vr::EVRButtonId>(vr::k_EButton_Axis0 + offset);
  }
  unsigned int GetAxisOffset(vr::EVRButtonId button_id) {
    DCHECK(vr::k_EButton_Axis0 <= button_id &&
           button_id < (vr::k_EButton_Axis0 + device::kMaxNumAxes));
    return static_cast<unsigned int>(button_id) -
           static_cast<unsigned int>(vr::k_EButton_Axis0);
  }

  device::ControllerFrameData GetCurrentControllerData(unsigned int index) {
    auto iter = controller_data_map_.find(index);
    DCHECK(iter != controller_data_map_.end());
    return iter->second;
  }

  base::RunLoop* wait_loop_ = nullptr;
  unsigned int num_submitted_frames_ = 0;
  unsigned int target_submitted_frames_ = 0;
};

void WebXrControllerInputMock::OnFrameSubmitted(
    device_test::mojom::SubmittedFrameDataPtr frame_data,
    device_test::mojom::XRTestHook::OnFrameSubmittedCallback callback) {
  num_submitted_frames_++;
  if (wait_loop_ && target_submitted_frames_ == num_submitted_frames_) {
    wait_loop_->Quit();
  }
  std::move(callback).Run();
}

// Ensure that changes to a gamepad object respect that it is the same object
// and that if whether or not an input source has a gamepad changes that the
// input source change event is fired and a new input source is created.
IN_PROC_BROWSER_TEST_F(WebXrVrBrowserTestStandard, TestInputGamepadSameObject) {
  WebXrControllerInputMock my_mock;

  // Create a set of buttons and axes that don't have enough data to be made
  // into an xr-standard gamepad (which we expect the runtimes to not report).
  // Note that we need to set the trigger axis because of how OpenVR handles
  // selects.
  uint64_t insufficient_buttons =
      vr::ButtonMaskFromId(vr::k_EButton_SteamVR_Trigger);
  std::map<vr::EVRButtonId, unsigned int> insufficient_axis_types = {
      {vr::k_EButton_SteamVR_Trigger, vr::k_eControllerAxis_Trigger},
  };

  // Create a set of buttons and axes that we expect to have enough data to be
  // made into an xr-standard gamepad (which we expect the runtimes to report).
  uint64_t sufficient_buttons =
      vr::ButtonMaskFromId(vr::k_EButton_SteamVR_Trigger) |
      vr::ButtonMaskFromId(vr::k_EButton_Axis0);
  std::map<vr::EVRButtonId, unsigned int> sufficient_axis_types = {
      {vr::k_EButton_Axis0, vr::k_eControllerAxis_TrackPad},
      {vr::k_EButton_SteamVR_Trigger, vr::k_eControllerAxis_Trigger},
  };

  // Start off without a gamepad.
  unsigned int controller_index = my_mock.CreateAndConnectController(
      device::ControllerRole::kControllerRoleRight, insufficient_axis_types,
      insufficient_buttons);

  LoadUrlAndAwaitInitialization(
      GetFileUrlForHtmlTestFile("test_webxr_input_same_object"));
  EnterSessionWithUserGestureOrFail();

  RunJavaScriptOrFail("setupListeners()");

  // We should only have seen the first change indicating we have input sources.
  PollJavaScriptBooleanOrFail("inputChangeEvents === 1", kPollTimeoutShort);

  // We only expect one input source, cache it.
  RunJavaScriptOrFail("validateInputSourceLength(1)");
  RunJavaScriptOrFail("updateCachedInputSource(0)");

  // Toggle a button and confirm that the controller is still the same.
  my_mock.PressReleasePrimaryTrigger(controller_index);
  RunJavaScriptOrFail("validateCachedSourcePresence(true)");
  RunJavaScriptOrFail("validateCurrentAndCachedGamepadMatch()");

  // Update the controller to now support a gamepad and verify that we get a
  // change event and that the old controller isn't present.  Then cache the new
  // one.
  my_mock.UpdateControllerSupport(controller_index, sufficient_axis_types,
                                  sufficient_buttons);
  PollJavaScriptBooleanOrFail("inputChangeEvents === 2", kPollTimeoutShort);
  RunJavaScriptOrFail("validateCachedSourcePresence(false)");
  RunJavaScriptOrFail("validateInputSourceLength(1)");
  RunJavaScriptOrFail("updateCachedInputSource(0)");

  // Toggle a button and confirm that the controller is still the same.
  my_mock.PressReleasePrimaryTrigger(controller_index);
  RunJavaScriptOrFail("validateCachedSourcePresence(true)");
  RunJavaScriptOrFail("validateCurrentAndCachedGamepadMatch()");

  // Switch back to the insufficient gamepad and confirm that we get the change.
  my_mock.UpdateControllerSupport(controller_index, insufficient_axis_types,
                                  insufficient_buttons);
  PollJavaScriptBooleanOrFail("inputChangeEvents === 3", kPollTimeoutShort);
  RunJavaScriptOrFail("validateCachedSourcePresence(false)");
  RunJavaScriptOrFail("validateInputSourceLength(1)");
  RunJavaScriptOrFail("done()");
  EndTest();
}

// Ensure that if the controller lacks enough data to be considered a Gamepad
// that the input source that it is associated with does not have a Gamepad.
IN_PROC_BROWSER_TEST_F(WebXrVrBrowserTestStandard, TestGamepadIncompleteData) {
  WebXrControllerInputMock my_mock;

  // Create a controller that only supports select, i.e. it lacks enough data
  // to be considered a gamepad.
  uint64_t supported_buttons =
      vr::ButtonMaskFromId(vr::k_EButton_SteamVR_Trigger);
  my_mock.CreateAndConnectController(
      device::ControllerRole::kControllerRoleRight, {}, supported_buttons);

  LoadUrlAndAwaitInitialization(
      GetFileUrlForHtmlTestFile("test_webxr_gamepad_support"));
  EnterSessionWithUserGestureOrFail();
  ExecuteStepAndWait("validateInputSourceHasNoGamepad()");
  RunJavaScriptOrFail("done()");
  EndTest();
}

// Ensure that if a Gamepad has the minimum required number of axes/buttons to
// be considered an xr-standard Gamepad, that it is exposed as such, and that
// we can check the state of it's priamry axes/button.
IN_PROC_BROWSER_TEST_F(WebXrVrBrowserTestStandard, TestGamepadMinimumData) {
  WebXrControllerInputMock my_mock;
  unsigned int controller_index = my_mock.CreateAndConnectMinimalGamepad();

  LoadUrlAndAwaitInitialization(
      GetFileUrlForHtmlTestFile("test_webxr_gamepad_support"));
  EnterSessionWithUserGestureOrFail();

  // Press the trigger and set the axis to a non-zero amount, so we can ensure
  // we aren't getting just default gamepad data.
  my_mock.TogglePrimaryTrigger(controller_index);
  my_mock.SetAxes(controller_index, vr::k_EButton_Axis0, 0.5, -0.5);

  // The trigger should be button 0, and the first set of axes should have it's
  // value set.
  ExecuteStepAndWait("validateMapping('xr-standard')");
  ExecuteStepAndWait("validateButtonPressed(0)");
  ExecuteStepAndWait("validateAxesValues(0, 0.5, -0.5)");
  RunJavaScriptOrFail("done()");
  EndTest();
}

// Ensure that if a Gamepad has all of the required and optional buttons as
// specified by the xr-standard mapping, that those buttons are plumbed up
// in their required places.
IN_PROC_BROWSER_TEST_F(WebXrVrBrowserTestStandard, TestGamepadCompleteData) {
  WebXrControllerInputMock my_mock;

  // Create a controller that supports all reserved buttons.  Note that per
  // third_party/openvr/src/headers/openvr.h, SteamVR_Trigger is Axis1.
  uint64_t supported_buttons =
      vr::ButtonMaskFromId(vr::k_EButton_SteamVR_Trigger) |
      vr::ButtonMaskFromId(vr::k_EButton_Axis0) |
      vr::ButtonMaskFromId(vr::k_EButton_Axis2) |
      vr::ButtonMaskFromId(vr::k_EButton_Grip);

  std::map<vr::EVRButtonId, unsigned int> axis_types = {
      {vr::k_EButton_Axis0, vr::k_eControllerAxis_Joystick},
      {vr::k_EButton_SteamVR_Trigger, vr::k_eControllerAxis_Trigger},
      {vr::k_EButton_Axis2, vr::k_eControllerAxis_TrackPad},
  };

  unsigned int controller_index = my_mock.CreateAndConnectController(
      device::ControllerRole::kControllerRoleRight, axis_types,
      supported_buttons);

  LoadUrlAndAwaitInitialization(
      GetFileUrlForHtmlTestFile("test_webxr_gamepad_support"));
  EnterSessionWithUserGestureOrFail();

  // Setup some state on the optional buttons (as TestGamepadMinimumData should
  // ensure proper state on the required buttons).
  // Set a value on the secondary set of axes.
  my_mock.SetAxes(controller_index, vr::k_EButton_Axis2, 0.25, -0.25);

  // Set the secondary trackpad/joystick to be touched.
  my_mock.ToggleButtonTouches(controller_index,
                              vr::ButtonMaskFromId(vr::k_EButton_Axis2));

  // Set the grip button to be pressed.
  my_mock.ToggleButtons(controller_index,
                        vr::ButtonMaskFromId(vr::k_EButton_Grip));

  // Controller should meet the requirements for the 'xr-standard' mapping.
  ExecuteStepAndWait("validateMapping('xr-standard')");

  // The secondary set of axes should be set appropriately.
  ExecuteStepAndWait("validateAxesValues(1, 0.25, -0.25)");

  // Button 2 is reserved for the Grip, and should be pressed.
  ExecuteStepAndWait("validateButtonPressed(2)");

  // Button 3 is reserved for the secondary trackpad/joystick and should be
  // touched but not pressed.
  ExecuteStepAndWait("validateButtonNotPressed(3)");
  ExecuteStepAndWait("validateButtonTouched(3)");

  RunJavaScriptOrFail("done()");
  EndTest();
}

// Ensure that if a Gamepad has all required buttons, an extra button not
// mapped in the xr-standard specification, and is missing reserved buttons
// from the XR Standard specification, that the extra button does not appear
// in either of the reserved button slots.
IN_PROC_BROWSER_TEST_F(WebXrVrBrowserTestStandard, TestGamepadReservedData) {
  WebXrControllerInputMock my_mock;

  // Create a controller that is missing reserved buttons, but supports an
  // extra button to guarantee that the reserved button is held.
  uint64_t supported_buttons =
      vr::ButtonMaskFromId(vr::k_EButton_SteamVR_Trigger) |
      vr::ButtonMaskFromId(vr::k_EButton_Axis0) |
      vr::ButtonMaskFromId(vr::k_EButton_A);

  std::map<vr::EVRButtonId, unsigned int> axis_types = {
      {vr::k_EButton_Axis0, vr::k_eControllerAxis_Joystick},
      {vr::k_EButton_SteamVR_Trigger, vr::k_eControllerAxis_Trigger},
  };

  unsigned int controller_index = my_mock.CreateAndConnectController(
      device::ControllerRole::kControllerRoleRight, axis_types,
      supported_buttons);

  LoadUrlAndAwaitInitialization(
      GetFileUrlForHtmlTestFile("test_webxr_gamepad_support"));
  EnterSessionWithUserGestureOrFail();

  // Claim that all buttons are pressed, note that any non-supported buttons
  // should be ignored.
  my_mock.ToggleButtons(controller_index, UINT64_MAX);

  // Index 2 and 3 are reserved for the grip and secondary joystick.
  // As our controller doesn't support them, they should be present but not
  // pressed, and our "extra" button should be index 4 and should be pressed.
  ExecuteStepAndWait("validateMapping('xr-standard')");
  ExecuteStepAndWait("validateButtonPressed(0)");
  ExecuteStepAndWait("validateButtonPressed(1)");
  ExecuteStepAndWait("validateButtonNotPressed(2)");
  ExecuteStepAndWait("validateButtonNotPressed(3)");
  ExecuteStepAndWait("validateButtonPressed(4)");

  RunJavaScriptOrFail("done()");
  EndTest();
}

// Test that OpenVR controller input is registered via WebXR's input method.
// Equivalent to
// WebXrVrInputTest#testControllerClicksRegisteredOnDaydream_WebXr.
IN_PROC_BROWSER_TEST_F(WebXrVrBrowserTestStandard,
                       TestControllerInputRegistered) {
  WebXrControllerInputMock my_mock;

  unsigned int controller_index = my_mock.CreateAndConnectMinimalGamepad();

  // Load the test page and enter presentation.
  LoadUrlAndAwaitInitialization(GetFileUrlForHtmlTestFile("test_webxr_input"));
  EnterSessionWithUserGestureOrFail();

  unsigned int num_iterations = 10;
  RunJavaScriptOrFail("stepSetupListeners(" +
                      base::NumberToString(num_iterations) + ")");

  // Press and unpress the controller's trigger a bunch of times and make sure
  // they're all registered.
  for (unsigned int i = 0; i < num_iterations; ++i) {
    my_mock.PressReleasePrimaryTrigger(controller_index);
    // After each trigger release, wait for the JavaScript to receive the
    // "select" event.
    WaitOnJavaScriptStep();
  }
  EndTest();
}

// Test that OpenVR controller input is registered via the Gamepad API.
// Equivalent to
// WebXrVrInputTest#testControllerClicksRegisteredOnDaydream
IN_PROC_BROWSER_TEST_F(WebVrBrowserTestStandard,
                       TestControllerInputRegistered) {
  WebXrControllerInputMock my_mock;

  // Connect a controller.
  auto controller_data = my_mock.CreateValidController(
      device::ControllerRole::kControllerRoleRight);
  // openvr_gamepad_helper assumes axis index 1 is the trigger, so we need to
  // set that here, otherwise it won't check whether it's pressed or not.
  // Note that we aren't able to use the CreateAndConnectMinimalGamepad helper
  // here as that adds support for axis_data[0], which causes OpenVR on WebVR
  // to treat that button as the primary input (per the defacto webvr standard),
  // and we want it to only see the trigger.
  controller_data.axis_data[1].axis_type = vr::k_eControllerAxis_Trigger;
  unsigned int controller_index = my_mock.ConnectController(controller_data);

  // Load the test page and enter presentation.
  LoadUrlAndAwaitInitialization(
      GetFileUrlForHtmlTestFile("test_gamepad_button"));
  EnterSessionWithUserGestureOrFail();

  // We need to have this, otherwise the JavaScript side of the Gamepad API
  // doesn't seem to pick up the correct button state? I.e. if we don't have
  // this, openvr_gamepad_helper properly sets the gamepad's button state,
  // but JavaScript still shows no buttons pressed.
  // TODO(bsheedy): Figure out why this is the case.
  my_mock.PressReleasePrimaryTrigger(controller_index);

  // Setting this in the Android version of the test needs to happen after a
  // flakiness workaround. Coincidentally, it's also helpful for the different
  // issue solved by the above PressReleasePrimaryTrigger, so make sure to set
  // it here so that the above press/release isn't caught by the test code.
  RunJavaScriptOrFail("canStartTest = true");
  // Press and release the trigger, ensuring the Gamepad API detects both.
  my_mock.TogglePrimaryTrigger(controller_index);
  WaitOnJavaScriptStep();
  my_mock.TogglePrimaryTrigger(controller_index);
  WaitOnJavaScriptStep();
  EndTest();
}

class WebXrHeadPoseMock : public MockXRDeviceHookBase {
 public:
  void WaitGetPresentingPose(
      device_test::mojom::XRTestHook::WaitGetPresentingPoseCallback callback)
      final {
    auto pose = device_test::mojom::PoseFrameData::New();
    pose->device_to_origin = pose_;
    std::move(callback).Run(std::move(pose));
  }

  void SetHeadPose(const gfx::Transform& pose) { pose_ = pose; }

 private:
  gfx::Transform pose_;
};

std::string TransformToColMajorString(gfx::Transform& t) {
  float array[16];
  t.matrix().asColMajorf(array);
  std::string array_string = "[";
  for (int i = 0; i < 16; i++) {
    array_string += base::NumberToString(array[i]) + ",";
  }
  array_string.pop_back();
  array_string.push_back(']');
  return array_string;
}

void TestHeadPosesUpdateImpl(WebXrVrBrowserTestBase* t) {
  WebXrHeadPoseMock my_mock;

  t->LoadUrlAndAwaitInitialization(
      t->GetFileUrlForHtmlTestFile("webxr_test_head_poses"));
  t->EnterSessionWithUserGestureOrFail();

  auto pose = gfx::Transform();
  my_mock.SetHeadPose(pose);
  t->RunJavaScriptOrFail("stepWaitForMatchingPose(" +
                         TransformToColMajorString(pose) + ")");
  t->WaitOnJavaScriptStep();

  // No significance to this new transform other than that it's easy to tell
  // whether the correct pose got piped through to WebXR or not.
  pose.RotateAboutXAxis(90);
  pose.Translate3d(2, 3, 4);
  my_mock.SetHeadPose(pose);
  t->RunJavaScriptOrFail("stepWaitForMatchingPose(" +
                         TransformToColMajorString(pose) + ")");
  t->WaitOnJavaScriptStep();
  t->AssertNoJavaScriptErrors();
}

// Test that head pose changes in OpenVR are properly reflected in the viewer
// pose provided by WebXR.
IN_PROC_BROWSER_TEST_F(WebXrVrBrowserTestStandard, TestHeadPosesUpdate) {
  TestHeadPosesUpdateImpl(this);
}

// Tests that head pose changes in WMR are properly reflected in the viewer pose
// provided by WebXR.
IN_PROC_BROWSER_TEST_F(WebXrVrBrowserTestWMR, TestHeadPosesUpdate) {
  TestHeadPosesUpdateImpl(this);
}

}  // namespace vr
