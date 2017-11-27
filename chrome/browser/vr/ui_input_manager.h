// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_UI_INPUT_MANAGER_H_
#define CHROME_BROWSER_VR_UI_INPUT_MANAGER_H_

#include <memory>
#include <vector>

#include "base/time/time.h"
#include "ui/gfx/geometry/point3_f.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/vector3d_f.h"
#include "ui/gfx/transform.h"

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

  // When testing, it can be useful to hit test directly along the laser.
  // Updating the strategy permits this behavior, but it should not be used in
  // production. In production, we hit test along a ray that extends from the
  // world origin to a point far along the laser.
  enum HitTestStrategy {
    PROJECT_TO_WORLD_ORIGIN,
    PROJECT_TO_LASER_ORIGIN_FOR_TEST,
  };

  explicit UiInputManager(UiScene* scene);
  ~UiInputManager();
  // TODO(tiborg): Use generic gesture type instead of blink::WebGestureEvent.
  void HandleInput(base::TimeTicks current_time,
                   const ControllerModel& controller_model,
                   ReticleModel* reticle_model,
                   GestureList* gesture_list);

  bool controller_quiescent() const { return controller_quiescent_; }

  void set_hit_test_strategy(HitTestStrategy strategy) {
    hit_test_strategy_ = strategy;
  }

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
                              ReticleModel* reticle_model) const;
  void UpdateQuiescenceState(base::TimeTicks current_time,
                             const ControllerModel& controller_model);

  UiScene* scene_;
  int hover_target_id_ = 0;
  // TODO(mthiesse): We shouldn't have a fling target. Elements should fling
  // independently and we should only cancel flings on the relevant element
  // when we do cancel flings.
  int fling_target_id_ = 0;
  int input_locked_element_id_ = 0;
  bool in_click_ = false;
  bool in_scroll_ = false;

  HitTestStrategy hit_test_strategy_ = HitTestStrategy::PROJECT_TO_WORLD_ORIGIN;
  ButtonState previous_button_state_ = ButtonState::UP;

  base::TimeTicks last_significant_controller_update_time_;
  gfx::Transform last_significant_controller_transform_;
  bool controller_quiescent_ = false;
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_UI_INPUT_MANAGER_H_
