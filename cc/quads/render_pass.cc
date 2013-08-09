// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/quads/render_pass.h"

#include "base/values.h"
#include "cc/base/math_util.h"
#include "cc/debug/traced_value.h"
#include "cc/output/copy_output_request.h"
#include "cc/quads/draw_quad.h"
#include "cc/quads/shared_quad_state.h"

namespace cc {

void* RenderPass::Id::AsTracingId() const {
  COMPILE_ASSERT(sizeof(size_t) <= sizeof(void*), size_t_bigger_than_pointer);
  return reinterpret_cast<void*>(base::HashPair(layer_id, index));
}

scoped_ptr<RenderPass> RenderPass::Create() {
  return make_scoped_ptr(new RenderPass);
}

RenderPass::RenderPass()
    : id(Id(-1, -1)),
      has_transparent_background(true),
      has_occlusion_from_outside_target_surface(false) {}

RenderPass::~RenderPass() {
  TRACE_EVENT_OBJECT_DELETED_WITH_ID(
      TRACE_DISABLED_BY_DEFAULT("cc.debug.quads"),
      "cc::RenderPass", id.AsTracingId());
}

scoped_ptr<RenderPass> RenderPass::Copy(Id new_id) const {
  scoped_ptr<RenderPass> copy_pass(Create());
  copy_pass->SetAll(new_id,
                    output_rect,
                    damage_rect,
                    transform_to_root_target,
                    has_transparent_background,
                    has_occlusion_from_outside_target_surface);
  return copy_pass.Pass();
}

void RenderPass::SetNew(Id id,
                        gfx::Rect output_rect,
                        gfx::RectF damage_rect,
                        const gfx::Transform& transform_to_root_target) {
  DCHECK_GT(id.layer_id, 0);
  DCHECK_GE(id.index, 0);

  this->id = id;
  this->output_rect = output_rect;
  this->damage_rect = damage_rect;
  this->transform_to_root_target = transform_to_root_target;

  DCHECK(quad_list.empty());
  DCHECK(shared_quad_state_list.empty());
}

void RenderPass::SetAll(Id id,
                        gfx::Rect output_rect,
                        gfx::RectF damage_rect,
                        const gfx::Transform& transform_to_root_target,
                        bool has_transparent_background,
                        bool has_occlusion_from_outside_target_surface) {
  DCHECK_GT(id.layer_id, 0);
  DCHECK_GE(id.index, 0);

  this->id = id;
  this->output_rect = output_rect;
  this->damage_rect = damage_rect;
  this->transform_to_root_target = transform_to_root_target;
  this->has_transparent_background = has_transparent_background;
  this->has_occlusion_from_outside_target_surface =
      has_occlusion_from_outside_target_surface;

  DCHECK(quad_list.empty());
  DCHECK(shared_quad_state_list.empty());
}

scoped_ptr<base::Value> RenderPass::AsValue() const {
  scoped_ptr<base::DictionaryValue> value(new base::DictionaryValue());
  value->Set("output_rect", MathUtil::AsValue(output_rect).release());
  value->Set("damage_rect", MathUtil::AsValue(damage_rect).release());
  value->SetBoolean("has_transparent_background", has_transparent_background);
  value->SetBoolean("has_occlusion_from_outside_target_surface",
                    has_occlusion_from_outside_target_surface);
  value->SetInteger("copy_requests", copy_requests.size());
  scoped_ptr<base::ListValue> shared_states_value(new base::ListValue());
  for (size_t i = 0; i < shared_quad_state_list.size(); ++i) {
    shared_states_value->Append(shared_quad_state_list[i]->AsValue().release());
  }
  value->Set("shared_quad_state_list", shared_states_value.release());
  scoped_ptr<base::ListValue> quad_list_value(new base::ListValue());
  for (size_t i = 0; i < quad_list.size(); ++i) {
    quad_list_value->Append(quad_list[i]->AsValue().release());
  }
  value->Set("quad_list", quad_list_value.release());

  TracedValue::MakeDictIntoImplicitSnapshotWithCategory(
      TRACE_DISABLED_BY_DEFAULT("cc.debug.quads"),
      value.get(), "cc::RenderPass", id.AsTracingId());
  return value.PassAs<base::Value>();
}

}  // namespace cc
