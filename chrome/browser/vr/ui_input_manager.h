// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_UI_INPUT_MANAGER_H_
#define CHROME_BROWSER_VR_UI_INPUT_MANAGER_H_

#include <memory>
#include <vector>

#include "ui/gfx/geometry/point3_f.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/vector3d_f.h"

namespace blink {
class WebGestureEvent;
}

namespace vr {

class UiScene;
class UiElement;
struct ControllerModel;
struct ReticleModel;

using GestureList = std::vector<std::unique_ptr<blink::WebGestureEvent>>;

// Based on controller input finds the hit UI element and determines the
// interaction with UI elements and the web contents.
class UiInputManager {
 public:
  enum ButtonState {
    UP,       // The button is released.
    DOWN,     // The button is pressed.
    CLICKED,  // Since the last update the button has been pressed and released.
              // The button is released now.
  };
  explicit UiInputManager(UiScene* scene);
  ~UiInputManager();
  // TODO(tiborg): Use generic gesture type instead of blink::WebGestureEvent.
  void HandleInput(const ControllerModel& controller_model,
                   ReticleModel* reticle_model,
                   GestureList* gesture_list);

 private:
  void SendFlingCancel(GestureList* gesture_list,
                       const gfx::PointF& target_point);
  void SendScrollEnd(GestureList* gesture_list,
                     const gfx::PointF& target_point,
                     ButtonState button_state);
  bool SendScrollBegin(UiElement* target,
                       GestureList* gesture_list,
                       const gfx::PointF& target_point);
  void SendScrollUpdate(GestureList* gesture_list,
                        const gfx::PointF& target_point);
  void SendHoverLeave(UiElement* target);
  bool SendHoverEnter(UiElement* target, const gfx::PointF& target_point);
  void SendHoverMove(const gfx::PointF& target_point);
  void SendButtonDown(UiElement* target,
                      const gfx::PointF& target_point,
                      ButtonState button_state);
  bool SendButtonUp(UiElement* target,
                    const gfx::PointF& target_point,
                    ButtonState button_state);
  void GetVisualTargetElement(const ControllerModel& controller_model,
                              ReticleModel* reticle_model,
                              gfx::Vector3dF* out_eye_to_target) const;

  UiScene* scene_;
  int hover_target_id_ = 0;
  // TODO(mthiesse): We shouldn't have a fling target. Elements should fling
  // independently and we should only cancel flings on the relevant element
  // when we do cancel flings.
  int fling_target_id_ = 0;
  int input_locked_element_id_ = 0;
  bool in_click_ = false;
  bool in_scroll_ = false;

  ButtonState previous_button_state_ = ButtonState::UP;
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_UI_INPUT_MANAGER_H_
