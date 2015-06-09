// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/viewport.h"

#include "base/logging.h"
#include "cc/input/top_controls_manager.h"
#include "cc/trees/layer_tree_host_impl.h"
#include "cc/trees/layer_tree_impl.h"
#include "ui/gfx/geometry/vector2d_conversions.h"
#include "ui/gfx/geometry/vector2d_f.h"

namespace cc {

// static
scoped_ptr<Viewport> Viewport::Create(
    LayerTreeHostImpl* host_impl) {
  return make_scoped_ptr(new Viewport(host_impl));
}

Viewport::Viewport(LayerTreeHostImpl* host_impl)
    : host_impl_(host_impl) {
  DCHECK(host_impl_);
}

void Viewport::Pan(const gfx::Vector2dF& delta) {
  gfx::Vector2dF pending_delta = delta;

  pending_delta -= host_impl_->ScrollLayer(InnerScrollLayer(),
                                           pending_delta,
                                           gfx::Point(),
                                           false);
}

Viewport::ScrollResult Viewport::ScrollBy(const gfx::Vector2dF& delta,
                                          const gfx::Point& viewport_point,
                                          bool is_wheel_scroll,
                                          bool affect_top_controls) {
  gfx::Vector2dF content_delta = delta;
  ScrollResult result;

  if (affect_top_controls && ShouldTopControlsConsumeScroll(delta)) {
    result.top_controls_applied_delta = ScrollTopControls(delta);
    content_delta -= result.top_controls_applied_delta;
  }

  gfx::Vector2dF pending_content_delta = content_delta;

  if (OuterScrollLayer()) {
    pending_content_delta -= host_impl_->ScrollLayer(OuterScrollLayer(),
                                                     pending_content_delta,
                                                     viewport_point,
                                                     is_wheel_scroll);
  }

  // TODO(bokan): This shouldn't be needed but removing it causes subtle
  // viewport movement during top controls manipulation.
  if (!gfx::ToRoundedVector2d(pending_content_delta).IsZero()) {
    pending_content_delta -= host_impl_->ScrollLayer(InnerScrollLayer(),
                                                     pending_content_delta,
                                                     viewport_point,
                                                     is_wheel_scroll);
    result.unused_scroll_delta = AdjustOverscroll(pending_content_delta);
  }


  result.applied_delta = content_delta - pending_content_delta;
  return result;
}

gfx::Vector2dF Viewport::ScrollTopControls(const gfx::Vector2dF& delta) {
  gfx::Vector2dF excess_delta =
      host_impl_->top_controls_manager()->ScrollBy(delta);

  return delta - excess_delta;
}

bool Viewport::ShouldTopControlsConsumeScroll(
    const gfx::Vector2dF& scroll_delta) const {
  // Always consume if it's in the direction to show the top controls.
  if (scroll_delta.y() < 0)
    return true;

  if (TotalScrollOffset().y() < MaxTotalScrollOffset().y())
    return true;

  return false;
}

gfx::Vector2dF Viewport::AdjustOverscroll(const gfx::Vector2dF& delta) const {
  const float kEpsilon = 0.1f;
  gfx::Vector2dF adjusted = delta;

  if (std::abs(adjusted.x()) < kEpsilon)
    adjusted.set_x(0.0f);
  if (std::abs(adjusted.y()) < kEpsilon)
    adjusted.set_y(0.0f);

  // Disable overscroll on axes which are impossible to scroll.
  if (host_impl_->settings().report_overscroll_only_for_scrollable_axes) {
    if (std::abs(MaxTotalScrollOffset().x()) <= kEpsilon ||
        !InnerScrollLayer()->user_scrollable_horizontal())
      adjusted.set_x(0.0f);
    if (std::abs(MaxTotalScrollOffset().y()) <= kEpsilon ||
        !InnerScrollLayer()->user_scrollable_vertical())
      adjusted.set_y(0.0f);
  }

  return adjusted;
}

gfx::ScrollOffset Viewport::MaxTotalScrollOffset() const {
  gfx::ScrollOffset offset;

  offset += InnerScrollLayer()->MaxScrollOffset();

  if (OuterScrollLayer())
    offset += OuterScrollLayer()->MaxScrollOffset();

  return offset;
}

gfx::ScrollOffset Viewport::TotalScrollOffset() const {
  gfx::ScrollOffset offset;

  offset += InnerScrollLayer()->CurrentScrollOffset();

  if (OuterScrollLayer())
    offset += OuterScrollLayer()->CurrentScrollOffset();

  return offset;
}

LayerImpl* Viewport::InnerScrollLayer() const {
  return host_impl_->InnerViewportScrollLayer();
}

LayerImpl* Viewport::OuterScrollLayer() const {
  return host_impl_->OuterViewportScrollLayer();
}

}  // namespace cc
