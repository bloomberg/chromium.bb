// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/surfaces/surface_aggregator.h"

#include "base/containers/hash_tables.h"
#include "base/logging.h"
#include "cc/output/compositor_frame.h"
#include "cc/output/delegated_frame_data.h"
#include "cc/quads/draw_quad.h"
#include "cc/quads/render_pass_draw_quad.h"
#include "cc/quads/shared_quad_state.h"
#include "cc/quads/surface_draw_quad.h"
#include "cc/surfaces/surface.h"
#include "cc/surfaces/surface_manager.h"

namespace cc {

SurfaceAggregator::SurfaceAggregator(SurfaceManager* manager)
    : manager_(manager) {
  DCHECK(manager_);
}

SurfaceAggregator::~SurfaceAggregator() {}

DelegatedFrameData* SurfaceAggregator::GetReferencedDataForSurfaceID(
    int surface_id) {
  Surface* referenced_surface = manager_->GetSurfaceForID(surface_id);
  if (!referenced_surface)
    return NULL;  // Invalid surface id, skip this quad.
  CompositorFrame* referenced_frame = referenced_surface->GetEligibleFrame();
  if (!referenced_frame)
    return NULL;
  return referenced_frame->delegated_frame_data.get();
}

class SurfaceAggregator::RenderPassIdAllocator {
 public:
  explicit RenderPassIdAllocator(int surface_id)
      : surface_id_(surface_id), next_index_(1) {}
  ~RenderPassIdAllocator() {}

  void AddKnownPass(RenderPass::Id id) {
    if (id_to_index_map_.find(id) != id_to_index_map_.end())
      return;
    id_to_index_map_[id] = next_index_++;
  }

  RenderPass::Id Remap(RenderPass::Id id) {
    DCHECK(id_to_index_map_.find(id) != id_to_index_map_.end());
    return RenderPass::Id(surface_id_, id_to_index_map_[id]);
  }

 private:
  base::hash_map<RenderPass::Id, int> id_to_index_map_;
  int surface_id_;
  int next_index_;

