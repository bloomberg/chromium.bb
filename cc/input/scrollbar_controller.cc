// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/scrollbar_layer_impl_base.h"

#include <algorithm>

#include "base/cancelable_callback.h"
#include "cc/base/math_util.h"
#include "cc/input/scrollbar.h"
#include "cc/input/scrollbar_controller.h"
#include "cc/trees/layer_tree_impl.h"
#include "cc/trees/scroll_node.h"

namespace cc {
ScrollbarController::~ScrollbarController() {
  if (cancelable_autoscroll_task_) {
    cancelable_autoscroll_task_->Cancel();
    cancelable_autoscroll_task_.reset();
  }
}

ScrollbarController::ScrollbarController(
    LayerTreeHostImpl* layer_tree_host_impl)
    : layer_tree_host_impl_(layer_tree_host_impl),
      scrollbar_scroll_is_active_(false),
      currently_captured_scrollbar_(nullptr),
      last_known_pointer_position_(gfx::PointF(0, 0)),
      drag_processed_for_current_frame_(false),
      cancelable_autoscroll_task_(nullptr) {}

void ScrollbarController::WillBeginImplFrame() {
  drag_processed_for_current_frame_ = false;
  RecomputeAutoscrollStateIfNeeded();
}

gfx::Vector2dF ScrollbarController::GetThumbRelativePoint(
    const gfx::PointF position_in_widget) {
  bool clipped;
  const gfx::PointF position_in_layer =
      GetScrollbarRelativePosition(position_in_widget, &clipped);

  if (clipped)
    return gfx::Vector2d(0, 0);

  const gfx::RectF thumb_rect(
      currently_captured_scrollbar_->ComputeThumbQuadRect());
  DCHECK(thumb_rect.Contains(position_in_layer));
  return position_in_layer - gfx::PointF(thumb_rect.origin());
}

// Performs hit test and prepares scroll deltas that will be used by GSB and
// GSU.
InputHandlerPointerResult ScrollbarController::HandlePointerDown(
    const gfx::PointF position_in_widget,
    bool shift_modifier) {
  LayerImpl* layer_impl = GetLayerHitByPoint(position_in_widget);

  // If a non-custom scrollbar layer was not found, we return early as there is
  // no point in setting additional state in the ScrollbarController. Return an
  // empty InputHandlerPointerResult in this case so that when it is bubbled up
  // to InputHandlerProxy::RouteToTypeSpecificHandler, the pointer event gets
  // passed on to the main thread.
  if (!(layer_impl && layer_impl->ToScrollbarLayer()))
    return InputHandlerPointerResult();

  currently_captured_scrollbar_ = layer_impl->ToScrollbarLayer();
  InputHandlerPointerResult scroll_result;
  scroll_result.target_scroller =
      currently_captured_scrollbar_->scroll_element_id();
  scroll_result.type = PointerResultType::kScrollbarScroll;
  layer_tree_host_impl_->active_tree()->UpdateScrollbarGeometries();
  const ScrollbarPart scrollbar_part =
      GetScrollbarPartFromPointerDown(position_in_widget);
  scroll_result.scroll_offset = GetScrollOffsetForScrollbarPart(
      scrollbar_part, currently_captured_scrollbar_->orientation(),
      shift_modifier);
  last_known_pointer_position_ = position_in_widget;
  scrollbar_scroll_is_active_ = true;
  scroll_result.scroll_units = Granularity(scrollbar_part, shift_modifier);
  if (scrollbar_part == ScrollbarPart::THUMB) {
    drag_state_ = DragState();
    drag_state_->anchor_relative_to_thumb_ =
        GetThumbRelativePoint(position_in_widget);

    // Record the current scroller offset. This will be needed to snap the
    // thumb back to its original position if the pointer moves too far away
    // from the track during a thumb drag.
    drag_state_->scroll_position_at_start_ =
        currently_captured_scrollbar_->current_pos();
  }

  if (!scroll_result.scroll_offset.IsZero()) {
    // Thumb drag is the only scrollbar manipulation that cannot produce an
    // autoscroll. All other interactions like clicking on arrows/trackparts
    // have the potential of initiating an autoscroll (if held down for long
    // enough).
    DCHECK(scrollbar_part != ScrollbarPart::THUMB);
    cancelable_autoscroll_task_ =
        std::make_unique<base::CancelableClosure>(base::Bind(
            &ScrollbarController::StartAutoScrollAnimation,
            base::Unretained(this),
            InitialDeltaToAutoscrollVelocity(scroll_result.scroll_offset),
            currently_captured_scrollbar_->scroll_element_id(),
            scrollbar_part));
    layer_tree_host_impl_->task_runner_provider()
        ->ImplThreadTaskRunner()
        ->PostDelayedTask(FROM_HERE, cancelable_autoscroll_task_->callback(),
                          kInitialAutoscrollTimerDelay);
  }
  return scroll_result;
}

bool ScrollbarController::SnapToDragOrigin(
    const gfx::PointF pointer_position_in_widget) {
  // Consult the ScrollbarTheme to check if thumb snapping is supported on the
  // current platform.
  if (!currently_captured_scrollbar_->SupportsDragSnapBack())
    return false;

  bool clipped = false;
  const gfx::PointF pointer_position_in_layer =
      GetScrollbarRelativePosition(pointer_position_in_widget, &clipped);

  if (clipped)
    return false;

  layer_tree_host_impl_->active_tree()->UpdateScrollbarGeometries();
  const ScrollbarOrientation orientation =
      currently_captured_scrollbar_->orientation();
  const gfx::Rect forward_track_rect =
      currently_captured_scrollbar_->ForwardTrackRect();

  // When dragging the thumb, there needs to exist "gutters" on either side of
  // the track. The thickness of these gutters is a multiple of the track (or
  // thumb) thickness. As long as the pointer remains within the bounds of these
  // gutters in the non-scrolling direction, thumb drag proceeds as expected.
  // The moment the pointer moves outside the bounds, the scroller needs to snap
  // back to the drag_origin (aka the scroll offset of the parent scroller
  // before the thumb drag initiated).
  int track_thickness = orientation == ScrollbarOrientation::VERTICAL
                            ? forward_track_rect.width()
                            : forward_track_rect.height();

  if (!track_thickness) {
    // For overlay scrollbars (or for tests that do not set up a track
    // thickness), use the thumb_thickness instead to determine the gutters.
    const int thumb_thickness = currently_captured_scrollbar_->ThumbThickness();

    // If the thumb doesn't have thickness, the gutters can't be determined.
    // Snapping shouldn't occur in this case.
    if (!thumb_thickness)
      return false;

    track_thickness = thumb_thickness;
  }

  const float gutter_thickness = kOffSideMultiplier * track_thickness;
  const float gutter_min_bound =
      orientation == ScrollbarOrientation::VERTICAL
          ? (forward_track_rect.x() - gutter_thickness)
          : (forward_track_rect.y() - gutter_thickness);
  const float gutter_max_bound =
      orientation == ScrollbarOrientation::VERTICAL
          ? (forward_track_rect.x() + track_thickness + gutter_thickness)
          : (forward_track_rect.y() + track_thickness + gutter_thickness);

  const float pointer_location = orientation == ScrollbarOrientation::VERTICAL
                                     ? pointer_position_in_layer.x()
                                     : pointer_position_in_layer.y();

  return pointer_location < gutter_min_bound ||
         pointer_location > gutter_max_bound;
}

ui::input_types::ScrollGranularity ScrollbarController::Granularity(
    const ScrollbarPart scrollbar_part,
    const bool shift_modifier) {
  const bool shift_click_on_scrollbar_track =
      shift_modifier && (scrollbar_part == ScrollbarPart::FORWARD_TRACK ||
                         scrollbar_part == ScrollbarPart::BACK_TRACK);
  if (shift_click_on_scrollbar_track || scrollbar_part == ScrollbarPart::THUMB)
    return ui::input_types::ScrollGranularity::kScrollByPrecisePixel;

  // TODO(arakeri): This needs to be updated to kLine once cc implements
  // handling it. crbug.com/959441
  return ui::input_types::ScrollGranularity::kScrollByPixel;
}

float ScrollbarController::GetScrollDeltaForShiftClick() {
  layer_tree_host_impl_->active_tree()->UpdateScrollbarGeometries();

  bool clipped = false;
  const gfx::PointF pointer_position_in_layer =
      GetScrollbarRelativePosition(last_known_pointer_position_, &clipped);

  if (clipped)
    return 0;

  const ScrollbarOrientation orientation =
      currently_captured_scrollbar_->orientation();
  const float pointer_location = orientation == ScrollbarOrientation::VERTICAL
                                     ? pointer_position_in_layer.y()
                                     : pointer_position_in_layer.x();

  // During a shift + click, the pointers current location (on the track) needs
  // to be considered as the center of the thumb and the thumb origin needs to
  // be calculated based on that. This will ensure that when shift + click is
  // processed, the thumb will be centered on the pointer.
  const int thumb_length = currently_captured_scrollbar_->ThumbLength();
  const float desired_thumb_origin = pointer_location - thumb_length / 2.f;

  const gfx::Rect thumb_rect(
      currently_captured_scrollbar_->ComputeThumbQuadRect());
  const float current_thumb_origin =
      orientation == ScrollbarOrientation::VERTICAL ? thumb_rect.y()
                                                    : thumb_rect.x();

  const float delta =
      round(std::abs(desired_thumb_origin - current_thumb_origin));
  return delta * GetScrollerToScrollbarRatio();
}

gfx::ScrollOffset ScrollbarController::GetScrollOffsetForDragPosition(
    const gfx::PointF pointer_position_in_widget) {
  layer_tree_host_impl_->active_tree()->UpdateScrollbarGeometries();

  const ScrollbarOrientation orientation =
      currently_captured_scrollbar_->orientation();
  if (SnapToDragOrigin(pointer_position_in_widget)) {
    const float delta = currently_captured_scrollbar_->current_pos() -
                        drag_state_->scroll_position_at_start_;
    return orientation == ScrollbarOrientation::VERTICAL
               ? gfx::ScrollOffset(0, -delta)
               : gfx::ScrollOffset(-delta, 0);
  }

  const gfx::Rect thumb_rect(
      currently_captured_scrollbar_->ComputeThumbQuadRect());
  const gfx::PointF drag_position_relative_to_layer =
      gfx::PointF(thumb_rect.origin()) + drag_state_->anchor_relative_to_thumb_;

  bool clipped = false;
  const gfx::PointF pointer_position_in_layer =
      GetScrollbarRelativePosition(pointer_position_in_widget, &clipped);

  if (clipped)
    return gfx::ScrollOffset(0, 0);

  // Calculate the delta based on the previously known thumb drag point.
  const gfx::Vector2dF pointer_delta =
      pointer_position_in_layer - drag_position_relative_to_layer;

  float scaled_scroller_to_scrollbar_ratio = GetScrollerToScrollbarRatio();
  float current_scroll_position = currently_captured_scrollbar_->current_pos();

  // Thumb position needs to be floored and Values between 0 and 1 are rounded
  // to one to match main thread per pixel behavior. Corresponding main thread
  // code in ScrollbarTheme::ThumbPosition
  float thumb_position = std::max(0.0f, current_scroll_position) /
                         scaled_scroller_to_scrollbar_ratio;
  thumb_position = (thumb_position < 1.0 && thumb_position > 0.0)
                       ? 1.0
                       : floorf(thumb_position);

  float delta_in_orientation = orientation == ScrollbarOrientation::VERTICAL
                                   ? pointer_delta.y()
                                   : pointer_delta.x();

  // This is effectively equal to delta_in_orientation *
  // scaled_scroller_to_scrollbar_ratio but is necessary due to truncated delta
  // value. Floored thumb_position cancels out the rounding error introduced
  // in pointer_delta due to static_cast<int> in
  // ScrollbarLayerImplBase::ComputeThumbQuadRectWithThumbThicknessScale
  float scroll_delta = (delta_in_orientation + thumb_position) *
                           scaled_scroller_to_scrollbar_ratio -
                       current_scroll_position;

  gfx::ScrollOffset scaled_thumb_drag_delta;

  // Scroll delta floored to match main thread per pixel behavior
  orientation == ScrollbarOrientation::VERTICAL
      ? scaled_thumb_drag_delta.set_y(floorf(scroll_delta))
      : scaled_thumb_drag_delta.set_x(floorf(scroll_delta));

  return scaled_thumb_drag_delta;
}

// Performs hit test and prepares scroll deltas that will be used by GSU.
InputHandlerPointerResult ScrollbarController::HandlePointerMove(
    const gfx::PointF position_in_widget) {
  last_known_pointer_position_ = position_in_widget;
  RecomputeAutoscrollStateIfNeeded();
  InputHandlerPointerResult scroll_result;

  // If a thumb drag is not in progress or if a GSU was already produced for a
  // thumb drag in this frame, there's no point in continuing on. Please see the
  // header file for details.
  if (!drag_state_.has_value() || drag_processed_for_current_frame_)
    return scroll_result;

  const ScrollNode* currently_scrolling_node =
      layer_tree_host_impl_->CurrentlyScrollingNode();

  // Thumb drag needs a scroll_node. Clear the thumb drag state and exit if it
  // is unset.
  if (currently_scrolling_node == nullptr) {
    drag_state_ = base::nullopt;
    return scroll_result;
  }

  // If scroll_offset can't be consumed, there's no point in continuing on.
  const gfx::ScrollOffset scroll_offset(
      GetScrollOffsetForDragPosition(position_in_widget));
  const gfx::Vector2dF clamped_scroll_offset(
      layer_tree_host_impl_->ComputeScrollDelta(
          *currently_scrolling_node, ScrollOffsetToVector2dF(scroll_offset)));

  if (clamped_scroll_offset.IsZero())
    return scroll_result;

  // Thumb drags have more granularity and are purely dependent on the pointer
  // movement. Hence we use kPrecisePixel when dragging the thumb.
  scroll_result.scroll_units =
      ui::input_types::ScrollGranularity::kScrollByPrecisePixel;
  scroll_result.type = PointerResultType::kScrollbarScroll;
  scroll_result.scroll_offset = gfx::ScrollOffset(clamped_scroll_offset);
  drag_processed_for_current_frame_ = true;

  return scroll_result;
}

float ScrollbarController::GetScrollerToScrollbarRatio() {
  // Calculating the delta by which the scroller layer should move when
  // dragging the thumb depends on the following factors:
  // - scrollbar_track_length
  // - scrollbar_thumb_length
  // - scroll_layer_length
  // - viewport_length
  // - device_scale_factor
  // - position_in_widget
  //
  // When a thumb drag is in progress, for every pixel that the pointer moves,
  // the delta for the corresponding scroll_layer needs to be scaled by the
  // following ratio:
  // scaled_scroller_to_scrollbar_ratio =
  //  (scroll_layer_length - viewport_length) /
  //   (scrollbar_track_length - scrollbar_thumb_length)
  //
  // |<--------------------- scroll_layer_length -------------------------->|
  //
  // +------------------------------------------------+......................
  // |                                                |                     .
  // |<-------------- viewport_length --------------->|                     .
  // |                                                |                     .
  // |                                                |                     .
  // |                                                |                     .
  // |                                                |                     .
  // |                                                |                     .
  // |                                                |                     .
  // |                                                |                     .
  // |                                                |                     .
  // |                                                |                     .
  // |  |<------- scrollbar_track_length --------->|  |                     .
  // |                                                |                     .
  // +--+-----+----------------------------+-------+--+......................
  // |<||     |############################|       ||>|
  // +--+-----+----------------------------+-------+--+
  //
  //          |<- scrollbar_thumb_length ->|
  //
  layer_tree_host_impl_->active_tree()->UpdateScrollbarGeometries();
  float scroll_layer_length =
      currently_captured_scrollbar_->scroll_layer_length();
  float scrollbar_track_length = currently_captured_scrollbar_->TrackLength();
  gfx::Rect thumb_rect(currently_captured_scrollbar_->ComputeThumbQuadRect());
  const ScrollbarOrientation orientation =
      currently_captured_scrollbar_->orientation();
  float scrollbar_thumb_length = orientation == ScrollbarOrientation::VERTICAL
                                     ? thumb_rect.height()
                                     : thumb_rect.width();

  const LayerImpl* owner_scroll_layer =
      layer_tree_host_impl_->active_tree()->ScrollableLayerByElementId(
          currently_captured_scrollbar_->scroll_element_id());
  const float viewport_length =
      orientation == ScrollbarOrientation::VERTICAL
          ? owner_scroll_layer->scroll_container_bounds().height()
          : (owner_scroll_layer->scroll_container_bounds().width());

  // For platforms which have use_zoom_for_dsf set to false (like Mac), the
  // device_scale_factor should not be used while determining the
  // scaled_scroller_to_scrollbar_ratio as thumb drag would appear jittery due
  // to constant over and under corrections.
  // (See ScrollbarController::ScreenSpaceScaleFactor()).
  float scaled_scroller_to_scrollbar_ratio =
      ((scroll_layer_length - viewport_length) /
       (scrollbar_track_length - scrollbar_thumb_length)) *
      ScreenSpaceScaleFactor();

  return scaled_scroller_to_scrollbar_ratio;
}

void ScrollbarController::RecomputeAutoscrollStateIfNeeded() {
  if (!autoscroll_state_.has_value())
    return;

  layer_tree_host_impl_->active_tree()->UpdateScrollbarGeometries();
  const ScrollbarOrientation orientation =
      currently_captured_scrollbar_->orientation();
  const gfx::Rect thumb_quad =
      currently_captured_scrollbar_->ComputeThumbQuadRect();

  bool clipped;
  gfx::PointF scroller_relative_position(
      GetScrollbarRelativePosition(last_known_pointer_position_, &clipped));

  if (clipped)
    return;

  // Based on the orientation of the scrollbar and the direction of the
  // autoscroll, the code below makes a decision of whether the track autoscroll
  // should be canceled or not.
  int thumb_start = 0;
  int thumb_end = 0;
  int pointer_position = 0;
  if (orientation == ScrollbarOrientation::VERTICAL) {
    thumb_start = thumb_quad.y();
    thumb_end = thumb_quad.y() + thumb_quad.height();
    pointer_position = scroller_relative_position.y();
  } else {
    thumb_start = thumb_quad.x();
    thumb_end = thumb_quad.x() + thumb_quad.width();
    pointer_position = scroller_relative_position.x();
  }

  // If the thumb reaches the pointer while autoscrolling, abort.
  if ((autoscroll_state_->direction ==
           AutoScrollDirection::AUTOSCROLL_FORWARD &&
       thumb_end > pointer_position) ||
      (autoscroll_state_->direction ==
           AutoScrollDirection::AUTOSCROLL_BACKWARD &&
       thumb_start < pointer_position))
    layer_tree_host_impl_->mutator_host()->ScrollAnimationAbort();

  // When the scroller is autoscrolling forward, its dimensions need to be
  // monitored. If the length of the scroller layer increases, the old one needs
  // to be aborted and a new autoscroll animation needs to start. This needs to
  // be done only for the "autoscroll forward" case. Autoscrolling backward
  // always has a constant value to animate to (which is '0'. See the function
  // ScrollbarController::StartAutoScrollAnimation).
  if (autoscroll_state_->direction == AutoScrollDirection::AUTOSCROLL_FORWARD) {
    const float scroll_layer_length =
        currently_captured_scrollbar_->scroll_layer_length();
    if (autoscroll_state_->scroll_layer_length != scroll_layer_length) {
      layer_tree_host_impl_->mutator_host()->ScrollAnimationAbort();
      StartAutoScrollAnimation(
          autoscroll_state_->velocity,
          currently_captured_scrollbar_->scroll_element_id(),
          autoscroll_state_->pressed_scrollbar_part);
    }
  }

  // The animations need to be aborted/restarted based on the pointer location
  // (i.e leaving/entering the track/arrows, reaching the track end etc). The
  // autoscroll_state_ however, needs to be reset on pointer changes.
  const gfx::RectF scrollbar_part_rect(
      GetRectForScrollbarPart(autoscroll_state_->pressed_scrollbar_part));
  if (!scrollbar_part_rect.Contains(scroller_relative_position)) {
    // Stop animating if pointer moves outside the rect bounds.
    layer_tree_host_impl_->mutator_host()->ScrollAnimationAbort();
  } else if (scrollbar_part_rect.Contains(scroller_relative_position) &&
             !layer_tree_host_impl_->mutator_host()->IsElementAnimating(
                 currently_captured_scrollbar_->scroll_element_id())) {
    // Start animating if pointer re-enters the bounds.
    StartAutoScrollAnimation(autoscroll_state_->velocity,
                             currently_captured_scrollbar_->scroll_element_id(),
                             autoscroll_state_->pressed_scrollbar_part);
  }
}

// Helper to calculate the autoscroll velocity.
float ScrollbarController::InitialDeltaToAutoscrollVelocity(
    gfx::ScrollOffset scroll_offset) const {
  const float scroll_delta = currently_captured_scrollbar_->orientation() ==
                                     ScrollbarOrientation::VERTICAL
                                 ? scroll_offset.y()
                                 : scroll_offset.x();
  return scroll_delta * kAutoscrollMultiplier;
}

void ScrollbarController::StartAutoScrollAnimation(
    const float velocity,
    ElementId element_id,
    ScrollbarPart pressed_scrollbar_part) {
  // Autoscroll and thumb drag are mutually exclusive. Both can't be active at
  // the same time.
  DCHECK(!drag_state_.has_value());
  DCHECK_NE(velocity, 0);

  // scroll_node is set up while handling GSB. If there's no node to scroll, we
  // don't need to create any animation for it.
  ScrollTree& scroll_tree =
      layer_tree_host_impl_->active_tree()->property_trees()->scroll_tree;
  ScrollNode* scroll_node = scroll_tree.FindNodeFromElementId(element_id);

  if (!(scroll_node && scrollbar_scroll_is_active_))
    return;

  layer_tree_host_impl_->active_tree()->UpdateScrollbarGeometries();

  // TODO(arakeri): The animation needs to be readjusted if the scroller length
  // changes. Tracked here: crbug.com/972485
  float scroll_layer_length =
      currently_captured_scrollbar_->scroll_layer_length();

  gfx::ScrollOffset current_offset =
      scroll_tree.current_scroll_offset(scroll_node->element_id);

  // Determine the max offset for the scroll based on the scrolling direction.
  // Negative scroll velocity indicates backwards scrolling whereas a positive
  // value indicates forwards scrolling.
  const float target_offset = velocity < 0 ? 0 : scroll_layer_length;
  const gfx::Vector2dF target_offset_vector =
      currently_captured_scrollbar_->orientation() ==
              ScrollbarOrientation::VERTICAL
          ? gfx::Vector2dF(current_offset.x(), target_offset)
          : gfx::Vector2dF(target_offset, current_offset.y());

  autoscroll_state_ = AutoScrollState();
  autoscroll_state_->velocity = velocity;
  autoscroll_state_->scroll_layer_length = scroll_layer_length;
  autoscroll_state_->pressed_scrollbar_part = pressed_scrollbar_part;
  autoscroll_state_->direction = velocity < 0
                                     ? AutoScrollDirection::AUTOSCROLL_BACKWARD
                                     : AutoScrollDirection::AUTOSCROLL_FORWARD;

  layer_tree_host_impl_->AutoScrollAnimationCreate(
      scroll_node, target_offset_vector, std::abs(velocity));
}

// Performs hit test and prepares scroll deltas that will be used by GSE.
InputHandlerPointerResult ScrollbarController::HandlePointerUp(
    const gfx::PointF position_in_widget) {
  InputHandlerPointerResult scroll_result;
  if (scrollbar_scroll_is_active_) {
    scrollbar_scroll_is_active_ = false;
    scroll_result.type = PointerResultType::kScrollbarScroll;
  }

  // TODO(arakeri): This needs to be moved to ScrollOffsetAnimationsImpl as it
  // has knowledge about what type of animation is running. crbug.com/976353
  // Only abort the animation if it is an "autoscroll" animation.
  if (autoscroll_state_.has_value())
    layer_tree_host_impl_->mutator_host()->ScrollAnimationAbort();

  if (cancelable_autoscroll_task_) {
    cancelable_autoscroll_task_->Cancel();
    cancelable_autoscroll_task_.reset();
  }

  drag_state_ = base::nullopt;
  autoscroll_state_ = base::nullopt;
  return scroll_result;
}

// Returns the layer that is hit by the position_in_widget.
LayerImpl* ScrollbarController::GetLayerHitByPoint(
    const gfx::PointF position_in_widget) {
  LayerTreeImpl* active_tree = layer_tree_host_impl_->active_tree();
  gfx::Point viewport_point(position_in_widget.x(), position_in_widget.y());

  gfx::PointF device_viewport_point = gfx::ScalePoint(
      gfx::PointF(viewport_point), active_tree->device_scale_factor());
  LayerImpl* layer_impl =
      active_tree->FindLayerThatIsHitByPoint(device_viewport_point);

  return layer_impl;
}

int ScrollbarController::GetScrollDeltaForScrollbarPart(
    const ScrollbarPart scrollbar_part,
    const bool shift_modifier) {
  int scroll_delta = 0;
  int viewport_length = 0;
  LayerImpl* owner_scroll_layer = nullptr;

  switch (scrollbar_part) {
    case ScrollbarPart::BACK_BUTTON:
    case ScrollbarPart::FORWARD_BUTTON:
      scroll_delta = kPixelsPerLineStep;
      break;
    case ScrollbarPart::BACK_TRACK:
    case ScrollbarPart::FORWARD_TRACK:
      if (shift_modifier) {
        scroll_delta = GetScrollDeltaForShiftClick();
        break;
      }
      owner_scroll_layer =
          layer_tree_host_impl_->active_tree()->ScrollableLayerByElementId(
              currently_captured_scrollbar_->scroll_element_id());
      viewport_length =
          currently_captured_scrollbar_->orientation() ==
                  ScrollbarOrientation::VERTICAL
              ? owner_scroll_layer->scroll_container_bounds().height()
              : (owner_scroll_layer->scroll_container_bounds().width());
      scroll_delta = viewport_length * kMinFractionToStepWhenPaging;
      break;
    default:
      scroll_delta = 0;
  }

  return scroll_delta * ScreenSpaceScaleFactor();
}

float ScrollbarController::ScreenSpaceScaleFactor() const {
  // TODO(arakeri): When crbug.com/716231 is fixed, this needs to be updated.
  // If use_zoom_for_dsf is false, the click deltas and thumb drag ratios
  // shouldn't be scaled. For example: On Mac, when the use_zoom_for_dsf is
  // false and the device_scale_factor is 2, the scroll delta for pointer clicks
  // on arrows would be incorrectly calculated as 80px instead of 40px. This is
  // also necessary to ensure that hit testing works as intended.
  return layer_tree_host_impl_->settings().use_zoom_for_dsf
             ? layer_tree_host_impl_->active_tree()->device_scale_factor()
             : 1.f;
}

gfx::PointF ScrollbarController::GetScrollbarRelativePosition(
    const gfx::PointF position_in_widget,
    bool* clipped) {
  gfx::Transform inverse_screen_space_transform(
      gfx::Transform::kSkipInitialization);

  // If use_zoom_for_dsf is false, the ScreenSpaceTransform needs to be scaled
  // down by the DSF to ensure that position_in_widget is transformed correctly.
  const float scale =
      !layer_tree_host_impl_->settings().use_zoom_for_dsf
          ? 1.f / layer_tree_host_impl_->active_tree()->device_scale_factor()
          : 1.f;
  gfx::Transform scaled_screen_space_transform(
      currently_captured_scrollbar_->ScreenSpaceTransform());
  scaled_screen_space_transform.PostScale(scale, scale);
  if (!scaled_screen_space_transform.GetInverse(
          &inverse_screen_space_transform))
    return gfx::PointF(0, 0);

  return gfx::PointF(MathUtil::ProjectPoint(inverse_screen_space_transform,
                                            position_in_widget, clipped));
}

// Determines the ScrollbarPart based on the position_in_widget.
ScrollbarPart ScrollbarController::GetScrollbarPartFromPointerDown(
    const gfx::PointF position_in_widget) {
  // position_in_widget needs to be transformed and made relative to the
  // scrollbar layer because hit testing assumes layer relative coordinates.
  bool clipped = false;

  const gfx::PointF scroller_relative_position(
      GetScrollbarRelativePosition(position_in_widget, &clipped));

  if (clipped)
    return ScrollbarPart::NO_PART;

  return currently_captured_scrollbar_->IdentifyScrollbarPart(
      scroller_relative_position);
}

// Determines the corresponding rect for the given scrollbar part.
gfx::Rect ScrollbarController::GetRectForScrollbarPart(
    const ScrollbarPart scrollbar_part) {
  if (scrollbar_part == ScrollbarPart::BACK_BUTTON)
    return currently_captured_scrollbar_->BackButtonRect();
  if (scrollbar_part == ScrollbarPart::FORWARD_BUTTON)
    return currently_captured_scrollbar_->ForwardButtonRect();
  if (scrollbar_part == ScrollbarPart::BACK_TRACK)
    return currently_captured_scrollbar_->BackTrackRect();
  if (scrollbar_part == ScrollbarPart::FORWARD_TRACK)
    return currently_captured_scrollbar_->ForwardTrackRect();
  return gfx::Rect(0, 0);
}

// Determines the scroll offsets based on the ScrollbarPart and the scrollbar
// orientation.
gfx::ScrollOffset ScrollbarController::GetScrollOffsetForScrollbarPart(
    const ScrollbarPart scrollbar_part,
    const ScrollbarOrientation orientation,
    const bool shift_modifier) {
  float scroll_delta =
      GetScrollDeltaForScrollbarPart(scrollbar_part, shift_modifier);

  // See CreateScrollStateForGesture for more information on how these values
  // will be interpreted.
  if (scrollbar_part == ScrollbarPart::BACK_BUTTON) {
    return orientation == ScrollbarOrientation::VERTICAL
               ? gfx::ScrollOffset(0, -scroll_delta)   // Up arrow
               : gfx::ScrollOffset(-scroll_delta, 0);  // Left arrow
  } else if (scrollbar_part == ScrollbarPart::FORWARD_BUTTON) {
    return orientation == ScrollbarOrientation::VERTICAL
               ? gfx::ScrollOffset(0, scroll_delta)   // Down arrow
               : gfx::ScrollOffset(scroll_delta, 0);  // Right arrow
  } else if (scrollbar_part == ScrollbarPart::BACK_TRACK) {
    return orientation == ScrollbarOrientation::VERTICAL
               ? gfx::ScrollOffset(0, -scroll_delta)   // Track click up
               : gfx::ScrollOffset(-scroll_delta, 0);  // Track click left
  } else if (scrollbar_part == ScrollbarPart::FORWARD_TRACK) {
    return orientation == ScrollbarOrientation::VERTICAL
               ? gfx::ScrollOffset(0, scroll_delta)   // Track click down
               : gfx::ScrollOffset(scroll_delta, 0);  // Track click right
  }

  return gfx::ScrollOffset(0, 0);
}

}  // namespace cc
