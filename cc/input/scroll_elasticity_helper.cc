// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/input/scroll_elasticity_helper.h"

#include "cc/layers/layer_impl.h"
#include "cc/trees/layer_tree_host_impl.h"
#include "cc/trees/layer_tree_impl.h"

namespace cc {

class ScrollElasticityHelperImpl : public ScrollElasticityHelper {
 public:
  explicit ScrollElasticityHelperImpl(LayerTreeHostImpl* layer_tree_host_impl);
  ~ScrollElasticityHelperImpl() override;

  // The amount that the view is stretched past the normal allowable bounds.
  // The "overhang" amount.
  gfx::Vector2dF StretchAmount() override;
  void SetStretchAmount(const gfx::Vector2dF& stretch_amount) override;
  bool PinnedInDirection(const gfx::Vector2dF& direction) override;
  bool CanScrollHorizontally() override;
  bool CanScrollVertically() override;
  void RequestAnimate() override;

 private:
  LayerTreeHostImpl* layer_tree_host_impl_;
};

ScrollElasticityHelperImpl::ScrollElasticityHelperImpl(
    LayerTreeHostImpl* layer_tree)
    : layer_tree_host_impl_(layer_tree) {
}

ScrollElasticityHelperImpl::~ScrollElasticityHelperImpl() {
}

gfx::Vector2dF ScrollElasticityHelperImpl::StretchAmount() {
  return -layer_tree_host_impl_->active_tree()->elastic_overscroll()->Current(
      true);
}

void ScrollElasticityHelperImpl::SetStretchAmount(
    const gfx::Vector2dF& stretch_amount) {
  if (stretch_amount == StretchAmount())
    return;

  layer_tree_host_impl_->active_tree()->elastic_overscroll()->SetCurrent(
      -stretch_amount);
  layer_tree_host_impl_->active_tree()->set_needs_update_draw_properties();
  layer_tree_host_impl_->SetNeedsCommit();
  layer_tree_host_impl_->SetNeedsRedraw();
  layer_tree_host_impl_->SetFullRootLayerDamage();
}

bool ScrollElasticityHelperImpl::PinnedInDirection(
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

bool ScrollElasticityHelperImpl::CanScrollHorizontally() {
  return layer_tree_host_impl_->active_tree()->TotalMaxScrollOffset().x() > 0;
}

bool ScrollElasticityHelperImpl::CanScrollVertically() {
  return layer_tree_host_impl_->active_tree()->TotalMaxScrollOffset().y() > 0;
}

void ScrollElasticityHelperImpl::RequestAnimate() {
  layer_tree_host_impl_->SetNeedsAnimate();
}

// static
ScrollElasticityHelper* ScrollElasticityHelper::CreateForLayerTreeHostImpl(
    LayerTreeHostImpl* layer_tree_host_impl) {
  return new ScrollElasticityHelperImpl(layer_tree_host_impl);
}

}  // namespace cc
