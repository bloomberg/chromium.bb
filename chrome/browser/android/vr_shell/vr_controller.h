// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_VR_SHELL_VR_CONTROLLER_H_
#define CHROME_BROWSER_ANDROID_VR_SHELL_VR_CONTROLLER_H_

#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/time/time.h"
#include "chrome/browser/android/vr_shell/vr_controller_model.h"
#include "device/vr/android/gvr/gvr_gamepad_data_provider.h"
#include "third_party/gvr-android-sdk/src/libraries/headers/vr/gvr/capi/include/gvr_types.h"
#include "ui/gfx/geometry/point3_f.h"
#include "ui/gfx/geometry/quaternion.h"
#include "ui/gfx/geometry/vector2d_f.h"
#include "ui/gfx/geometry/vector3d_f.h"

namespace blink {
class WebGestureEvent;
}

namespace gfx {
class Transform;
}

namespace gvr {
class ControllerState;
}

namespace vr_shell {

class ElbowModel;

// Angle (radians) the beam down from the controller axis, for wrist comfort.
constexpr float kErgoAngleOffset = 0.26f;

using GestureList = std::vector<std::unique_ptr<blink::WebGestureEvent>>;

class VrController {
 public:
  // Controller API entry point.
  explicit VrController(gvr_context* gvr_context);
  ~VrController();

  // Must be called when the Activity gets OnResume().
  void OnResume();

  // Must be called when the Activity gets OnPause().
  void OnPause();

  device::GvrGamepadData GetGamepadData();

  // Must be called when the GL renderer gets OnDrawFrame().
  void UpdateState(const gfx::Vector3dF& head_direction);

  std::unique_ptr<GestureList> DetectGestures();

  bool IsTouching();

  float TouchPosX();

  float TouchPosY();

  gfx::Quaternion Orientation() const;
  void GetTransform(gfx::Transform* out) const;
  float GetOpacity() const;
  gfx::Point3F GetPointerStart() const;

  VrControllerModel::State GetModelState() const;

  bool TouchDownHappened();

  bool TouchUpHappened();

  bool ButtonUpHappened(gvr::ControllerButton button);
  bool ButtonDownHappened(gvr::ControllerButton button);
  bool ButtonState(gvr::ControllerButton button) const;

  bool IsConnected();

  base::TimeTicks GetLastOrientationTimestamp() const;
  base::TimeTicks GetLastTouchTimestamp() const;
  base::TimeTicks GetLastButtonTimestamp() const;

 private:
  enum GestureDetectorState {
    WAITING,   // waiting for user to touch down
    TOUCHING,  // touching the touch pad but not scrolling
    SCROLLING  // scrolling on the touch pad
  };

  struct TouchPoint {
    gfx::Vector2dF position;
    int64_t timestamp;
  };

  struct TouchInfo {
    TouchPoint touch_point;
    bool touch_up;
    bool touch_down;
    bool is_touching;
  };

  struct ButtonInfo {
    gvr::ControllerButton button;
    bool button_up;
    bool button_down;
    bool button_state;
    int64_t timestamp;
  };

  void UpdateGestureFromTouchInfo(blink::WebGestureEvent* gesture);

  bool GetButtonLongPressFromButtonInfo();

  // Handle the waiting state.
  void HandleWaitingState(blink::WebGestureEvent* gesture);

  // Handle the detecting state.
  void HandleDetectingState(blink::WebGestureEvent* gesture);

  // Handle the scrolling state.
  void HandleScrollingState(blink::WebGestureEvent* gesture);
  void UpdateTouchInfo();

  // Returns true if the touch position is within the slop of the initial touch
  // point, false otherwise.
  bool InSlop(const gfx::Vector2dF touch_position);

  // Returns true if the gesture is in horizontal direction.
  bool IsHorizontalGesture();

  void Reset();

  void UpdateGestureParameters();

  // If the user is touching the touch pad and the touch point is different from
  // before, update the touch point and return true. Otherwise, return false.
  bool UpdateCurrentTouchpoint();

  void UpdateOverallVelocity();

  // State of gesture detector.
  GestureDetectorState state_;

  std::unique_ptr<gvr::ControllerApi> controller_api_;

  // The last controller state (updated once per frame).
  std::unique_ptr<gvr::ControllerState> controller_state_;

  std::unique_ptr<gvr::GvrApi> gvr_api_;

  float last_qx_;
  bool pinch_started_;
  bool zoom_in_progress_ = false;
  bool touch_position_changed_ = false;

  // Handedness from user prefs.
  gvr::ControllerHandedness handedness_;

  // Current touch info after the extrapolation.
  std::unique_ptr<TouchInfo> touch_info_;

  // A pointer storing the touch point from previous frame.
  std::unique_ptr<TouchPoint> prev_touch_point_;

  // A pointer storing the touch point from current frame.
  std::unique_ptr<TouchPoint> cur_touch_point_;

  // Initial touch point.
  std::unique_ptr<TouchPoint> init_touch_point_;

  // Overall velocity
  gfx::Vector2dF overall_velocity_;

  // Last velocity that is used for fling and direction detection
  gfx::Vector2dF last_velocity_;

  // Displacement of the touch point from the previews to the current touch
  gfx::Vector2dF displacement_;

  int64_t last_touch_timestamp_ = 0;
  int64_t last_timestamp_nanos_ = 0;

  // Number of consecutively extrapolated touch points
  int extrapolated_touch_ = 0;

  std::unique_ptr<ElbowModel> elbow_model_;

  DISALLOW_COPY_AND_ASSIGN(VrController);
};

}  // namespace vr_shell

#endif  // CHROME_BROWSER_ANDROID_VR_SHELL_VR_CONTROLLER_H_
