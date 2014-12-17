// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/input/scroll_elasticity_helper.h"

#include "cc/layers/layer_impl.h"
#include "cc/trees/layer_tree_host_impl.h"
#include "cc/trees/layer_tree_impl.h"

namespace cc {

ScrollElasticityHelper::ScrollElasticityHelper(LayerTreeHostImpl* layer_tree)
    : layer_tree_host_impl_(layer_tree), timer_active_(false) {
}

ScrollElasticityHelper::~ScrollElasticityHelper() {
}

bool ScrollElasticityHelper::AllowsHorizontalStretching() {
  // The WebKit implementation has this interface because it is written in terms
  // of overscrolling on a per-layer basis, not for the whole layer tree. In
  // that implementation, this always returns true for the frame view's
  // scrollable area.
  // TODO(ccameron): This is function is redundant and may be removed.
  return true;
}

bool ScrollElasticityHelper::AllowsVerticalStretching() {
  // TODO(ccameron): This is function is redundant and may be removed.
  return true;
}

gfx::Vector2dF ScrollElasticityHelper::StretchAmount() {
  // TODO(ccameron): Use the value of active_tree->elastic_overscroll directly
  return stretch_offset_;
}

bool ScrollElasticityHelper::PinnedInDirection(
    const gfx::Vector2dF& direction) {
  gfx::ScrollOffset scroll_offset =
      layer_tree_host_impl_->active_tree()->TotalScrollOffset();
  gfx::ScrollOffset max_scroll_offset =
      layer_tree_host_impl_->active_tree()->TotalMaxScrollOffset();
  bool result = false;
  if (direction.x() < 0)
    result |= scroll_offset.x() <= 0;
  if (direction.x() > 0)
    result |= scroll_offset.x() >= max_scroll_offset.x();
  if (direction.y() < 0)
    result |= scroll_offset.y() <= 0;
  if (direction.y() > 0)
    result |= scroll_offset.y() >= max_scroll_offset.y();
  return result;
}

bool ScrollElasticityHelper::CanScrollHorizontally() {
  return layer_tree_host_impl_->active_tree()->TotalMaxScrollOffset().x() > 0;
}

bool ScrollElasticityHelper::CanScrollVertically() {
  return layer_tree_host_impl_->active_tree()->TotalMaxScrollOffset().y() > 0;
}

gfx::Vector2dF ScrollElasticityHelper::AbsoluteScrollPosition() {
  // TODO(ccameron): This is function is redundant and may be removed.
  return StretchAmount();
}

void ScrollElasticityHelper::ImmediateScrollBy(const gfx::Vector2dF& scroll) {
  // TODO(ccameron): This is function is redundant and may be removed.
}

void ScrollElasticityHelper::ImmediateScrollByWithoutContentEdgeConstraints(
    const gfx::Vector2dF& scroll) {
  stretch_offset_ += scroll;
  // TODO(ccameron): Use the value of active_tree->elastic_overscroll directly
  // Note that this assumes that this property's true value is ever changed
  // by the impl thread. While this is true, it is redundant state.
  layer_tree_host_impl_->active_tree()->elastic_overscroll()->SetCurrent(
      -stretch_offset_);
  layer_tree_host_impl_->active_tree()->set_needs_update_draw_properties();
  layer_tree_host_impl_->SetNeedsCommit();
  layer_tree_host_impl_->SetNeedsRedraw();
  layer_tree_host_impl_->SetFullRootLayerDamage();
}

void ScrollElasticityHelper::StartSnapRubberbandTimer() {
  if (timer_active_)
    return;
  timer_active_ = true;
  layer_tree_host_impl_->SetNeedsAnimate();
}

void ScrollElasticityHelper::StopSnapRubberbandTimer() {
  timer_active_ = false;
}

void ScrollElasticityHelper::SnapRubberbandTimerFired() {
  if (timer_active_)
    layer_tree_host_impl_->SetNeedsAnimate();
}

void ScrollElasticityHelper::AdjustScrollPositionToBoundsIfNecessary() {
  // TODO(ccameron): This is function is redundant and may be removed.
}

}  // namespace cc
