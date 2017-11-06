// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/ui_input_manager.h"

#include <algorithm>

#include "base/containers/adapters.h"
#include "base/macros.h"
#include "chrome/browser/vr/elements/ui_element.h"
#include "chrome/browser/vr/model/controller_model.h"
#include "chrome/browser/vr/model/reticle_model.h"
#include "chrome/browser/vr/ui_renderer.h"
#include "chrome/browser/vr/ui_scene.h"
// TODO(tiborg): Remove include once we use a generic type to pass scroll/fling
// gestures.
#include "third_party/WebKit/public/platform/WebGestureEvent.h"

namespace vr {

namespace {

static constexpr gfx::PointF kInvalidTargetPoint =
    gfx::PointF(std::numeric_limits<float>::max(),
                std::numeric_limits<float>::max());
static constexpr gfx::Point3F kOrigin = {0.0f, 0.0f, 0.0f};

static constexpr float kControllerQuiescenceAngularThresholdDegrees = 3.5f;
static constexpr float kControllerQuiescenceTemporalThresholdSeconds = 1.2f;

gfx::Point3F GetRayPoint(const gfx::Point3F& rayOrigin,
                         const gfx::Vector3dF& rayVector,
                         float scale) {
  return rayOrigin + gfx::ScaleVector3d(rayVector, scale);
}

bool IsScrollEvent(const GestureList& list) {
  if (list.empty()) {
    return false;
  }
  // We assume that we only need to consider the first gesture in the list.
  blink::WebInputEvent::Type type = list.front()->GetType();
  if (type == blink::WebInputEvent::kGestureScrollBegin ||
      type == blink::WebInputEvent::kGestureScrollEnd ||
      type == blink::WebInputEvent::kGestureScrollUpdate ||
      type == blink::WebInputEvent::kGestureFlingStart ||
      type == blink::WebInputEvent::kGestureFlingCancel) {
    return true;
  }
  return false;
}

bool GetTargetLocalPoint(const gfx::Vector3dF& eye_to_target,
                         const UiElement& element,
                         gfx::PointF* out_target_local_point,
                         gfx::Point3F* out_target_point,
                         float* out_distance_to_plane) {
  if (!element.GetRayDistance(kOrigin, eye_to_target, out_distance_to_plane)) {
    return false;
  }

  if (*out_distance_to_plane < 0) {
    return false;
  }

  *out_target_point =
      GetRayPoint(kOrigin, eye_to_target, *out_distance_to_plane);
  gfx::PointF unit_xy_point =
      element.GetUnitRectangleCoordinates(*out_target_point);

  out_target_local_point->set_x(0.5f + unit_xy_point.x());
  out_target_local_point->set_y(0.5f - unit_xy_point.y());
  return true;
}

}  // namespace

UiInputManager::UiInputManager(UiScene* scene) : scene_(scene) {}

UiInputManager::~UiInputManager() {}

void UiInputManager::HandleInput(base::TimeTicks current_time,
                                 const ControllerModel& controller_model,
                                 ReticleModel* reticle_model,
                                 GestureList* gesture_list) {
  UpdateQuiescenceState(current_time, controller_model);
  gfx::Vector3dF eye_to_target;
  reticle_model->target_element_id = 0;
  reticle_model->target_local_point = kInvalidTargetPoint;
  GetVisualTargetElement(controller_model, reticle_model, &eye_to_target);

  UiElement* target_element = nullptr;
  // TODO(vollick): this should be replaced with a formal notion of input
  // capture.
  if (input_locked_element_id_) {
    target_element = scene_->GetUiElementById(input_locked_element_id_);
    if (target_element) {
      gfx::Point3F plane_intersection_point;
      float distance_to_plane;
      if (!GetTargetLocalPoint(eye_to_target, *target_element,
                               &reticle_model->target_local_point,
                               &plane_intersection_point, &distance_to_plane)) {
        reticle_model->target_local_point = kInvalidTargetPoint;
      }
    }
  } else if (!in_scroll_ && !in_click_) {
    // TODO(vollick): support multiple dispatch. We may want to, for example,
    // dispatch raw events to several elements we hit (imagine nested horizontal
    // and vertical scrollers). Currently, we only dispatch to one "winner".
    target_element = scene_->GetUiElementById(reticle_model->target_element_id);
    if (target_element && IsScrollEvent(*gesture_list)) {
      UiElement* ancestor = target_element;
      while (!ancestor->scrollable() && ancestor->parent()) {
        ancestor = ancestor->parent();
      }
      if (ancestor->scrollable()) {
        target_element = ancestor;
      }
    }
  }

  SendFlingCancel(gesture_list, reticle_model->target_local_point);
  // For simplicity, don't allow scrolling while clicking until we need to.
  if (!in_click_) {
    SendScrollEnd(gesture_list, reticle_model->target_local_point,
                  controller_model.touchpad_button_state);
    if (!SendScrollBegin(target_element, gesture_list,
                         reticle_model->target_local_point)) {
      SendScrollUpdate(gesture_list, reticle_model->target_local_point);
    }
  }

  // If we're still scrolling, don't hover (and we can't be clicking, because
  // click ends scroll).
  if (in_scroll_) {
    return;
  }
  SendHoverLeave(target_element);
  if (!SendHoverEnter(target_element, reticle_model->target_local_point)) {
    SendHoverMove(reticle_model->target_local_point);
  }
  SendButtonDown(target_element, reticle_model->target_local_point,
                 controller_model.touchpad_button_state);
  if (SendButtonUp(target_element, reticle_model->target_local_point,
                   controller_model.touchpad_button_state)) {
    target_element = scene_->GetUiElementById(reticle_model->target_element_id);
    SendHoverLeave(target_element);
    SendHoverEnter(target_element, reticle_model->target_local_point);
  }

  previous_button_state_ =
      (controller_model.touchpad_button_state == ButtonState::CLICKED)
          ? ButtonState::UP
          : controller_model.touchpad_button_state;
}

void UiInputManager::SendFlingCancel(GestureList* gesture_list,
                                     const gfx::PointF& target_point) {
  if (!fling_target_id_) {
    return;
  }
  if (gesture_list->empty() || (gesture_list->front()->GetType() !=
                                blink::WebInputEvent::kGestureFlingCancel)) {
    return;
  }

  // Scrolling currently only supported on content window.
  UiElement* element = scene_->GetUiElementById(fling_target_id_);
  if (element) {
    DCHECK(element->scrollable());
    element->OnFlingCancel(std::move(gesture_list->front()), target_point);
  }
  gesture_list->erase(gesture_list->begin());
  fling_target_id_ = 0;
}

void UiInputManager::SendScrollEnd(GestureList* gesture_list,
                                   const gfx::PointF& target_point,
                                   ButtonState button_state) {
  if (!in_scroll_) {
    return;
  }
  DCHECK_GT(input_locked_element_id_, 0);
  UiElement* element = scene_->GetUiElementById(input_locked_element_id_);

  if (previous_button_state_ != button_state &&
      (button_state == ButtonState::DOWN ||
       button_state == ButtonState::CLICKED)) {
    DCHECK_GT(gesture_list->size(), 0LU);
    DCHECK_EQ(gesture_list->front()->GetType(),
              blink::WebInputEvent::kGestureScrollEnd);
  }
  if (element) {
    DCHECK(element->scrollable());
  }
  if (gesture_list->empty() || (gesture_list->front()->GetType() !=
                                    blink::WebInputEvent::kGestureScrollEnd &&
                                gesture_list->front()->GetType() !=
                                    blink::WebInputEvent::kGestureFlingStart)) {
    return;
  }
  DCHECK_LE(gesture_list->size(), 2LU);
  if (gesture_list->front()->GetType() ==
      blink::WebInputEvent::kGestureScrollEnd) {
    if (element) {
      element->OnScrollEnd(std::move(gesture_list->front()), target_point);
    }
  } else {
    DCHECK_EQ(gesture_list->front()->GetType(),
              blink::WebInputEvent::kGestureFlingStart);
    fling_target_id_ = input_locked_element_id_;
    if (element) {
      element->OnFlingStart(std::move(gesture_list->front()), target_point);
    }
  }
  gesture_list->erase(gesture_list->begin());
  input_locked_element_id_ = 0;
  in_scroll_ = false;
}

bool UiInputManager::SendScrollBegin(UiElement* target,
                                     GestureList* gesture_list,
                                     const gfx::PointF& target_point) {
  if (in_scroll_ || !target) {
    return false;
  }
  // Scrolling currently only supported on content window.
  if (!target->scrollable()) {
    return false;
  }
  if (gesture_list->empty() || (gesture_list->front()->GetType() !=
                                blink::WebInputEvent::kGestureScrollBegin)) {
    return false;
  }
  input_locked_element_id_ = target->id();
  in_scroll_ = true;
  target->OnScrollBegin(std::move(gesture_list->front()), target_point);
  gesture_list->erase(gesture_list->begin());
  return true;
}

void UiInputManager::SendScrollUpdate(GestureList* gesture_list,
                                      const gfx::PointF& target_point) {
  if (!in_scroll_) {
    return;
  }
  DCHECK(input_locked_element_id_);
  if (gesture_list->empty() || (gesture_list->front()->GetType() !=
                                blink::WebInputEvent::kGestureScrollUpdate)) {
    return;
  }
  // Scrolling currently only supported on content window.
  UiElement* element = scene_->GetUiElementById(input_locked_element_id_);
  if (element) {
    DCHECK(element->scrollable());
    element->OnScrollUpdate(std::move(gesture_list->front()), target_point);
  }
  gesture_list->erase(gesture_list->begin());
}

void UiInputManager::SendHoverLeave(UiElement* target) {
  if (!hover_target_id_) {
    return;
  }
  if (target && target->id() == hover_target_id_) {
    return;
  }
  UiElement* element = scene_->GetUiElementById(hover_target_id_);
  if (element) {
    element->OnHoverLeave();
  }
  hover_target_id_ = 0;
}

bool UiInputManager::SendHoverEnter(UiElement* target,
                                    const gfx::PointF& target_point) {
  if (!target || target->id() == hover_target_id_) {
    return false;
  }
  target->OnHoverEnter(target_point);
  hover_target_id_ = target->id();
  return true;
}

void UiInputManager::SendHoverMove(const gfx::PointF& target_point) {
  if (!hover_target_id_) {
    return;
  }
  UiElement* element = scene_->GetUiElementById(hover_target_id_);
  if (!element) {
    return;
  }

  // TODO(mthiesse, vollick): Content is currently way too sensitive to mouse
  // moves for how noisy the controller is. It's almost impossible to click a
  // link without unintentionally starting a drag event. For this reason we
  // disable mouse moves, only delivering a down and up event.
  if (element->name() == kContentQuad && in_click_) {
    return;
  }

  element->OnMove(target_point);
}

void UiInputManager::SendButtonDown(UiElement* target,
                                    const gfx::PointF& target_point,
                                    ButtonState button_state) {
  if (in_click_) {
    return;
  }
  if (previous_button_state_ == button_state ||
      (button_state != ButtonState::DOWN &&
       button_state != ButtonState::CLICKED)) {
    return;
  }
  in_click_ = true;
  if (target) {
    target->OnButtonDown(target_point);
    input_locked_element_id_ = target->id();
  } else {
    input_locked_element_id_ = 0;
  }
}

bool UiInputManager::SendButtonUp(UiElement* target,
                                  const gfx::PointF& target_point,
                                  ButtonState button_state) {
  if (!in_click_) {
    return false;
  }
  if (previous_button_state_ == button_state ||
      (button_state != ButtonState::UP &&
       button_state != ButtonState::CLICKED)) {
    return false;
  }
  in_click_ = false;
  if (!input_locked_element_id_) {
    return false;
  }
  UiElement* element = scene_->GetUiElementById(input_locked_element_id_);
  if (element) {
    target->OnButtonUp(target_point);
  }
  input_locked_element_id_ = 0;
  return true;
}

void UiInputManager::GetVisualTargetElement(
    const ControllerModel& controller_model,
    ReticleModel* reticle_model,
    gfx::Vector3dF* out_eye_to_target) const {
  // If we place the reticle based on elements intersecting the controller beam,
  // we can end up with the reticle hiding behind elements, or jumping laterally
  // in the field of view. This is physically correct, but hard to use. For
  // usability, do the following instead:
  //
  // - Project the controller laser onto a distance-limiting sphere.
  // - Create a vector between the eyes and the outer surface point.
  // - If any UI elements intersect this vector, and is within the bounding
  //   sphere, choose the closest to the eyes, and place the reticle at the
  //   intersection point.

  // Compute the distance from the eyes to the distance limiting sphere. Note
  // that the sphere is centered at the controller, rather than the eye, for
  // simplicity.
  float distance = scene_->background_distance();
  reticle_model->target_point =
      GetRayPoint(controller_model.laser_origin,
                  controller_model.laser_direction, distance);
  *out_eye_to_target = reticle_model->target_point - kOrigin;
  out_eye_to_target->GetNormalized(out_eye_to_target);

  std::vector<const UiElement*> elements;
  for (auto& element : scene_->root_element()) {
    if (element.IsVisible()) {
      elements.push_back(&element);
    }
  }

  std::vector<const UiElement*> sorted =
      UiRenderer::GetElementsInDrawOrder(elements);

  for (const auto* element : base::Reversed(sorted)) {
    if (!element->IsHitTestable()) {
      continue;
    }

    gfx::PointF local_point;
    gfx::Point3F plane_intersection_point;
    float distance_to_plane;
    if (!GetTargetLocalPoint(*out_eye_to_target, *element, &local_point,
                             &plane_intersection_point, &distance_to_plane)) {
      continue;
    }
    if (!element->HitTest(local_point)) {
      continue;
    }

    reticle_model->target_point = plane_intersection_point;
    reticle_model->target_element_id = element->id();
    reticle_model->target_local_point = local_point;
    break;
  }
}

void UiInputManager::UpdateQuiescenceState(
    base::TimeTicks current_time,
    const ControllerModel& controller_model) {
  // Update quiescence state.
  gfx::Point3F old_position;
  gfx::Point3F old_forward_position(0, 0, -1);
  last_significant_controller_transform_.TransformPoint(&old_position);
  last_significant_controller_transform_.TransformPoint(&old_forward_position);
  gfx::Vector3dF old_forward = old_forward_position - old_position;
  old_forward.GetNormalized(&old_forward);
  gfx::Point3F new_position;
  gfx::Point3F new_forward_position(0, 0, -1);
  controller_model.transform.TransformPoint(&new_position);
  controller_model.transform.TransformPoint(&new_forward_position);
  gfx::Vector3dF new_forward = new_forward_position - new_position;
  new_forward.GetNormalized(&new_forward);

  float angle = AngleBetweenVectorsInDegrees(old_forward, new_forward);
  if (angle > kControllerQuiescenceAngularThresholdDegrees || in_click_ ||
      in_scroll_) {
    controller_quiescent_ = false;
    last_significant_controller_transform_ = controller_model.transform;
    last_significant_controller_update_time_ = current_time;
  } else if ((current_time - last_significant_controller_update_time_)
                 .InSecondsF() >
             kControllerQuiescenceTemporalThresholdSeconds) {
    controller_quiescent_ = true;
  }
}

}  // namespace vr
