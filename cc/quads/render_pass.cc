// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/quads/render_pass.h"

#include <stddef.h>

#include <algorithm>

#include "base/memory/ptr_util.h"
#include "base/numerics/safe_conversions.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/trace_event_argument.h"
#include "base/values.h"
#include "cc/base/math_util.h"
#include "cc/debug/traced_value.h"
#include "cc/output/copy_output_request.h"
#include "cc/quads/debug_border_draw_quad.h"
#include "cc/quads/draw_quad.h"
#include "cc/quads/largest_draw_quad.h"
#include "cc/quads/picture_draw_quad.h"
#include "cc/quads/render_pass_draw_quad.h"
#include "cc/quads/shared_quad_state.h"
#include "cc/quads/solid_color_draw_quad.h"
#include "cc/quads/stream_video_draw_quad.h"
#include "cc/quads/surface_draw_quad.h"
#include "cc/quads/texture_draw_quad.h"
#include "cc/quads/tile_draw_quad.h"
#include "cc/quads/yuv_video_draw_quad.h"

namespace {
const size_t kDefaultNumSharedQuadStatesToReserve = 32;
const size_t kDefaultNumQuadsToReserve = 128;
}

namespace cc {

QuadList::QuadList()
    : ListContainer<DrawQuad>(LargestDrawQuadAlignment(),
                              LargestDrawQuadSize(),
                              kDefaultNumSharedQuadStatesToReserve) {}

QuadList::QuadList(size_t default_size_to_reserve)
    : ListContainer<DrawQuad>(LargestDrawQuadAlignment(),
                              LargestDrawQuadSize(),
                              default_size_to_reserve) {}

std::unique_ptr<RenderPass> RenderPass::Create() {
  return base::WrapUnique(new RenderPass());
}

std::unique_ptr<RenderPass> RenderPass::Create(size_t num_layers) {
  return base::WrapUnique(new RenderPass(num_layers));
}

std::unique_ptr<RenderPass> RenderPass::Create(
    size_t shared_quad_state_list_size,
    size_t quad_list_size) {
  return base::WrapUnique(
      new RenderPass(shared_quad_state_list_size, quad_list_size));
}

RenderPass::RenderPass()
    : quad_list(kDefaultNumQuadsToReserve),
      shared_quad_state_list(ALIGNOF(SharedQuadState),
                             sizeof(SharedQuadState),
                             kDefaultNumSharedQuadStatesToReserve) {}

// Each layer usually produces one shared quad state, so the number of layers
// is a good hint for what to reserve here.
RenderPass::RenderPass(size_t num_layers)
    : has_transparent_background(true),
      quad_list(kDefaultNumQuadsToReserve),
      shared_quad_state_list(ALIGNOF(SharedQuadState),
                             sizeof(SharedQuadState),
                             num_layers) {}

RenderPass::RenderPass(size_t shared_quad_state_list_size,
                       size_t quad_list_size)
    : has_transparent_background(true),
      quad_list(quad_list_size),
      shared_quad_state_list(ALIGNOF(SharedQuadState),
                             sizeof(SharedQuadState),
                             shared_quad_state_list_size) {}

RenderPass::~RenderPass() {
  TRACE_EVENT_OBJECT_DELETED_WITH_ID(
      TRACE_DISABLED_BY_DEFAULT("cc.debug.quads"), "cc::RenderPass",
      reinterpret_cast<void*>(id));
}

std::unique_ptr<RenderPass> RenderPass::Copy(int new_id) const {
  std::unique_ptr<RenderPass> copy_pass(
      Create(shared_quad_state_list.size(), quad_list.size()));
  copy_pass->SetAll(new_id, output_rect, damage_rect, transform_to_root_target,
                    filters, background_filters, color_space,
                    has_transparent_background);
  return copy_pass;
}

std::unique_ptr<RenderPass> RenderPass::DeepCopy() const {
  // Since we can't copy these, it's wrong to use DeepCopy in a situation where
  // you may have copy_requests present.
  DCHECK_EQ(copy_requests.size(), 0u);

  std::unique_ptr<RenderPass> copy_pass(
      Create(shared_quad_state_list.size(), quad_list.size()));
  copy_pass->SetAll(id, output_rect, damage_rect, transform_to_root_target,
                    filters, background_filters, color_space,
                    has_transparent_background);
  for (auto* shared_quad_state : shared_quad_state_list) {
    SharedQuadState* copy_shared_quad_state =
        copy_pass->CreateAndAppendSharedQuadState();
    *copy_shared_quad_state = *shared_quad_state;
  }
  SharedQuadStateList::ConstIterator sqs_iter = shared_quad_state_list.begin();
  SharedQuadStateList::Iterator copy_sqs_iter =
      copy_pass->shared_quad_state_list.begin();
  for (auto* quad : quad_list) {
    while (quad->shared_quad_state != *sqs_iter) {
      ++sqs_iter;
      ++copy_sqs_iter;
      DCHECK(sqs_iter != shared_quad_state_list.end());
    }
    DCHECK(quad->shared_quad_state == *sqs_iter);

    SharedQuadState* copy_shared_quad_state = *copy_sqs_iter;

    if (quad->material == DrawQuad::RENDER_PASS) {
      const RenderPassDrawQuad* pass_quad =
          RenderPassDrawQuad::MaterialCast(quad);
      copy_pass->CopyFromAndAppendRenderPassDrawQuad(
          pass_quad, copy_shared_quad_state, pass_quad->render_pass_id);
    } else {
      copy_pass->CopyFromAndAppendDrawQuad(quad, copy_shared_quad_state);
    }
  }
  return copy_pass;
}

// static
void RenderPass::CopyAll(const std::vector<std::unique_ptr<RenderPass>>& in,
                         std::vector<std::unique_ptr<RenderPass>>* out) {
  for (const auto& source : in)
    out->push_back(source->DeepCopy());
}

void RenderPass::SetNew(int id,
                        const gfx::Rect& output_rect,
                        const gfx::Rect& damage_rect,
                        const gfx::Transform& transform_to_root_target) {
  DCHECK(id);
  DCHECK(damage_rect.IsEmpty() || output_rect.Contains(damage_rect))
      << "damage_rect: " << damage_rect.ToString()
      << " output_rect: " << output_rect.ToString();

  this->id = id;
  this->output_rect = output_rect;
  this->damage_rect = damage_rect;
  this->transform_to_root_target = transform_to_root_target;

  DCHECK(quad_list.empty());
  DCHECK(shared_quad_state_list.empty());
}

void RenderPass::SetAll(int id,
                        const gfx::Rect& output_rect,
                        const gfx::Rect& damage_rect,
                        const gfx::Transform& transform_to_root_target,
                        const FilterOperations& filters,
                        const FilterOperations& background_filters,
                        const gfx::ColorSpace& color_space,
                        bool has_transparent_background) {
  DCHECK(id);

  this->id = id;
  this->output_rect = output_rect;
  this->damage_rect = damage_rect;
  this->transform_to_root_target = transform_to_root_target;
  this->filters = filters;
  this->background_filters = background_filters;
  this->color_space = color_space;
  this->has_transparent_background = has_transparent_background;

  DCHECK(quad_list.empty());
  DCHECK(shared_quad_state_list.empty());
}

void RenderPass::AsValueInto(base::trace_event::TracedValue* value) const {
  MathUtil::AddToTracedValue("output_rect", output_rect, value);
  MathUtil::AddToTracedValue("damage_rect", damage_rect, value);

  value->SetBoolean("has_transparent_background", has_transparent_background);
  value->SetInteger("copy_requests",
                    base::saturated_cast<int>(copy_requests.size()));

  value->BeginArray("filters");
  filters.AsValueInto(value);
  value->EndArray();

  value->BeginArray("background_filters");
  background_filters.AsValueInto(value);
  value->EndArray();

  value->BeginArray("shared_quad_state_list");
  for (auto* shared_quad_state : shared_quad_state_list) {
    value->BeginDictionary();
    shared_quad_state->AsValueInto(value);
    value->EndDictionary();
  }
  value->EndArray();

  value->BeginArray("quad_list");
  for (auto* quad : quad_list) {
    value->BeginDictionary();
    quad->AsValueInto(value);
    value->EndDictionary();
  }
  value->EndArray();

  TracedValue::MakeDictIntoImplicitSnapshotWithCategory(
      TRACE_DISABLED_BY_DEFAULT("cc.debug.quads"), value, "cc::RenderPass",
      reinterpret_cast<void*>(id));
}

SharedQuadState* RenderPass::CreateAndAppendSharedQuadState() {
  return shared_quad_state_list.AllocateAndConstruct<SharedQuadState>();
}

RenderPassDrawQuad* RenderPass::CopyFromAndAppendRenderPassDrawQuad(
    const RenderPassDrawQuad* quad,
    const SharedQuadState* shared_quad_state,
    int render_pass_id) {
  RenderPassDrawQuad* copy_quad =
      CopyFromAndAppendTypedDrawQuad<RenderPassDrawQuad>(quad);
  copy_quad->shared_quad_state = shared_quad_state;
  copy_quad->render_pass_id = render_pass_id;
  return copy_quad;
}

DrawQuad* RenderPass::CopyFromAndAppendDrawQuad(
    const DrawQuad* quad,
    const SharedQuadState* shared_quad_state) {
  switch (quad->material) {
    case DrawQuad::DEBUG_BORDER:
      CopyFromAndAppendTypedDrawQuad<DebugBorderDrawQuad>(quad);
      break;
    case DrawQuad::PICTURE_CONTENT:
      CopyFromAndAppendTypedDrawQuad<PictureDrawQuad>(quad);
      break;
    case DrawQuad::TEXTURE_CONTENT:
      CopyFromAndAppendTypedDrawQuad<TextureDrawQuad>(quad);
      break;
    case DrawQuad::SOLID_COLOR:
      CopyFromAndAppendTypedDrawQuad<SolidColorDrawQuad>(quad);
      break;
    case DrawQuad::TILED_CONTENT:
      CopyFromAndAppendTypedDrawQuad<TileDrawQuad>(quad);
      break;
    case DrawQuad::STREAM_VIDEO_CONTENT:
      CopyFromAndAppendTypedDrawQuad<StreamVideoDrawQuad>(quad);
      break;
    case DrawQuad::SURFACE_CONTENT:
      CopyFromAndAppendTypedDrawQuad<SurfaceDrawQuad>(quad);
      break;
    case DrawQuad::YUV_VIDEO_CONTENT:
      CopyFromAndAppendTypedDrawQuad<YUVVideoDrawQuad>(quad);
      break;
    // RenderPass quads need to use specific CopyFrom function.
    case DrawQuad::RENDER_PASS:
    case DrawQuad::INVALID:
      LOG(FATAL) << "Invalid DrawQuad material " << quad->material;
      break;
  }
  quad_list.back()->shared_quad_state = shared_quad_state;
  return quad_list.back();
}

}  // namespace cc
