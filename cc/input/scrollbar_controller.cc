// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/scrollbar_layer_impl_base.h"

#include <algorithm>

#include "cc/base/math_util.h"
#include "cc/input/scrollbar.h"
#include "cc/input/scrollbar_controller.h"
#include "cc/trees/layer_tree_impl.h"

namespace cc {
ScrollbarController::ScrollbarController(
    LayerTreeHostImpl* layer_tree_host_impl)
    : layer_tree_host_impl_(layer_tree_host_impl),
      scrollbar_scroll_is_active_(false) {}

// Performs hit test and prepares scroll deltas that will be used by GSB and
// GSU.
InputHandlerPointerResult ScrollbarController::HandleMouseDown(
    const gfx::PointF position_in_widget) {
  InputHandlerPointerResult scroll_result;
  LayerImpl* layer_impl = GetLayerHitByPoint(position_in_widget);
  if (layer_impl && layer_impl->is_scrollbar()) {
    scrollbar_scroll_is_active_ = true;
    scroll_result.type = PointerResultType::kScrollbarScroll;
    scroll_result.scroll_offset =
        GetScrollStateBasedOnHitTest(layer_impl, position_in_widget);
  }
  return scroll_result;
}

// Performs hit test and prepares scroll deltas that will be used by GSE.
InputHandlerPointerResult ScrollbarController::HandleMouseUp(
    const gfx::PointF position_in_widget) {
  InputHandlerPointerResult scroll_result;
  if (scrollbar_scroll_is_active_) {
    scrollbar_scroll_is_active_ = false;
    scroll_result.type = PointerResultType::kScrollbarScroll;
  }
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
      active_tree->FindFirstScrollingLayerOrScrollbarThatIsHitByPoint(
          device_viewport_point);

  return layer_impl;
}

// Determines the scroll offsets based on hit test results.
gfx::ScrollOffset ScrollbarController::GetScrollStateBasedOnHitTest(
    const LayerImpl* scrollbar_layer_impl,
    const gfx::PointF position_in_widget) {
  const ScrollbarLayerImplBase* scrollbar_layer =
      static_cast<const ScrollbarLayerImplBase*>(scrollbar_layer_impl);
  const ScrollbarOrientation orientation = scrollbar_layer->orientation();

  // position_in_widget needs to be transformed and made relative to the
  // scrollbar layer because hit testing assumes layer relative coordinates.
  ScrollbarPart scrollbar_part = ScrollbarPart::NO_PART;
  gfx::Transform inverse_screen_space_transform(
      gfx::Transform::kSkipInitialization);
  if (scrollbar_layer_impl->ScreenSpaceTransform().GetInverse(
          &inverse_screen_space_transform)) {
    bool clipped;
    gfx::PointF scroller_relative_position(MathUtil::ProjectPoint(
        inverse_screen_space_transform, position_in_widget, &clipped));

    if (clipped)
      return gfx::ScrollOffset(0, 0);

    scrollbar_part =
        scrollbar_layer->IdentifyScrollbarPart(scroller_relative_position);
  }

  float scroll_delta =
      layer_tree_host_impl_->active_tree()->device_scale_factor() *
      kPixelsPerLineStep;

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
  }
  return gfx::ScrollOffset(0, 0);
}

}  // namespace cc
