// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_VR_SHELL_VR_CONTROLLER_H_
#define CHROME_BROWSER_ANDROID_VR_SHELL_VR_CONTROLLER_H_

#include <memory>
#include <vector>

#include "base/macros.h"
#include "third_party/WebKit/public/platform/WebGestureEvent.h"
#include "third_party/WebKit/public/platform/WebInputEvent.h"
#include "third_party/gvr-android-sdk/src/libraries/headers/vr/gvr/capi/include/gvr_types.h"

using blink::WebGestureEvent;
using blink::WebInputEvent;

namespace gvr {
class ControllerState;
}

namespace vr_shell {

class VrController {
 public:
  // Controller API entry point.
  explicit VrController(gvr_context* gvr_context);
  ~VrController();

  // Must be called when the Activity gets OnResume().
  void OnResume();

  // Must be called when the Activity gets OnPause().
  void OnPause();

  // Must be called when the GL renderer gets OnSurfaceCreated().
  void Initialize(gvr_context* gvr_context);

  // Must be called when the GL renderer gets OnDrawFrame().
  void UpdateState();

  std::vector<std::unique_ptr<WebGestureEvent>> DetectGestures();

  bool IsTouching();

  float TouchPosX();

  float TouchPosY();

  const gvr::Quatf Orientation();

  bool TouchDownHappened();

  bool TouchUpHappened();

  bool ButtonUpHappened(gvr::ControllerButton button);
  bool ButtonDownHappened(gvr::ControllerButton button);

  bool IsConnected();

 private:
  enum GestureDetectorState {
    WAITING,   // waiting for user to touch down
    TOUCHING,  // touching the touch pad but not scrolling
    SCROLLING  // scrolling on the touch pad
  };

  struct TouchPoint {
    gvr::Vec2f position;
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

  void UpdateGestureFromTouchInfo(WebGestureEvent* gesture);

  bool GetButtonLongPressFromButtonInfo();

  // Handle the waiting state.
  void HandleWaitingState(WebGestureEvent* gesture);

  // Handle the detecting state.
  void HandleDetectingState(WebGestureEvent* gesture);

  // Handle the scrolling state.
  void HandleScrollingState(WebGestureEvent* gesture);
  void UpdateTouchInfo();

  // Returns true if the touch position is within the slop of the initial touch
  // point, false otherwise.
  bool InSlop(const gvr::Vec2f touch_position);

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

  float last_qx_;
  bool pinch_started_;
  bool zoom_in_progress_ = false;
  bool touch_position_changed_ = false;

  // Current touch info after the extrapolation.
  std::unique_ptr<TouchInfo> touch_info_;

  // A pointer storing the touch point from previous frame.
  std::unique_ptr<TouchPoint> prev_touch_point_;

  // A pointer storing the touch point from current frame.
  std::unique_ptr<TouchPoint> cur_touch_point_;

  // Initial touch point.
  std::unique_ptr<TouchPoint> init_touch_point_;

  // Overall velocity
  gvr::Vec2f overall_velocity_;

  // Last velocity that is used for fling and direction detection
  gvr::Vec2f last_velocity_;

  // Displacement of the touch point from the previews to the current touch
  gvr::Vec2f displacement_;

  int64_t last_touch_timestamp_ = 0;
  int64_t last_timestamp_nanos_ = 0;

  // Number of consecutively extrapolated touch points
  int extrapolated_touch_ = 0;

  DISALLOW_COPY_AND_ASSIGN(VrController);
};

}  // namespace vr_shell

#endif  // CHROME_BROWSER_ANDROID_VR_SHELL_VR_CONTROLLER_H_
