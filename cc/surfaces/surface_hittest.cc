// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/surfaces/surface_hittest.h"

#include "cc/output/compositor_frame.h"
#include "cc/output/delegated_frame_data.h"
#include "cc/quads/draw_quad.h"
#include "cc/quads/render_pass_draw_quad.h"
#include "cc/quads/surface_draw_quad.h"
#include "cc/surfaces/surface.h"
#include "cc/surfaces/surface_manager.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/transform.h"

namespace cc {
namespace {
const RenderPass* GetRootRenderPass(SurfaceManager* manager,
                                    SurfaceId surface_id) {
  Surface* surface = manager->GetSurfaceForId(surface_id);

  const CompositorFrame* surface_frame = surface->GetEligibleFrame();
  if (!surface_frame)
    return nullptr;

  const DelegatedFrameData* frame_data =
      surface_frame->delegated_frame_data.get();
  return frame_data->render_pass_list.empty()
             ? nullptr
             : frame_data->render_pass_list.back();
}
}

SurfaceHittest::SurfaceHittest(SurfaceManager* manager) : manager_(manager) {}

SurfaceHittest::~SurfaceHittest() {}

SurfaceId SurfaceHittest::Hittest(SurfaceId surface_id,
                                  const gfx::Point& point,
                                  gfx::Point* transformed_point) {
  SurfaceId hittest_surface_id = surface_id;

  if (transformed_point)
    *transformed_point = point;

  HittestInternal(surface_id, GetRootRenderPass(manager_, surface_id), point,
                  &hittest_surface_id, transformed_point);

  referenced_passes_.clear();

  return hittest_surface_id;
}

bool SurfaceHittest::HittestInternal(SurfaceId surface_id,
                                     const RenderPass* render_pass,
                                     const gfx::Point& point,
                                     SurfaceId* out_surface_id,
                                     gfx::Point* out_transformed_point) {
  // To avoid an infinite recursion, we need to skip the RenderPass if it's
  // already been referenced.
  if (referenced_passes_.find(render_pass) != referenced_passes_.end())
    return false;
  referenced_passes_.insert(render_pass);

  gfx::Transform transform_from_root_target;
  if (!render_pass ||
      !render_pass->transform_to_root_target.GetInverse(
          &transform_from_root_target)) {
    return false;
  }

  gfx::Point point_in_target_space(point);
  transform_from_root_target.TransformPoint(&point_in_target_space);

  for (const auto* quad : render_pass->quad_list) {
    // First we test against the clip_rect. The clip_rect is in target space, so
    // we can test the point directly.
    if (!quad->shared_quad_state->is_clipped ||
        quad->shared_quad_state->clip_rect.Contains(point_in_target_space)) {
      // We now transform the point to content space and test if it hits the
      // rect.
      gfx::Transform target_to_quad_transform;
      if (quad->shared_quad_state->quad_to_target_transform.GetInverse(
              &target_to_quad_transform)) {
        gfx::Point transformed_point(point_in_target_space);
        target_to_quad_transform.TransformPoint(&transformed_point);

        if (quad->rect.Contains(transformed_point)) {
          if (quad->material == DrawQuad::SURFACE_CONTENT) {
            // We've hit a SurfaceDrawQuad, we need to recurse into this
            // Surface.
            const SurfaceDrawQuad* surface_quad =
                SurfaceDrawQuad::MaterialCast(quad);

            gfx::Point point_in_current_surface;
            if (out_transformed_point) {
              point_in_current_surface = *out_transformed_point;
              *out_transformed_point = transformed_point;
            }

            if (HittestInternal(
                    surface_quad->surface_id,
                    GetRootRenderPass(manager_, surface_quad->surface_id),
                    transformed_point, out_surface_id, out_transformed_point)) {
              return true;
            } else {
              if (out_transformed_point)
                *out_transformed_point = point_in_current_surface;
            }
          } else if (quad->material == DrawQuad::RENDER_PASS) {
            // We've hit a RenderPassDrawQuad, we need to recurse into this
            // RenderPass.
            const RenderPassDrawQuad* render_quad =
                RenderPassDrawQuad::MaterialCast(quad);

            Surface* surface = manager_->GetSurfaceForId(surface_id);
            const CompositorFrame* surface_frame = surface->GetEligibleFrame();
            DCHECK(surface_frame);
            const DelegatedFrameData* frame_data =
                surface_frame->delegated_frame_data.get();

            const RenderPass* quad_render_pass = nullptr;
            for (const auto* render_pass : frame_data->render_pass_list) {
              if (render_pass->id == render_quad->render_pass_id) {
                quad_render_pass = render_pass;
                break;
              }
            }

            if (quad_render_pass &&
                HittestInternal(surface_id, quad_render_pass,
                                point_in_target_space, out_surface_id,
                                out_transformed_point)) {
              return true;
            }
          } else {
            // We've hit a different type of quad in the current Surface,
            // there's no need to iterate anymore, this is the quad that
            // receives the event;
            *out_surface_id = surface_id;
            return true;
          }
        }
      }
    }
  }

  return false;
}
}  // namespace cc
