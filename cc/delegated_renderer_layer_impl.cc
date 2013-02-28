// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/delegated_renderer_layer_impl.h"

#include "cc/append_quads_data.h"
#include "cc/delegated_frame_data.h"
#include "cc/layer_tree_impl.h"
#include "cc/math_util.h"
#include "cc/quad_sink.h"
#include "cc/render_pass_draw_quad.h"
#include "cc/render_pass_sink.h"

namespace cc {

DelegatedRendererLayerImpl::DelegatedRendererLayerImpl(
    LayerTreeImpl* tree_impl, int id)
    : LayerImpl(tree_impl, id),
      child_id_(0) {
}

DelegatedRendererLayerImpl::~DelegatedRendererLayerImpl() {
  ClearRenderPasses();
  ClearChildId();
}

bool DelegatedRendererLayerImpl::hasDelegatedContent() const {
  return !render_passes_in_draw_order_.empty();
}

bool DelegatedRendererLayerImpl::hasContributingDelegatedRenderPasses() const {
  // The root RenderPass for the layer is merged with its target
  // RenderPass in each frame. So we only have extra RenderPasses
  // to merge when we have a non-root RenderPass present.
  return render_passes_in_draw_order_.size() > 1;
}

void DelegatedRendererLayerImpl::SetFrameData(
    scoped_ptr<DelegatedFrameData> frame_data,
    gfx::RectF damage_in_frame) {
  CreateChildIdIfNeeded();
  DCHECK(child_id_);

  // Display size is already set so we can compute what the damage rect
  // will be in layer space.
  RenderPass* new_root_pass = frame_data->render_pass_list.empty() ?
                              NULL : frame_data->render_pass_list.back();
  bool new_frame_is_empty = !new_root_pass;

  RenderPass* old_root_pass = render_passes_in_draw_order_.empty() ?
                              NULL : render_passes_in_draw_order_.back();
  bool old_frame_is_empty = !old_root_pass;

  gfx::RectF damage_in_layer;
  if (new_frame_is_empty) {
    if (!old_frame_is_empty)
      damage_in_layer = gfx::Rect(bounds());
  } else {
    DCHECK(!new_root_pass->output_rect.IsEmpty());
    damage_in_layer = MathUtil::mapClippedRect(
        DelegatedFrameToLayerSpaceTransform(new_root_pass->output_rect.size()),
        damage_in_frame);
  }
  setUpdateRect(gfx::UnionRects(updateRect(), damage_in_layer));

  // TODO(danakj): Convert the resource ids the render passes and return data
  // for a frame ack.
  SetRenderPasses(frame_data->render_pass_list);
}

void DelegatedRendererLayerImpl::SetDisplaySize(gfx::Size size) {
  if (display_size_ == size)
    return;
  display_size_ = size;
  noteLayerPropertyChanged();
}

void DelegatedRendererLayerImpl::SetRenderPasses(
    ScopedPtrVector<RenderPass>& render_passes_in_draw_order) {
  gfx::RectF old_root_damage;
  if (!render_passes_in_draw_order_.empty())
    old_root_damage = render_passes_in_draw_order_.back()->damage_rect;

  ClearRenderPasses();

  for (size_t i = 0; i < render_passes_in_draw_order.size(); ++i) {
    render_passes_index_by_id_.insert(
        std::pair<RenderPass::Id, int>(render_passes_in_draw_order[i]->id, i));
    scoped_ptr<RenderPass> passed_render_pass =
        render_passes_in_draw_order.take(
            render_passes_in_draw_order.begin() + i);
    render_passes_in_draw_order_.push_back(passed_render_pass.Pass());
  }
  render_passes_in_draw_order.clear();

  if (!render_passes_in_draw_order_.empty())
    render_passes_in_draw_order_.back()->damage_rect.Union(old_root_damage);
}

void DelegatedRendererLayerImpl::ClearRenderPasses() {
  // FIXME: Release the resources back to the nested compositor.
  render_passes_index_by_id_.clear();
  render_passes_in_draw_order_.clear();
}

scoped_ptr<LayerImpl> DelegatedRendererLayerImpl::createLayerImpl(
    LayerTreeImpl* treeImpl) {
  return DelegatedRendererLayerImpl::create(treeImpl, id()).PassAs<LayerImpl>();
}

void DelegatedRendererLayerImpl::didLoseOutputSurface() {
  ClearRenderPasses();
  ClearChildId();
}

gfx::Transform DelegatedRendererLayerImpl::DelegatedFrameToLayerSpaceTransform(
    gfx::Size frame_size) const {
  gfx::Size display_size = display_size_.IsEmpty() ? bounds() : display_size_;

  gfx::Transform delegated_frame_to_layer_space_transform;
  delegated_frame_to_layer_space_transform.Scale(
      static_cast<double>(display_size.width()) / frame_size.width(),
      static_cast<double>(display_size.height()) / frame_size.height());
  return delegated_frame_to_layer_space_transform;
}

static inline int IndexToId(int index) { return index + 1; }
static inline int IdToIndex(int id) { return id - 1; }

RenderPass::Id DelegatedRendererLayerImpl::firstContributingRenderPassId()
    const {
  return RenderPass::Id(id(), IndexToId(0));
}

RenderPass::Id DelegatedRendererLayerImpl::nextContributingRenderPassId(
    RenderPass::Id previous) const {
  return RenderPass::Id(previous.layer_id, previous.index + 1);
}

RenderPass::Id DelegatedRendererLayerImpl::ConvertDelegatedRenderPassId(
    RenderPass::Id delegated_render_pass_id) const {
  base::hash_map<RenderPass::Id, int>::const_iterator found =
      render_passes_index_by_id_.find(delegated_render_pass_id);
  DCHECK(found != render_passes_index_by_id_.end());
  unsigned delegatedRenderPassIndex = found->second;
  return RenderPass::Id(id(), IndexToId(delegatedRenderPassIndex));
}

void DelegatedRendererLayerImpl::AppendContributingRenderPasses(
    RenderPassSink* render_pass_sink) {
  DCHECK(hasContributingDelegatedRenderPasses());

  for (size_t i = 0; i < render_passes_in_draw_order_.size() - 1; ++i) {
    RenderPass::Id output_render_pass_id =
        ConvertDelegatedRenderPassId(render_passes_in_draw_order_[i]->id);

    // Don't clash with the RenderPass we generate if we own a RenderSurface.
    DCHECK(output_render_pass_id.index > 0);

    render_pass_sink->appendRenderPass(
        render_passes_in_draw_order_[i]->Copy(output_render_pass_id));
  }
}

void DelegatedRendererLayerImpl::appendQuads(
    QuadSink& quad_sink, AppendQuadsData& append_quads_data) {
  if (render_passes_in_draw_order_.empty())
    return;

  RenderPass::Id target_render_pass_id = append_quads_data.renderPassId;

  const RenderPass* root_delegated_render_pass =
      render_passes_in_draw_order_.back();

  DCHECK(root_delegated_render_pass->output_rect.origin().IsOrigin());
  gfx::Size frame_size = root_delegated_render_pass->output_rect.size();

  // If the index of the renderPassId is 0, then it is a renderPass generated
  // for a layer in this compositor, not the delegated renderer. Then we want to
  // merge our root renderPass with the target renderPass. Otherwise, it is some
  // renderPass which we added from the delegated renderer.
  bool should_merge_root_render_pass_with_target = !target_render_pass_id.index;
  if (should_merge_root_render_pass_with_target) {
    // Verify that the renderPass we are appending to is created our
    // renderTarget.
    DCHECK(target_render_pass_id.layer_id == renderTarget()->id());

    AppendRenderPassQuads(
        &quad_sink, &append_quads_data, root_delegated_render_pass, frame_size);
  } else {
    // Verify that the renderPass we are appending to was created by us.
    DCHECK(target_render_pass_id.layer_id == id());

    int render_pass_index = IdToIndex(target_render_pass_id.index);
    const RenderPass* delegated_render_pass =
        render_passes_in_draw_order_[render_pass_index];
    AppendRenderPassQuads(
        &quad_sink, &append_quads_data, delegated_render_pass, frame_size);
  }
}

void DelegatedRendererLayerImpl::AppendRenderPassQuads(
    QuadSink* quad_sink,
    AppendQuadsData* append_quads_data,
    const RenderPass* delegated_render_pass,
    gfx::Size frame_size) const {

  const SharedQuadState* delegated_shared_quad_state = NULL;
  SharedQuadState* output_shared_quad_state = NULL;

  for (size_t i = 0; i < delegated_render_pass->quad_list.size(); ++i) {
    const DrawQuad* delegated_quad = delegated_render_pass->quad_list[i];

    if (delegated_quad->shared_quad_state != delegated_shared_quad_state) {
      delegated_shared_quad_state = delegated_quad->shared_quad_state;
      output_shared_quad_state = quad_sink->useSharedQuadState(
          delegated_shared_quad_state->Copy());

      bool is_root_delegated_render_pass =
          delegated_render_pass == render_passes_in_draw_order_.back();
      if (is_root_delegated_render_pass) {
        // Don't allow areas inside the bounds that are empty.
        DCHECK(display_size_.IsEmpty() ||
               gfx::Rect(display_size_).Contains(gfx::Rect(bounds())));
        gfx::Transform delegated_frame_to_target_transform =
            drawTransform() * DelegatedFrameToLayerSpaceTransform(frame_size);

        output_shared_quad_state->content_to_target_transform.ConcatTransform(
            delegated_frame_to_target_transform);

        if (renderTarget() == this) {
          DCHECK(!isClipped());
          DCHECK(renderSurface());
          output_shared_quad_state->clip_rect = MathUtil::mapClippedRect(
              delegated_frame_to_target_transform,
              output_shared_quad_state->clip_rect);
        } else {
          gfx::Rect clip_rect = drawableContentRect();
          if (output_shared_quad_state->is_clipped) {
            clip_rect.Intersect(MathUtil::mapClippedRect(
                delegated_frame_to_target_transform,
                output_shared_quad_state->clip_rect));
          }
          output_shared_quad_state->clip_rect = clip_rect;
          output_shared_quad_state->is_clipped = true;
        }

        output_shared_quad_state->opacity *= drawOpacity();
      }
    }
    DCHECK(output_shared_quad_state);

    scoped_ptr<DrawQuad> output_quad;
    if (delegated_quad->material != DrawQuad::RENDER_PASS) {
      output_quad = delegated_quad->Copy(output_shared_quad_state);
    } else {
      RenderPass::Id delegated_contributing_render_pass_id =
          RenderPassDrawQuad::MaterialCast(delegated_quad)->render_pass_id;
      RenderPass::Id output_contributing_render_pass_id =
          ConvertDelegatedRenderPassId(delegated_contributing_render_pass_id);
      DCHECK(output_contributing_render_pass_id !=
             append_quads_data->renderPassId);

      output_quad = RenderPassDrawQuad::MaterialCast(delegated_quad)->Copy(
          output_shared_quad_state,
          output_contributing_render_pass_id).PassAs<DrawQuad>();
    }
    DCHECK(output_quad.get());

    quad_sink->append(output_quad.Pass(), *append_quads_data);
  }
}

const char* DelegatedRendererLayerImpl::layerTypeAsString() const {
  return "DelegatedRendererLayer";
}

void DelegatedRendererLayerImpl::CreateChildIdIfNeeded() {
  if (child_id_)
    return;

  ResourceProvider* resource_provider = layerTreeImpl()->resource_provider();
  child_id_ = resource_provider->createChild();
}

void DelegatedRendererLayerImpl::ClearChildId() {
  if (!child_id_)
    return;

  ResourceProvider* resource_provider = layerTreeImpl()->resource_provider();
  resource_provider->destroyChild(child_id_);
  child_id_ = 0;
}

}  // namespace cc
