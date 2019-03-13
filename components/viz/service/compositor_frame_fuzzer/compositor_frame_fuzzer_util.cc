// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/compositor_frame_fuzzer/compositor_frame_fuzzer_util.h"

#include <limits>
#include <memory>
#include <utility>

#include "components/viz/common/quads/solid_color_draw_quad.h"
#include "components/viz/service/compositor_frame_fuzzer/fuzzer_browser_process.h"

namespace viz {

namespace {

// Handles inf / NaN by setting to 0.
double MakeNormal(double x) {
  return isnormal(x) ? x : 0;
}
float MakeNormal(float x) {
  return isnormal(x) ? x : 0;
}

// Normalizes value to a float in [0, 1]. Use to convert a fuzzed
// uint32 into a percentage.
float Normalize(uint32_t x) {
  return static_cast<float>(x) / std::numeric_limits<uint32_t>::max();
}

gfx::Rect GetRectFromProtobuf(const content::fuzzing::proto::Rect& proto_rect) {
  return gfx::Rect(proto_rect.x(), proto_rect.y(), proto_rect.width(),
                   proto_rect.height());
}

gfx::Transform GetTransformFromProtobuf(
    const content::fuzzing::proto::Transform& proto_transform) {
  gfx::Transform transform = gfx::Transform();

  // Note: There are no checks here that disallow a non-invertible transform
  // (for instance, if |scale_x| or |scale_y| is 0).
  transform.Scale(MakeNormal(proto_transform.scale_x()),
                  MakeNormal(proto_transform.scale_y()));

  transform.Rotate(MakeNormal(proto_transform.rotate()));

  transform.Translate(MakeNormal(proto_transform.translate_x()),
                      MakeNormal(proto_transform.translate_y()));

  return transform;
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
  gfx::Rect rp_damage_rect =
      GetRectFromProtobuf(render_pass_spec.damage_rect());

  // Handle constraints on RenderPass:
  // Ensure that |rp_output_rect| has non-zero area and that |rp_damage_rect| is
  // contained in |rp_output_rect|.
  ExpandToMinSize(&rp_output_rect, 1);
  rp_damage_rect.AdjustToFit(rp_output_rect);

  pass->SetNew(1, rp_output_rect, rp_damage_rect, gfx::Transform());

  for (const auto& quad_spec : render_pass_spec.quad_list()) {
    gfx::Rect quad_rect = GetRectFromProtobuf(quad_spec.rect());
    gfx::Rect quad_visible_rect = GetRectFromProtobuf(quad_spec.visible_rect());

    // Handle constraints on DrawQuad:
    // Ensure that |quad_rect| has non-zero area and that |quad_visible_rect| is
    // contained in |quad_rect|.
    ExpandToMinSize(&quad_rect, 1);
    quad_visible_rect.AdjustToFit(quad_rect);

    auto* shared_quad_state = pass->CreateAndAppendSharedQuadState();
    shared_quad_state->SetAll(
        GetTransformFromProtobuf(quad_spec.sqs().transform()),
        GetRectFromProtobuf(quad_spec.sqs().layer_rect()),
        GetRectFromProtobuf(quad_spec.sqs().visible_rect()), gfx::RRectF(),
        GetRectFromProtobuf(quad_spec.sqs().clip_rect()),
        quad_spec.sqs().is_clipped(), quad_spec.sqs().are_contents_opaque(),
        Normalize(quad_spec.sqs().opacity()), SkBlendMode::kSrcOver,
        quad_spec.sqs().sorting_context_id());
    auto* color_quad = pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
    color_quad->SetNew(shared_quad_state, quad_rect, quad_visible_rect,
                       quad_spec.quad().color(),
                       quad_spec.quad().force_anti_aliasing_off());
  }

  frame.render_pass_list.push_back(std::move(pass));
  return frame;
}

}  // namespace viz
