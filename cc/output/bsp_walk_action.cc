// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/output/bsp_walk_action.h"

#include <memory>
#include <vector>

#include "cc/output/direct_renderer.h"
#include "cc/output/draw_polygon.h"
#include "components/viz/common/quads/draw_quad.h"

namespace cc {

BspWalkActionDrawPolygon::BspWalkActionDrawPolygon(
    DirectRenderer* renderer,
    const gfx::Rect& render_pass_scissor,
    bool using_scissor_as_optimization)
    : renderer_(renderer),
      render_pass_scissor_(render_pass_scissor),
      using_scissor_as_optimization_(using_scissor_as_optimization) {
}

void BspWalkActionDrawPolygon::operator()(DrawPolygon* item) {
  gfx::Transform inverse_transform;
  bool invertible =
      item->original_ref()
          ->shared_quad_state->quad_to_target_transform.GetInverse(
              &inverse_transform);
  DCHECK(invertible);
  item->TransformToLayerSpace(inverse_transform);
  renderer_->DoDrawPolygon(*item, render_pass_scissor_,
                           using_scissor_as_optimization_);
}

BspWalkActionToVector::BspWalkActionToVector(std::vector<DrawPolygon*>* in_list)
    : list_(in_list) {
}

void BspWalkActionToVector::operator()(DrawPolygon* item) {
  list_->push_back(item);
}

}  // namespace cc
