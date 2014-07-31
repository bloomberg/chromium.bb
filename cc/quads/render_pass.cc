// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/quads/render_pass.h"

#include "base/debug/trace_event_argument.h"
#include "base/values.h"
#include "cc/base/math_util.h"
#include "cc/debug/traced_value.h"
#include "cc/output/copy_output_request.h"
#include "cc/quads/checkerboard_draw_quad.h"
#include "cc/quads/debug_border_draw_quad.h"
#include "cc/quads/draw_quad.h"
#include "cc/quads/io_surface_draw_quad.h"
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

void* RenderPass::Id::AsTracingId() const {
  COMPILE_ASSERT(sizeof(size_t) <= sizeof(void*),  // NOLINT
                 size_t_bigger_than_pointer);
  return reinterpret_cast<void*>(base::HashPair(layer_id, index));
}

scoped_ptr<RenderPass> RenderPass::Create() {
  return make_scoped_ptr(new RenderPass());
}

scoped_ptr<RenderPass> RenderPass::Create(size_t num_layers) {
  return make_scoped_ptr(new RenderPass(num_layers));
}

RenderPass::RenderPass() : id(Id(-1, -1)), has_transparent_background(true) {
  shared_quad_state_list.reserve(kDefaultNumSharedQuadStatesToReserve);
  quad_list.reserve(kDefaultNumQuadsToReserve);
}

RenderPass::RenderPass(size_t num_layers)
    : id(Id(-1, -1)), has_transparent_background(true) {
  // Each layer usually produces one shared quad state, so the number of layers
  // is a good hint for what to reserve here.
  shared_quad_state_list.reserve(num_layers);
  quad_list.reserve(kDefaultNumQuadsToReserve);
}

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
                    has_transparent_background);
  return copy_pass.Pass();
}

// static
void RenderPass::CopyAll(const ScopedPtrVector<RenderPass>& in,
                         ScopedPtrVector<RenderPass>* out) {
  for (size_t i = 0; i < in.size(); ++i) {
    RenderPass* source = in[i];

    // Since we can't copy these, it's wrong to use CopyAll in a situation where
    // you may have copy_requests present.
    DCHECK_EQ(source->copy_requests.size(), 0u);

    scoped_ptr<RenderPass> copy_pass(Create());
    copy_pass->SetAll(source->id,
                      source->output_rect,
                      source->damage_rect,
                      source->transform_to_root_target,
                      source->has_transparent_background);
    for (size_t i = 0; i < source->shared_quad_state_list.size(); ++i) {
      SharedQuadState* copy_shared_quad_state =
          copy_pass->CreateAndAppendSharedQuadState();
      copy_shared_quad_state->CopyFrom(source->shared_quad_state_list[i]);
    }
    for (size_t i = 0, sqs_i = 0; i < source->quad_list.size(); ++i) {
      while (source->quad_list[i]->shared_quad_state !=
             source->shared_quad_state_list[sqs_i]) {
        ++sqs_i;
        DCHECK_LT(sqs_i, source->shared_quad_state_list.size());
      }
      DCHECK(source->quad_list[i]->shared_quad_state ==
             source->shared_quad_state_list[sqs_i]);

      DrawQuad* quad = source->quad_list[i];

      if (quad->material == DrawQuad::RENDER_PASS) {
        const RenderPassDrawQuad* pass_quad =
            RenderPassDrawQuad::MaterialCast(quad);
        copy_pass->CopyFromAndAppendRenderPassDrawQuad(
            pass_quad,
            copy_pass->shared_quad_state_list[sqs_i],
            pass_quad->render_pass_id);
      } else {
        copy_pass->CopyFromAndAppendDrawQuad(
            quad, copy_pass->shared_quad_state_list[sqs_i]);
      }
    }
    out->push_back(copy_pass.Pass());
  }
}

void RenderPass::SetNew(Id id,
                        const gfx::Rect& output_rect,
                        const gfx::Rect& damage_rect,
                        const gfx::Transform& transform_to_root_target) {
  DCHECK_GT(id.layer_id, 0);
  DCHECK_GE(id.index, 0);
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

void RenderPass::SetAll(Id id,
                        const gfx::Rect& output_rect,
                        const gfx::Rect& damage_rect,
                        const gfx::Transform& transform_to_root_target,
                        bool has_transparent_background) {
  DCHECK_GT(id.layer_id, 0);
  DCHECK_GE(id.index, 0);

  this->id = id;
  this->output_rect = output_rect;
  this->damage_rect = damage_rect;
  this->transform_to_root_target = transform_to_root_target;
  this->has_transparent_background = has_transparent_background;

  DCHECK(quad_list.empty());
  DCHECK(shared_quad_state_list.empty());
}

void RenderPass::AsValueInto(base::debug::TracedValue* value) const {
  value->BeginArray("output_rect");
  MathUtil::AddToTracedValue(output_rect, value);
  value->EndArray();

  value->BeginArray("damage_rect");
  MathUtil::AddToTracedValue(damage_rect, value);
  value->EndArray();

  value->SetBoolean("has_transparent_background", has_transparent_background);
  value->SetInteger("copy_requests", copy_requests.size());

  value->BeginArray("shared_quad_state_list");
  for (size_t i = 0; i < shared_quad_state_list.size(); ++i) {
    value->BeginDictionary();
    shared_quad_state_list[i]->AsValueInto(value);
    value->EndDictionary();
  }
  value->EndArray();

  value->BeginArray("quad_list");
  for (size_t i = 0; i < quad_list.size(); ++i) {
    value->BeginDictionary();
    quad_list[i]->AsValueInto(value);
    value->EndDictionary();
  }
  value->EndArray();

  TracedValue::MakeDictIntoImplicitSnapshotWithCategory(
      TRACE_DISABLED_BY_DEFAULT("cc.debug.quads"),
      value,
      "cc::RenderPass",
      id.AsTracingId());
}

SharedQuadState* RenderPass::CreateAndAppendSharedQuadState() {
  shared_quad_state_list.push_back(make_scoped_ptr(new SharedQuadState));
  return shared_quad_state_list.back();
}

RenderPassDrawQuad* RenderPass::CopyFromAndAppendRenderPassDrawQuad(
    const RenderPassDrawQuad* quad,
    const SharedQuadState* shared_quad_state,
    RenderPass::Id render_pass_id) {
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
    case DrawQuad::CHECKERBOARD:
      CopyFromAndAppendTypedDrawQuad<CheckerboardDrawQuad>(quad);
      break;
    case DrawQuad::DEBUG_BORDER:
      CopyFromAndAppendTypedDrawQuad<DebugBorderDrawQuad>(quad);
      break;
    case DrawQuad::IO_SURFACE_CONTENT:
      CopyFromAndAppendTypedDrawQuad<IOSurfaceDrawQuad>(quad);
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