  DISALLOW_COPY_AND_ASSIGN(RenderPassIdAllocator);
};

RenderPass::Id SurfaceAggregator::RemapPassId(
    RenderPass::Id surface_local_pass_id,
    int surface_id) {
  RenderPassIdAllocator* allocator = render_pass_allocator_map_.get(surface_id);
  if (!allocator) {
    allocator = new RenderPassIdAllocator(surface_id);
    render_pass_allocator_map_.set(surface_id, make_scoped_ptr(allocator));
  }
  allocator->AddKnownPass(surface_local_pass_id);
  return allocator->Remap(surface_local_pass_id);
}

void SurfaceAggregator::HandleSurfaceQuad(const SurfaceDrawQuad* surface_quad,
                                          RenderPass* dest_pass) {
  int surface_id = surface_quad->surface_id;
  // If this surface's id is already in our referenced set then it creates
  // a cycle in the graph and should be dropped.
  if (referenced_surfaces_.count(surface_id))
    return;
  DelegatedFrameData* referenced_data =
      GetReferencedDataForSurfaceID(surface_id);
  if (!referenced_data)
    return;
  std::set<int>::iterator it = referenced_surfaces_.insert(surface_id).first;

  const RenderPassList& referenced_passes = referenced_data->render_pass_list;
  for (size_t j = 0; j + 1 < referenced_passes.size(); ++j) {
    const RenderPass& source = *referenced_passes[j];

    scoped_ptr<RenderPass> copy_pass(RenderPass::Create());

    RenderPass::Id remapped_pass_id = RemapPassId(source.id, surface_id);

    copy_pass->SetAll(remapped_pass_id,
                      source.output_rect,
                      source.damage_rect,
                      source.transform_to_root_target,
                      source.has_transparent_background);

    CopyQuadsToPass(source.quad_list,
                    source.shared_quad_state_list,
                    copy_pass.get(),
                    surface_id);

    dest_pass_list_->push_back(copy_pass.Pass());
  }

  // TODO(jamesr): Clean up last pass special casing.
  const RenderPass& last_pass = *referenced_data->render_pass_list.back();
  const QuadList& quads = last_pass.quad_list;

  for (size_t j = 0; j < last_pass.shared_quad_state_list.size(); ++j) {
    dest_pass->shared_quad_state_list.push_back(
        last_pass.shared_quad_state_list[j]->Copy());
  }
  // TODO(jamesr): Map transform correctly for quads in the referenced
  // surface into this pass's space.
  // TODO(jamesr): Make sure clipping is enforced.
  CopyQuadsToPass(
      quads, last_pass.shared_quad_state_list, dest_pass, surface_id);

  referenced_surfaces_.erase(it);
}

void SurfaceAggregator::CopyQuadsToPass(
    const QuadList& source_quad_list,
    const SharedQuadStateList& source_shared_quad_state_list,
    RenderPass* dest_pass,
    int surface_id) {
  for (size_t j = 0; j < source_shared_quad_state_list.size(); ++j) {
    dest_pass->shared_quad_state_list.push_back(
        source_shared_quad_state_list[j]->Copy());
  }

  for (size_t i = 0, sqs_i = 0; i < source_quad_list.size(); ++i) {
    DrawQuad* quad = source_quad_list[i];

    while (quad->shared_quad_state != source_shared_quad_state_list[sqs_i]) {
      ++sqs_i;
      DCHECK_LT(sqs_i, source_shared_quad_state_list.size());
      DCHECK_LT(sqs_i, dest_pass->shared_quad_state_list.size());
    }
    DCHECK_EQ(quad->shared_quad_state, source_shared_quad_state_list[sqs_i]);

    if (quad->material == DrawQuad::SURFACE_CONTENT) {
      const SurfaceDrawQuad* surface_quad = SurfaceDrawQuad::MaterialCast(quad);
      HandleSurfaceQuad(surface_quad, dest_pass);
    } else if (quad->material == DrawQuad::RENDER_PASS) {
      const RenderPassDrawQuad* pass_quad =
          RenderPassDrawQuad::MaterialCast(quad);
      RenderPass::Id original_pass_id = pass_quad->render_pass_id;
      RenderPass::Id remapped_pass_id =
          RemapPassId(original_pass_id, surface_id);

      dest_pass->quad_list.push_back(
          pass_quad->Copy(dest_pass->shared_quad_state_list[sqs_i],
                          remapped_pass_id).PassAs<DrawQuad>());
    } else {
      dest_pass->quad_list.push_back(
          quad->Copy(dest_pass->shared_quad_state_list[sqs_i]));
    }
  }
}

void SurfaceAggregator::CopyPasses(const RenderPassList& source_pass_list,
                                   int surface_id) {
  for (size_t i = 0; i < source_pass_list.size(); ++i) {
    const RenderPass& source = *source_pass_list[i];

    scoped_ptr<RenderPass> copy_pass(RenderPass::Create());

    RenderPass::Id remapped_pass_id = RemapPassId(source.id, surface_id);

    copy_pass->SetAll(remapped_pass_id,
                      source.output_rect,
                      source.damage_rect,
                      source.transform_to_root_target,
                      source.has_transparent_background);

    CopyQuadsToPass(source.quad_list,
                    source.shared_quad_state_list,
                    copy_pass.get(),
                    surface_id);

    dest_pass_list_->push_back(copy_pass.Pass());
  }
}

scoped_ptr<CompositorFrame> SurfaceAggregator::Aggregate(int surface_id) {
  Surface* surface = manager_->GetSurfaceForID(surface_id);
  if (!surface)
    return scoped_ptr<CompositorFrame>();
  CompositorFrame* root_surface_frame = surface->GetEligibleFrame();
  if (!root_surface_frame)
    return scoped_ptr<CompositorFrame>();

  scoped_ptr<CompositorFrame> frame(new CompositorFrame);
  frame->delegated_frame_data = make_scoped_ptr(new DelegatedFrameData);

  DCHECK(root_surface_frame->delegated_frame_data);

  const RenderPassList& source_pass_list =
      root_surface_frame->delegated_frame_data->render_pass_list;

  referenced_surfaces_.insert(surface_id);

  dest_pass_list_ = &frame->delegated_frame_data->render_pass_list;
  CopyPasses(source_pass_list, surface_id);

  referenced_surfaces_.clear();
  dest_pass_list_ = NULL;

  // TODO(jamesr): Aggregate all resource references into the returned frame's
  // resource list.

  return frame.Pass();
}

}  // namespace cc
