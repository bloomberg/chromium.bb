// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/compositor_frame_fuzzer/compositor_frame_fuzzer_util.h"

#include <memory>
#include <utility>

#include "components/viz/common/quads/solid_color_draw_quad.h"
#include "components/viz/service/compositor_frame_fuzzer/fuzzer_browser_process.h"

namespace viz {

namespace {

gfx::Rect GetRectFromProtobuf(const content::fuzzing::proto::Rect& proto_rect) {
  return gfx::Rect(proto_rect.x(), proto_rect.y(), proto_rect.width(),
                   proto_rect.height());
}

// Mutates a gfx::Rect to ensure width and height are both at least min_size.
// Use in case e.g. a 0-width/height Rect would cause a validation error on
// deserialization.
void ExpandToMinSize(gfx::Rect* rect, int min_size) {
  if (rect->width() < min_size) {
    // grow width to min_size in +x direction
    // (may be clamped if x + min_size overflows)
    rect->set_width(min_size);
  }

  // if previous attempt failed due to overflow
  if (rect->width() < min_size) {
    // grow width to min_size in -x direction
    rect->Offset(-(min_size - rect->width()), 0);
    rect->set_width(min_size);
  }

  if (rect->height() < min_size) {
    // grow height to min_size in +y direction
    // (may be clamped if y + min_size overflows)
    rect->set_height(min_size);
  }

  // if previous attempt failed due to overflow
  if (rect->height() < min_size) {
    // grow height to min_size in -y direction
    rect->Offset(0, -(min_size - rect->height()));
    rect->set_height(min_size);
  }
}

}  // namespace

CompositorFrame GenerateFuzzedCompositorFrame(
    const content::fuzzing::proto::RenderPass& render_pass_spec) {
  CompositorFrame frame;

  frame.metadata.begin_frame_ack.source_id = BeginFrameArgs::kManualSourceId;
  frame.metadata.begin_frame_ack.sequence_number =
      BeginFrameArgs::kStartingFrameNumber;
  frame.metadata.begin_frame_ack.has_damage = true;
  frame.metadata.frame_token = 1;
  frame.metadata.device_scale_factor = 1;
  frame.metadata.local_surface_id_allocation_time = base::TimeTicks::Now();

  std::unique_ptr<RenderPass> pass = RenderPass::Create();
  gfx::Rect rp_output_rect =
      GetRectFromProtobuf(render_pass_spec.output_rect());
  ExpandToMinSize(&rp_output_rect, 1);
  pass->SetNew(1, rp_output_rect,
               GetRectFromProtobuf(render_pass_spec.damage_rect()),
               gfx::Transform());

  content::fuzzing::proto::SolidColorDrawQuad quad_spec =
      render_pass_spec.draw_quad();
  gfx::Rect quad_rect = GetRectFromProtobuf(quad_spec.rect());
  gfx::Rect quad_visible_rect = GetRectFromProtobuf(quad_spec.visible_rect());
  auto* shared_quad_state = pass->CreateAndAppendSharedQuadState();
  shared_quad_state->SetAll(gfx::Transform(), gfx::Rect(quad_rect),
                            gfx::Rect(quad_rect), gfx::Rect(quad_rect),
                            /*is_clipped=*/false,
                            /*are_contents_opaque=*/false, 1,
                            SkBlendMode::kSrcOver, /*sorting_context_id=*/0);
  auto* color_quad = pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  color_quad->SetNew(shared_quad_state, quad_visible_rect, gfx::Rect(quad_rect),
                     quad_spec.color(), quad_spec.force_anti_aliasing_off());

  frame.render_pass_list.push_back(std::move(pass));
  return frame;
}

}  // namespace viz
