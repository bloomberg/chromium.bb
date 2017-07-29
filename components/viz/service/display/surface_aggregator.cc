// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display/surface_aggregator.h"

#include <stddef.h>

#include <map>

#include "base/bind.h"
#include "base/containers/adapters.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/stl_util.h"
#include "base/trace_event/trace_event.h"
#include "cc/base/math_util.h"
#include "cc/output/compositor_frame.h"
#include "cc/quads/draw_quad.h"
#include "cc/quads/render_pass_draw_quad.h"
#include "cc/quads/shared_quad_state.h"
#include "cc/quads/solid_color_draw_quad.h"
#include "cc/quads/surface_draw_quad.h"
#include "cc/quads/texture_draw_quad.h"
#include "cc/resources/resource_provider.h"
#include "components/viz/service/frame_sinks/compositor_frame_sink_support.h"
#include "components/viz/service/surfaces/surface.h"
#include "components/viz/service/surfaces/surface_client.h"
#include "components/viz/service/surfaces/surface_manager.h"

namespace viz {
namespace {

// Maximum bucket size for the UMA stats.
constexpr int kUmaStatMaxSurfaces = 30;

const char kUmaValidSurface[] =
    "Compositing.SurfaceAggregator.SurfaceDrawQuad.ValidSurface";
const char kUmaMissingSurface[] =
    "Compositing.SurfaceAggregator.SurfaceDrawQuad.MissingSurface";
const char kUmaNoActiveFrame[] =
    "Compositing.SurfaceAggregator.SurfaceDrawQuad.NoActiveFrame";

void MoveMatchingRequests(
    cc::RenderPassId render_pass_id,
    std::multimap<cc::RenderPassId, std::unique_ptr<CopyOutputRequest>>*
        copy_requests,
    std::vector<std::unique_ptr<CopyOutputRequest>>* output_requests) {
  auto request_range = copy_requests->equal_range(render_pass_id);
  for (auto it = request_range.first; it != request_range.second; ++it) {
    DCHECK(it->second);
    output_requests->push_back(std::move(it->second));
  }
  copy_requests->erase(request_range.first, request_range.second);
}

// Returns true if the damage rect is valid.
bool CalculateQuadSpaceDamageRect(
    const gfx::Transform& quad_to_target_transform,
    const gfx::Transform& target_to_root_transform,
    const gfx::Rect& root_damage_rect,
    gfx::Rect* quad_space_damage_rect) {
  gfx::Transform quad_to_root_transform(target_to_root_transform,
                                        quad_to_target_transform);
  gfx::Transform inverse_transform(gfx::Transform::kSkipInitialization);
  bool inverse_valid = quad_to_root_transform.GetInverse(&inverse_transform);
  if (!inverse_valid)
    return false;

  *quad_space_damage_rect = cc::MathUtil::ProjectEnclosingClippedRect(
      inverse_transform, root_damage_rect);
  return true;
}

}  // namespace

SurfaceAggregator::SurfaceAggregator(SurfaceManager* manager,
                                     cc::ResourceProvider* provider,
                                     bool aggregate_only_damaged)
    : manager_(manager),
      provider_(provider),
      next_render_pass_id_(1),
      aggregate_only_damaged_(aggregate_only_damaged),
      weak_factory_(this) {
  DCHECK(manager_);
}

SurfaceAggregator::~SurfaceAggregator() {
  // Notify client of all surfaces being removed.
  contained_surfaces_.clear();
  ProcessAddedAndRemovedSurfaces();
}

SurfaceAggregator::PrewalkResult::PrewalkResult() {}

SurfaceAggregator::PrewalkResult::~PrewalkResult() {}

// Create a clip rect for an aggregated quad from the original clip rect and
// the clip rect from the surface it's on.
SurfaceAggregator::ClipData SurfaceAggregator::CalculateClipRect(
    const ClipData& surface_clip,
    const ClipData& quad_clip,
    const gfx::Transform& target_transform) {
  ClipData out_clip;
  if (surface_clip.is_clipped)
    out_clip = surface_clip;

  if (quad_clip.is_clipped) {
    // TODO(jamesr): This only works if target_transform maps integer
    // rects to integer rects.
    gfx::Rect final_clip =
        cc::MathUtil::MapEnclosingClippedRect(target_transform, quad_clip.rect);
    if (out_clip.is_clipped)
      out_clip.rect.Intersect(final_clip);
    else
      out_clip.rect = final_clip;
    out_clip.is_clipped = true;
  }

  return out_clip;
}

cc::RenderPassId SurfaceAggregator::RemapPassId(
    cc::RenderPassId surface_local_pass_id,
    const SurfaceId& surface_id) {
  auto key = std::make_pair(surface_id, surface_local_pass_id);
  auto it = render_pass_allocator_map_.find(key);
  if (it != render_pass_allocator_map_.end()) {
    it->second.in_use = true;
    return it->second.id;
  }

  RenderPassInfo render_pass_info;
  render_pass_info.id = next_render_pass_id_++;
  render_pass_allocator_map_[key] = render_pass_info;
  return render_pass_info.id;
}

int SurfaceAggregator::ChildIdForSurface(Surface* surface) {
  auto it = surface_id_to_resource_child_id_.find(surface->surface_id());
  if (it == surface_id_to_resource_child_id_.end()) {
    int child_id = provider_->CreateChild(
        base::Bind(&SurfaceAggregator::UnrefResources, surface->client()));
    provider_->SetChildNeedsSyncTokens(child_id, surface->needs_sync_tokens());
    surface_id_to_resource_child_id_[surface->surface_id()] = child_id;
    return child_id;
  } else {
    return it->second;
  }
}

gfx::Rect SurfaceAggregator::DamageRectForSurface(
    const Surface* surface,
    const cc::RenderPass& source,
    const gfx::Rect& full_rect) const {
  auto it = previous_contained_surfaces_.find(surface->surface_id());
  if (it != previous_contained_surfaces_.end()) {
    uint64_t previous_index = it->second;
    if (previous_index == surface->GetActiveFrameIndex())
      return gfx::Rect();
  }
  const SurfaceId& previous_surface_id = surface->previous_frame_surface_id();

  if (surface->surface_id() != previous_surface_id) {
    it = previous_contained_surfaces_.find(previous_surface_id);
  }
  if (it != previous_contained_surfaces_.end()) {
    uint64_t previous_index = it->second;
    if (previous_index == surface->GetActiveFrameIndex() - 1)
      return source.damage_rect;
  }

  return full_rect;
}

// static
void SurfaceAggregator::UnrefResources(
    base::WeakPtr<SurfaceClient> surface_client,
    const std::vector<ReturnedResource>& resources,
    cc::BlockingTaskRunner* main_thread_task_runner) {
  if (surface_client)
    surface_client->UnrefResources(resources);
}

void SurfaceAggregator::HandleSurfaceQuad(
    const cc::SurfaceDrawQuad* surface_quad,
    const gfx::Transform& target_transform,
    const ClipData& clip_rect,
    cc::RenderPass* dest_pass,
    bool ignore_undamaged,
    gfx::Rect* damage_rect_in_quad_space,
    bool* damage_rect_in_quad_space_valid) {
  SurfaceId surface_id = surface_quad->surface_id;
  // If this surface's id is already in our referenced set then it creates
  // a cycle in the graph and should be dropped.
  if (referenced_surfaces_.count(surface_id))
    return;
  Surface* surface = manager_->GetSurfaceForId(surface_id);
  if (!surface || !surface->HasActiveFrame()) {
    if (surface_quad->fallback_quad) {
      HandleSurfaceQuad(surface_quad->fallback_quad, target_transform,
                        clip_rect, dest_pass, ignore_undamaged,
                        damage_rect_in_quad_space,
                        damage_rect_in_quad_space_valid);
    } else if (!surface) {
      ++uma_stats_.missing_surface;
    } else {
      ++uma_stats_.no_active_frame;
    }
    return;
  }
  ++uma_stats_.valid_surface;

  if (ignore_undamaged) {
    gfx::Transform quad_to_target_transform(
        target_transform,
        surface_quad->shared_quad_state->quad_to_target_transform);
    *damage_rect_in_quad_space_valid = CalculateQuadSpaceDamageRect(
        quad_to_target_transform, dest_pass->transform_to_root_target,
        root_damage_rect_, damage_rect_in_quad_space);
    if (*damage_rect_in_quad_space_valid &&
        !damage_rect_in_quad_space->Intersects(surface_quad->visible_rect)) {
      return;
    }
  }

  const cc::CompositorFrame& frame = surface->GetActiveFrame();

  // A map keyed by cc::RenderPass id.
  Surface::CopyRequestsMap copy_requests;
  surface->TakeCopyOutputRequests(&copy_requests);

  const cc::RenderPassList& render_pass_list = frame.render_pass_list;
  if (!valid_surfaces_.count(surface->surface_id())) {
    for (auto& request : copy_requests)
      request.second->SendEmptyResult();
    return;
  }

  referenced_surfaces_.insert(surface_id);
  // TODO(vmpstr): provider check is a hack for unittests that don't set up a
  // resource provider.
  cc::ResourceProvider::ResourceIdMap empty_map;
  const auto& child_to_parent_map =
      provider_ ? provider_->GetChildToParentMap(ChildIdForSurface(surface))
                : empty_map;
  bool merge_pass =
      surface_quad->shared_quad_state->opacity == 1.f && copy_requests.empty();

  const cc::RenderPassList& referenced_passes = render_pass_list;
  size_t passes_to_copy =
      merge_pass ? referenced_passes.size() - 1 : referenced_passes.size();
  for (size_t j = 0; j < passes_to_copy; ++j) {
    const cc::RenderPass& source = *referenced_passes[j];

    size_t sqs_size = source.shared_quad_state_list.size();
    size_t dq_size = source.quad_list.size();
    std::unique_ptr<cc::RenderPass> copy_pass(
        cc::RenderPass::Create(sqs_size, dq_size));

    cc::RenderPassId remapped_pass_id = RemapPassId(source.id, surface_id);

    copy_pass->SetAll(remapped_pass_id, source.output_rect, source.output_rect,
                      source.transform_to_root_target, source.filters,
                      source.background_filters, blending_color_space_,
                      source.has_transparent_background,
                      source.cache_render_pass,
                      source.has_damage_from_contributing_content);

    MoveMatchingRequests(source.id, &copy_requests, &copy_pass->copy_requests);

    // Contributing passes aggregated in to the pass list need to take the
    // transform of the surface quad into account to update their transform to
    // the root surface.
    copy_pass->transform_to_root_target.ConcatTransform(
        surface_quad->shared_quad_state->quad_to_target_transform);
    copy_pass->transform_to_root_target.ConcatTransform(target_transform);
    copy_pass->transform_to_root_target.ConcatTransform(
        dest_pass->transform_to_root_target);

    CopyQuadsToPass(source.quad_list, source.shared_quad_state_list,
                    child_to_parent_map, gfx::Transform(), ClipData(),
                    copy_pass.get(), surface_id);

    // If the render pass has copy requests, or should be cached, or has
    // moving-pixel filters, or in a moving-pixel surface, we should damage the
    // whole output rect so that we always drawn the full content. Otherwise, we
    // might have incompleted copy request, or cached patially drawn render
    // pass.
    if (!copy_request_passes_.count(remapped_pass_id) &&
        !copy_pass->cache_render_pass &&
        !moved_pixel_passes_.count(remapped_pass_id)) {
      gfx::Transform inverse_transform(gfx::Transform::kSkipInitialization);
      if (copy_pass->transform_to_root_target.GetInverse(&inverse_transform)) {
        gfx::Rect damage_rect_in_render_pass_space =
            cc::MathUtil::ProjectEnclosingClippedRect(inverse_transform,
                                                      root_damage_rect_);
        copy_pass->damage_rect.Intersect(damage_rect_in_render_pass_space);
      }
    }

    if (copy_pass->has_damage_from_contributing_content)
      contributing_content_damaged_passes_.insert(copy_pass->id);
    dest_pass_list_->push_back(std::move(copy_pass));
  }

  gfx::Transform surface_transform =
      surface_quad->shared_quad_state->quad_to_target_transform;
  surface_transform.ConcatTransform(target_transform);

  const auto& last_pass = *render_pass_list.back();
  // This will check if all the surface_quads (including child surfaces) has
  // damage because HandleSurfaceQuad is a recursive call by calling
  // CopyQuadsToPass in it.
  dest_pass->has_damage_from_contributing_content |=
      !DamageRectForSurface(surface, last_pass, last_pass.output_rect)
           .IsEmpty();

  if (merge_pass) {
    // TODO(jamesr): Clean up last pass special casing.
    const cc::QuadList& quads = last_pass.quad_list;

    // Intersect the transformed visible rect and the clip rect to create a
    // smaller cliprect for the quad.
    ClipData surface_quad_clip_rect(
        true, cc::MathUtil::MapEnclosingClippedRect(
                  surface_quad->shared_quad_state->quad_to_target_transform,
                  surface_quad->visible_rect));
    if (surface_quad->shared_quad_state->is_clipped) {
      surface_quad_clip_rect.rect.Intersect(
          surface_quad->shared_quad_state->clip_rect);
    }

    ClipData quads_clip =
        CalculateClipRect(clip_rect, surface_quad_clip_rect, target_transform);

    CopyQuadsToPass(quads, last_pass.shared_quad_state_list,
                    child_to_parent_map, surface_transform, quads_clip,
                    dest_pass, surface_id);
  } else {
    cc::RenderPassId remapped_pass_id = RemapPassId(last_pass.id, surface_id);

    auto* shared_quad_state =
        CopySharedQuadState(surface_quad->shared_quad_state, target_transform,
                            clip_rect, dest_pass);

    auto* quad = dest_pass->CreateAndAppendDrawQuad<cc::RenderPassDrawQuad>();
    quad->SetNew(shared_quad_state, surface_quad->rect,
                 surface_quad->visible_rect, remapped_pass_id, 0, gfx::RectF(),
                 gfx::Size(), gfx::Vector2dF(), gfx::PointF(),
                 gfx::RectF(surface_quad->rect));
  }

  // Need to re-query since referenced_surfaces_ iterators are not stable.
  referenced_surfaces_.erase(referenced_surfaces_.find(surface_id));
}

void SurfaceAggregator::AddColorConversionPass() {
  if (dest_pass_list_->empty())
    return;

  auto* root_render_pass = dest_pass_list_->back().get();
  if (root_render_pass->color_space == output_color_space_)
    return;

  gfx::Rect output_rect = root_render_pass->output_rect;
  CHECK(root_render_pass->transform_to_root_target == gfx::Transform());

  if (!color_conversion_render_pass_id_)
    color_conversion_render_pass_id_ = next_render_pass_id_++;

  auto color_conversion_pass = cc::RenderPass::Create(1, 1);
  color_conversion_pass->SetNew(color_conversion_render_pass_id_, output_rect,
                                root_render_pass->damage_rect,
                                root_render_pass->transform_to_root_target);
  color_conversion_pass->color_space = output_color_space_;

  auto* shared_quad_state =
      color_conversion_pass->CreateAndAppendSharedQuadState();
  shared_quad_state->quad_layer_rect = output_rect;
  shared_quad_state->visible_quad_layer_rect = output_rect;
  shared_quad_state->opacity = 1.f;

  auto* quad =
      color_conversion_pass->CreateAndAppendDrawQuad<cc::RenderPassDrawQuad>();
  quad->SetNew(shared_quad_state, output_rect, output_rect,
               root_render_pass->id, 0, gfx::RectF(), gfx::Size(),
               gfx::Vector2dF(), gfx::PointF(), gfx::RectF(output_rect));
  dest_pass_list_->push_back(std::move(color_conversion_pass));
}

cc::SharedQuadState* SurfaceAggregator::CopySharedQuadState(
    const cc::SharedQuadState* source_sqs,
    const gfx::Transform& target_transform,
    const ClipData& clip_rect,
    cc::RenderPass* dest_render_pass) {
  auto* copy_shared_quad_state =
      dest_render_pass->CreateAndAppendSharedQuadState();
  *copy_shared_quad_state = *source_sqs;
  // target_transform contains any transformation that may exist
  // between the context that these quads are being copied from (i.e. the
  // surface's draw transform when aggregated from within a surface) to the
  // target space of the pass. This will be identity except when copying the
  // root draw pass from a surface into a pass when the surface draw quad's
  // transform is not identity.
  copy_shared_quad_state->quad_to_target_transform.ConcatTransform(
      target_transform);

  ClipData new_clip_rect = CalculateClipRect(
      clip_rect, ClipData(source_sqs->is_clipped, source_sqs->clip_rect),
      target_transform);
  copy_shared_quad_state->is_clipped = new_clip_rect.is_clipped;
  copy_shared_quad_state->clip_rect = new_clip_rect.rect;
  return copy_shared_quad_state;
}

void SurfaceAggregator::CopyQuadsToPass(
    const cc::QuadList& source_quad_list,
    const cc::SharedQuadStateList& source_shared_quad_state_list,
    const std::unordered_map<ResourceId, ResourceId>& child_to_parent_map,
    const gfx::Transform& target_transform,
    const ClipData& clip_rect,
    cc::RenderPass* dest_pass,
    const SurfaceId& surface_id) {
  const cc::SharedQuadState* last_copied_source_shared_quad_state = nullptr;
  const cc::SharedQuadState* dest_shared_quad_state = nullptr;
  // If the current frame has copy requests or cached render passes, then
  // aggregate the entire thing, as otherwise parts of the copy requests may be
  // ignored and we could cache partially drawn render pass.
  const bool ignore_undamaged =
      aggregate_only_damaged_ && !has_copy_requests_ &&
      !has_cached_render_passes_ && !moved_pixel_passes_.count(dest_pass->id);
  // Damage rect in the quad space of the current shared quad state.
  // TODO(jbauman): This rect may contain unnecessary area if
  // transform isn't axis-aligned.
  gfx::Rect damage_rect_in_quad_space;
  bool damage_rect_in_quad_space_valid = false;

#if DCHECK_IS_ON()
  // If quads have come in with SharedQuadState out of order, or when quads have
  // invalid SharedQuadState pointer, it should DCHECK.
  auto sqs_iter = source_shared_quad_state_list.cbegin();
  for (auto* quad : source_quad_list) {
    while (sqs_iter != source_shared_quad_state_list.cend() &&
           quad->shared_quad_state != *sqs_iter) {
      ++sqs_iter;
    }
    DCHECK(sqs_iter != source_shared_quad_state_list.cend());
  }
#endif

  for (auto* quad : source_quad_list) {
    if (quad->material == cc::DrawQuad::SURFACE_CONTENT) {
      const auto* surface_quad = cc::SurfaceDrawQuad::MaterialCast(quad);
      // HandleSurfaceQuad may add other shared quad state, so reset the
      // current data.
      last_copied_source_shared_quad_state = nullptr;

      // The primary SurfaceDrawQuad should have already dealt with the fallback
      // cc::DrawQuad.
      if (surface_quad->surface_draw_quad_type ==
          cc::SurfaceDrawQuadType::FALLBACK)
        continue;

      HandleSurfaceQuad(surface_quad, target_transform, clip_rect, dest_pass,
                        ignore_undamaged, &damage_rect_in_quad_space,
                        &damage_rect_in_quad_space_valid);
    } else {
      if (quad->shared_quad_state != last_copied_source_shared_quad_state) {
        dest_shared_quad_state = CopySharedQuadState(
            quad->shared_quad_state, target_transform, clip_rect, dest_pass);
        last_copied_source_shared_quad_state = quad->shared_quad_state;
        if (aggregate_only_damaged_ && !has_copy_requests_ &&
            !has_cached_render_passes_) {
          damage_rect_in_quad_space_valid = CalculateQuadSpaceDamageRect(
              dest_shared_quad_state->quad_to_target_transform,
              dest_pass->transform_to_root_target, root_damage_rect_,
              &damage_rect_in_quad_space);
        }
      }

      if (ignore_undamaged) {
        if (damage_rect_in_quad_space_valid &&
            !damage_rect_in_quad_space.Intersects(quad->visible_rect))
          continue;
      }

      cc::DrawQuad* dest_quad;
      if (quad->material == cc::DrawQuad::RENDER_PASS) {
        const auto* pass_quad = cc::RenderPassDrawQuad::MaterialCast(quad);
        cc::RenderPassId original_pass_id = pass_quad->render_pass_id;
        cc::RenderPassId remapped_pass_id =
            RemapPassId(original_pass_id, surface_id);

        // If the RenderPassDrawQuad is referring to other render pass with the
        // |has_damage_from_contributing_content| set on it, then the dest_pass
        // should have the flag set on it as well.
        if (contributing_content_damaged_passes_.count(remapped_pass_id))
          dest_pass->has_damage_from_contributing_content = true;

        dest_quad = dest_pass->CopyFromAndAppendRenderPassDrawQuad(
            pass_quad, dest_shared_quad_state, remapped_pass_id);
      } else if (quad->material == cc::DrawQuad::TEXTURE_CONTENT) {
        const auto* texture_quad = cc::TextureDrawQuad::MaterialCast(quad);
        if (texture_quad->secure_output_only &&
            (!output_is_secure_ || copy_request_passes_.count(dest_pass->id))) {
          auto* solid_color_quad =
              dest_pass->CreateAndAppendDrawQuad<cc::SolidColorDrawQuad>();
          solid_color_quad->SetNew(dest_shared_quad_state, quad->rect,
                                   quad->visible_rect, SK_ColorBLACK, false);
          dest_quad = solid_color_quad;
        } else {
          dest_quad = dest_pass->CopyFromAndAppendDrawQuad(
              quad, dest_shared_quad_state);
        }
      } else {
        dest_quad =
            dest_pass->CopyFromAndAppendDrawQuad(quad, dest_shared_quad_state);
      }
      if (!child_to_parent_map.empty()) {
        for (ResourceId& resource_id : dest_quad->resources) {
          auto it = child_to_parent_map.find(resource_id);
          DCHECK(it != child_to_parent_map.end());

          DCHECK_EQ(it->first, resource_id);
          ResourceId remapped_id = it->second;
          resource_id = remapped_id;
        }
      }
    }
  }
}

void SurfaceAggregator::CopyPasses(const cc::CompositorFrame& frame,
                                   Surface* surface) {
  // The root surface is allowed to have copy output requests, so grab them
  // off its render passes. This map contains a set of CopyOutputRequests
  // keyed by each cc::RenderPass id.
  Surface::CopyRequestsMap copy_requests;
  surface->TakeCopyOutputRequests(&copy_requests);

  const auto& source_pass_list = frame.render_pass_list;
  DCHECK(valid_surfaces_.count(surface->surface_id()));
  if (!valid_surfaces_.count(surface->surface_id()))
    return;

  // TODO(vmpstr): provider check is a hack for unittests that don't set up a
  // resource provider.
  cc::ResourceProvider::ResourceIdMap empty_map;
  const auto& child_to_parent_map =
      provider_ ? provider_->GetChildToParentMap(ChildIdForSurface(surface))
                : empty_map;
  for (size_t i = 0; i < source_pass_list.size(); ++i) {
    const auto& source = *source_pass_list[i];

    size_t sqs_size = source.shared_quad_state_list.size();
    size_t dq_size = source.quad_list.size();
    auto copy_pass = cc::RenderPass::Create(sqs_size, dq_size);

    MoveMatchingRequests(source.id, &copy_requests, &copy_pass->copy_requests);

    cc::RenderPassId remapped_pass_id =
        RemapPassId(source.id, surface->surface_id());

    copy_pass->SetAll(remapped_pass_id, source.output_rect, source.output_rect,
                      source.transform_to_root_target, source.filters,
                      source.background_filters, blending_color_space_,
                      source.has_transparent_background,
                      source.cache_render_pass,
                      source.has_damage_from_contributing_content);

    CopyQuadsToPass(source.quad_list, source.shared_quad_state_list,
                    child_to_parent_map, gfx::Transform(), ClipData(),
                    copy_pass.get(), surface->surface_id());

    // If the render pass has copy requests, or should be cached, or has
    // moving-pixel filters, or in a moving-pixel surface, we should damage the
    // whole output rect so that we always drawn the full content. Otherwise, we
    // might have incompleted copy request, or cached patially drawn render
    // pass.
    if (!copy_request_passes_.count(remapped_pass_id) &&
        !copy_pass->cache_render_pass &&
        !moved_pixel_passes_.count(remapped_pass_id)) {
      gfx::Transform inverse_transform(gfx::Transform::kSkipInitialization);
      if (copy_pass->transform_to_root_target.GetInverse(&inverse_transform)) {
        gfx::Rect damage_rect_in_render_pass_space =
            cc::MathUtil::ProjectEnclosingClippedRect(inverse_transform,
                                                      root_damage_rect_);
        copy_pass->damage_rect.Intersect(damage_rect_in_render_pass_space);
      }
    }

    if (copy_pass->has_damage_from_contributing_content)
      contributing_content_damaged_passes_.insert(copy_pass->id);
    dest_pass_list_->push_back(std::move(copy_pass));
  }
}

void SurfaceAggregator::ProcessAddedAndRemovedSurfaces() {
  for (const auto& surface : previous_contained_surfaces_) {
    if (!contained_surfaces_.count(surface.first)) {
      // Release resources of removed surface.
      auto it = surface_id_to_resource_child_id_.find(surface.first);
      if (it != surface_id_to_resource_child_id_.end()) {
        provider_->DestroyChild(it->second);
        surface_id_to_resource_child_id_.erase(it);
      }

      // Notify client of removed surface.
      Surface* surface_ptr = manager_->GetSurfaceForId(surface.first);
      if (surface_ptr) {
        surface_ptr->RunDrawCallback();
      }
    }
  }
}

// Walk the Surface tree from surface_id. Validate the resources of the current
// surface and its descendants, check if there are any copy requests, and
// return the combined damage rect.
gfx::Rect SurfaceAggregator::PrewalkTree(const SurfaceId& surface_id,
                                         bool in_moved_pixel_surface,
                                         int parent_pass_id,
                                         PrewalkResult* result) {
  // This is for debugging a possible use after free.
  // TODO(jbauman): Remove this once we have enough information.
  // http://crbug.com/560181
  base::WeakPtr<SurfaceAggregator> debug_weak_this = weak_factory_.GetWeakPtr();

  if (referenced_surfaces_.count(surface_id))
    return gfx::Rect();
  Surface* surface = manager_->GetSurfaceForId(surface_id);
  if (!surface) {
    contained_surfaces_[surface_id] = 0;
    return gfx::Rect();
  }
  contained_surfaces_[surface_id] = surface->GetActiveFrameIndex();
  if (!surface->HasActiveFrame())
    return gfx::Rect();
  const cc::CompositorFrame& frame = surface->GetActiveFrame();
  int child_id = 0;
  // TODO(jbauman): hack for unit tests that don't set up rp
  if (provider_) {
    child_id = ChildIdForSurface(surface);
    surface->RefResources(frame.resource_list);
    provider_->ReceiveFromChild(child_id, frame.resource_list);
  }
  CHECK(debug_weak_this.get());

  std::vector<ResourceId> referenced_resources;
  size_t reserve_size = frame.resource_list.size();
  referenced_resources.reserve(reserve_size);

  bool invalid_frame = false;
  cc::ResourceProvider::ResourceIdMap empty_map;
  const auto& child_to_parent_map =
      provider_ ? provider_->GetChildToParentMap(child_id) : empty_map;

  CHECK(debug_weak_this.get());
  cc::RenderPassId remapped_pass_id =
      RemapPassId(frame.render_pass_list.back()->id, surface_id);
  if (in_moved_pixel_surface)
    moved_pixel_passes_.insert(remapped_pass_id);
  if (parent_pass_id)
    render_pass_dependencies_[parent_pass_id].insert(remapped_pass_id);

  struct SurfaceInfo {
    SurfaceInfo(const SurfaceId& id,
                bool has_moved_pixels,
                cc::RenderPassId parent_pass_id,
                const gfx::Transform& target_to_surface_transform)
        : id(id),
          has_moved_pixels(has_moved_pixels),
          parent_pass_id(parent_pass_id),
          target_to_surface_transform(target_to_surface_transform) {}

    SurfaceId id;
    bool has_moved_pixels;
    cc::RenderPassId parent_pass_id;
    gfx::Transform target_to_surface_transform;
  };
  std::vector<SurfaceInfo> child_surfaces;

  gfx::Rect pixel_moving_background_filters_rect;
  // This data is created once and typically small or empty. Collect all items
  // and pass to a flat_vector to sort once.
  std::vector<cc::RenderPassId> pixel_moving_background_filter_passes_data;
  for (const auto& render_pass : frame.render_pass_list) {
    if (render_pass->background_filters.HasFilterThatMovesPixels()) {
      pixel_moving_background_filter_passes_data.push_back(
          RemapPassId(render_pass->id, surface_id));
      // TODO(wutao): Partial swap does not work with pixel moving background
      // filter. See https://crbug.com/737255. Current solution is to mark the
      // whole output rect as damaged.
      pixel_moving_background_filters_rect.Union(
          cc::MathUtil::MapEnclosingClippedRect(
              render_pass->transform_to_root_target, render_pass->output_rect));
    }
  }
  base::flat_set<cc::RenderPassId> pixel_moving_background_filter_passes(
      std::move(pixel_moving_background_filter_passes_data),
      base::KEEP_FIRST_OF_DUPES);

  for (const auto& render_pass : base::Reversed(frame.render_pass_list)) {
    cc::RenderPassId remapped_pass_id =
        RemapPassId(render_pass->id, surface_id);
    bool has_pixel_moving_filter =
        render_pass->filters.HasFilterThatMovesPixels();
    if (has_pixel_moving_filter)
      moved_pixel_passes_.insert(remapped_pass_id);
    bool in_moved_pixel_pass = has_pixel_moving_filter ||
                               !!moved_pixel_passes_.count(remapped_pass_id);
    for (auto* quad : render_pass->quad_list) {
      if (quad->material == cc::DrawQuad::SURFACE_CONTENT) {
        const auto* surface_quad = cc::SurfaceDrawQuad::MaterialCast(quad);
        gfx::Transform target_to_surface_transform(
            render_pass->transform_to_root_target,
            surface_quad->shared_quad_state->quad_to_target_transform);
        child_surfaces.emplace_back(surface_quad->surface_id,
                                    in_moved_pixel_pass, remapped_pass_id,
                                    target_to_surface_transform);
      } else if (quad->material == cc::DrawQuad::RENDER_PASS) {
        const auto* render_pass_quad =
            cc::RenderPassDrawQuad::MaterialCast(quad);
        if (in_moved_pixel_pass) {
          moved_pixel_passes_.insert(
              RemapPassId(render_pass_quad->render_pass_id, surface_id));
        }
        if (pixel_moving_background_filter_passes.count(
                render_pass_quad->render_pass_id)) {
          in_moved_pixel_pass = true;
        }
        render_pass_dependencies_[remapped_pass_id].insert(
            RemapPassId(render_pass_quad->render_pass_id, surface_id));
      }

      if (!provider_)
        continue;
      for (ResourceId resource_id : quad->resources) {
        if (!child_to_parent_map.count(resource_id)) {
          invalid_frame = true;
          break;
        }
        referenced_resources.push_back(resource_id);
      }
    }
  }

  if (invalid_frame)
    return gfx::Rect();
  CHECK(debug_weak_this.get());
  valid_surfaces_.insert(surface->surface_id());

  ResourceIdSet resource_set(std::move(referenced_resources),
                             base::KEEP_FIRST_OF_DUPES);
  if (provider_)
    provider_->DeclareUsedResourcesFromChild(child_id, resource_set);
  CHECK(debug_weak_this.get());

  gfx::Rect damage_rect;
  gfx::Rect full_damage;
  cc::RenderPass* last_pass = frame.render_pass_list.back().get();
  full_damage = last_pass->output_rect;
  damage_rect =
      DamageRectForSurface(surface, *last_pass, last_pass->output_rect);

  // Avoid infinite recursion by adding current surface to
  // referenced_surfaces_.
  referenced_surfaces_.insert(surface->surface_id());
  for (const auto& surface_info : child_surfaces) {
    gfx::Rect surface_damage =
        PrewalkTree(surface_info.id, surface_info.has_moved_pixels,
                    surface_info.parent_pass_id, result);
    if (surface_damage.IsEmpty())
      continue;
    if (surface_info.has_moved_pixels) {
      // Areas outside the rect hit by target_to_surface_transform may be
      // modified if there is a filter that moves pixels.
      damage_rect = full_damage;
      continue;
    }

    damage_rect.Union(cc::MathUtil::MapEnclosingClippedRect(
        surface_info.target_to_surface_transform, surface_damage));
  }

  CHECK(debug_weak_this.get());
  for (const auto& surface_id : frame.metadata.referenced_surfaces) {
    if (!contained_surfaces_.count(surface_id)) {
      result->undrawn_surfaces.insert(surface_id);
      PrewalkTree(surface_id, false, 0, result);
    }
  }

  CHECK(debug_weak_this.get());
  // TODO(staraz): It shouldn't need to call the callback when the damage is
  // from |surface| and not from |child_surfaces|.
  if (!damage_rect.IsEmpty()) {
    surface->RunWillDrawCallback(damage_rect);
    manager_->SurfaceWillDraw(surface->surface_id());
  }

  CHECK(debug_weak_this.get());
  for (const auto& render_pass : frame.render_pass_list) {
    if (!render_pass->copy_requests.empty()) {
      cc::RenderPassId remapped_pass_id =
          RemapPassId(render_pass->id, surface_id);
      copy_request_passes_.insert(remapped_pass_id);
    }
    if (render_pass->cache_render_pass)
      has_cached_render_passes_ = true;
  }

  // TODO(jbauman): Remove when https://crbug.com/745684 fixed.
  CHECK(surface->surface_id() == surface_id);
  auto it = referenced_surfaces_.find(surface_id);
  // TODO(jbauman): Remove when https://crbug.com/745684 fixed.
  CHECK(referenced_surfaces_.end() != it);
  referenced_surfaces_.erase(it);
  if (!damage_rect.IsEmpty() && frame.metadata.may_contain_video)
    result->may_contain_video = true;

  damage_rect.Union(pixel_moving_background_filters_rect);
  return damage_rect;
}

void SurfaceAggregator::CopyUndrawnSurfaces(PrewalkResult* prewalk_result) {
  // undrawn_surfaces are Surfaces that were identified by prewalk as being
  // referenced by a drawn Surface, but aren't contained in a SurfaceDrawQuad.
  // They need to be iterated over to ensure that any copy requests on them
  // (or on Surfaces they reference) are executed.
  std::vector<SurfaceId> surfaces_to_copy(
      prewalk_result->undrawn_surfaces.begin(),
      prewalk_result->undrawn_surfaces.end());
  DCHECK(referenced_surfaces_.empty());

  for (size_t i = 0; i < surfaces_to_copy.size(); i++) {
    SurfaceId surface_id = surfaces_to_copy[i];
    Surface* surface = manager_->GetSurfaceForId(surface_id);
    if (!surface)
      continue;
    if (!surface->HasActiveFrame())
      continue;
    const cc::CompositorFrame& frame = surface->GetActiveFrame();
    bool surface_has_copy_requests = false;
    for (const auto& render_pass : frame.render_pass_list) {
      surface_has_copy_requests |= !render_pass->copy_requests.empty();
    }
    if (!surface_has_copy_requests) {
      // Children are not necessarily included in undrawn_surfaces (because
      // they weren't referenced directly from a drawn surface), but may have
      // copy requests, so make sure to check them as well.
      for (const auto& child_id : frame.metadata.referenced_surfaces) {
        // Don't iterate over the child Surface if it was already listed as a
        // child of a different Surface, or in the case where there's infinite
        // recursion.
        if (!prewalk_result->undrawn_surfaces.count(child_id)) {
          surfaces_to_copy.push_back(child_id);
          prewalk_result->undrawn_surfaces.insert(child_id);
        }
      }
    } else {
      referenced_surfaces_.insert(surface_id);
      CopyPasses(frame, surface);
      // CopyPasses may have mutated container, need to re-query to erase.
      referenced_surfaces_.erase(referenced_surfaces_.find(surface_id));
    }
  }
}

void SurfaceAggregator::PropagateCopyRequestPasses() {
  std::vector<cc::RenderPassId> copy_requests_to_iterate(
      copy_request_passes_.begin(), copy_request_passes_.end());
  while (!copy_requests_to_iterate.empty()) {
    cc::RenderPassId first = copy_requests_to_iterate.back();
    copy_requests_to_iterate.pop_back();
    auto it = render_pass_dependencies_.find(first);
    if (it == render_pass_dependencies_.end())
      continue;
    for (auto pass : it->second) {
      if (copy_request_passes_.insert(pass).second) {
        copy_requests_to_iterate.push_back(pass);
      }
    }
  }
}

cc::CompositorFrame SurfaceAggregator::Aggregate(const SurfaceId& surface_id) {
  uma_stats_.Reset();

  Surface* surface = manager_->GetSurfaceForId(surface_id);
  DCHECK(surface);
  contained_surfaces_[surface_id] = surface->GetActiveFrameIndex();

  if (!surface->HasActiveFrame())
    return {};

  const cc::CompositorFrame& root_surface_frame = surface->GetActiveFrame();
  TRACE_EVENT0("viz", "SurfaceAggregator::Aggregate");

  cc::CompositorFrame frame;

  dest_pass_list_ = &frame.render_pass_list;

  valid_surfaces_.clear();
  has_cached_render_passes_ = false;
  PrewalkResult prewalk_result;
  root_damage_rect_ = PrewalkTree(surface_id, false, 0, &prewalk_result);
  PropagateCopyRequestPasses();
  has_copy_requests_ = !copy_request_passes_.empty();
  frame.metadata.may_contain_video = prewalk_result.may_contain_video;

  CopyUndrawnSurfaces(&prewalk_result);
  referenced_surfaces_.insert(surface_id);
  CopyPasses(root_surface_frame, surface);
  // CopyPasses may have mutated container, need to re-query to erase.
  referenced_surfaces_.erase(referenced_surfaces_.find(surface_id));
  AddColorConversionPass();

  moved_pixel_passes_.clear();
  copy_request_passes_.clear();
  contributing_content_damaged_passes_.clear();
  render_pass_dependencies_.clear();

  // Remove all render pass mappings that weren't used in the current frame.
  for (auto it = render_pass_allocator_map_.begin();
       it != render_pass_allocator_map_.end();) {
    if (it->second.in_use) {
      it->second.in_use = false;
      it++;
    } else {
      it = render_pass_allocator_map_.erase(it);
    }
  }

  DCHECK(referenced_surfaces_.empty());

  if (dest_pass_list_->empty())
    return {};

  dest_pass_list_ = NULL;
  ProcessAddedAndRemovedSurfaces();
  contained_surfaces_.swap(previous_contained_surfaces_);
  contained_surfaces_.clear();

  for (auto it : previous_contained_surfaces_) {
    Surface* surface = manager_->GetSurfaceForId(it.first);
    if (surface)
      surface->TakeLatencyInfo(&frame.metadata.latency_info);
  }

  // TODO(jamesr): Aggregate all resource references into the returned frame's
  // resource list.

  // Log UMA stats for SurfaceDrawQuads on the number of surfaces that were
  // aggregated together and any failures.
  UMA_HISTOGRAM_EXACT_LINEAR(kUmaValidSurface, uma_stats_.valid_surface,
                             kUmaStatMaxSurfaces);
  UMA_HISTOGRAM_EXACT_LINEAR(kUmaMissingSurface, uma_stats_.missing_surface,
                             kUmaStatMaxSurfaces);
  UMA_HISTOGRAM_EXACT_LINEAR(kUmaNoActiveFrame, uma_stats_.no_active_frame,
                             kUmaStatMaxSurfaces);

  return frame;
}

void SurfaceAggregator::ReleaseResources(const SurfaceId& surface_id) {
  auto it = surface_id_to_resource_child_id_.find(surface_id);
  if (it != surface_id_to_resource_child_id_.end()) {
    provider_->DestroyChild(it->second);
    surface_id_to_resource_child_id_.erase(it);
  }
}

void SurfaceAggregator::SetFullDamageForSurface(const SurfaceId& surface_id) {
  auto it = previous_contained_surfaces_.find(surface_id);
  if (it == previous_contained_surfaces_.end())
    return;
  // Set the last drawn index as 0 to ensure full damage next time it's drawn.
  it->second = 0;
}

void SurfaceAggregator::SetOutputColorSpace(
    const gfx::ColorSpace& blending_color_space,
    const gfx::ColorSpace& output_color_space) {
  blending_color_space_ = blending_color_space.IsValid()
                              ? blending_color_space
                              : gfx::ColorSpace::CreateSRGB();
  output_color_space_ = output_color_space.IsValid()
                            ? output_color_space
                            : gfx::ColorSpace::CreateSRGB();
}

}  // namespace viz
