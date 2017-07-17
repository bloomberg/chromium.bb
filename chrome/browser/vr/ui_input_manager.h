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

using GestureList = std::vector<std::unique_ptr<blink::WebGestureEvent>>;

// Receives interaction events with the web content from the UiInputManager.
class UiInputManagerDelegate {
 public:
  virtual ~UiInputManagerDelegate();

  virtual void OnContentEnter(const gfx::PointF& normalized_hit_point) = 0;
  virtual void OnContentLeave() = 0;
  virtual void OnContentMove(const gfx::PointF& normalized_hit_point) = 0;
  virtual void OnContentDown(const gfx::PointF& normalized_hit_point) = 0;
  virtual void OnContentUp(const gfx::PointF& normalized_hit_point) = 0;
  virtual void OnContentFlingBegin(
      std::unique_ptr<blink::WebGestureEvent> gesture,
      const gfx::PointF& normalized_hit_point) = 0;
  virtual void OnContentFlingCancel(
      std::unique_ptr<blink::WebGestureEvent> gesture,
      const gfx::PointF& normalized_hit_point) = 0;
  virtual void OnContentScrollBegin(
      std::unique_ptr<blink::WebGestureEvent> gesture,
      const gfx::PointF& normalized_hit_point) = 0;
  virtual void OnContentScrollUpdate(
      std::unique_ptr<blink::WebGestureEvent> gesture,
      const gfx::PointF& normalized_hit_point) = 0;
  virtual void OnContentScrollEnd(
      std::unique_ptr<blink::WebGestureEvent> gesture,
      const gfx::PointF& normalized_hit_point) = 0;
};

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
  UiInputManager(UiScene* scene, UiInputManagerDelegate* delegate);
  ~UiInputManager();
  // TODO(tiborg): Use generic gesture type instead of blink::WebGestureEvent.
  void HandleInput(const gfx::Vector3dF& laser_direction,
                   const gfx::Point3F& laser_origin,
                   ButtonState button_state,
                   GestureList* gesture_list,
                   gfx::Point3F* out_target_point,
                   UiElement** out_reticle_render_target);

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
  void SendButtonUp(UiElement* target,
                    const gfx::PointF& target_point,
                    ButtonState button_state);
  void GetVisualTargetElement(const gfx::Vector3dF& laser_direction,
                              const gfx::Point3F& laser_origin,
                              gfx::Vector3dF* out_eye_to_target,
                              gfx::Point3F* out_target_point,
                              UiElement** out_target_element,
                              gfx::PointF* out_target_local_point) const;
  bool GetTargetLocalPoint(const gfx::Vector3dF& eye_to_target,
                           const UiElement& element,
                           float max_distance_to_plane,
                           gfx::PointF* out_target_local_point,
                           gfx::Point3F* out_target_point,
                           float* out_distance_to_plane) const;

  UiScene* scene_;
  UiInputManagerDelegate* delegate_;
  // TODO(mthiesse): We need to handle elements being removed, and update this
  // state appropriately.
  UiElement* hover_target_ = nullptr;
  // TODO(mthiesse): We shouldn't have a fling target. Elements should fling
  // independently and we should only cancel flings on the relevant element
  // when we do cancel flings.
  UiElement* fling_target_ = nullptr;
  UiElement* input_locked_element_ = nullptr;
  bool in_click_ = false;
  bool in_scroll_ = false;

  ButtonState previous_button_state_ = ButtonState::UP;
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_UI_INPUT_MANAGER_H_
