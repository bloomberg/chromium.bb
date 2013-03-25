// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/delegated_renderer_layer_impl.h"

#include "base/bind.h"
#include "cc/base/math_util.h"
#include "cc/layers/append_quads_data.h"
#include "cc/layers/quad_sink.h"
#include "cc/layers/render_pass_sink.h"
#include "cc/output/delegated_frame_data.h"
#include "cc/quads/render_pass_draw_quad.h"
#include "cc/quads/solid_color_draw_quad.h"
#include "cc/trees/layer_tree_impl.h"

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

bool DelegatedRendererLayerImpl::HasDelegatedContent() const {
  return !render_passes_in_draw_order_.empty();
}

bool DelegatedRendererLayerImpl::HasContributingDelegatedRenderPasses() const {
  // The root RenderPass for the layer is merged with its target
  // RenderPass in each frame. So we only have extra RenderPasses
  // to merge when we have a non-root RenderPass present.
  return render_passes_in_draw_order_.size() > 1;
}

static ResourceProvider::ResourceId ResourceRemapHelper(
    bool* invalid_frame,
    const ResourceProvider::ResourceIdMap& child_to_parent_map,
    ResourceProvider::ResourceIdSet *remapped_resources,
    ResourceProvider::ResourceId id) {

  ResourceProvider::ResourceIdMap::const_iterator it =
      child_to_parent_map.find(id);
  if (it == child_to_parent_map.end()) {
    *invalid_frame = true;
    return 0;
  }

  DCHECK(it->first == id);
  ResourceProvider::ResourceId remapped_id = it->second;
  remapped_resources->insert(remapped_id);
  return remapped_id;
}

void DelegatedRendererLayerImpl::SetFrameData(
    scoped_ptr<DelegatedFrameData> frame_data,
    gfx::RectF damage_in_frame,
    TransferableResourceArray* resources_for_ack) {
  CreateChildIdIfNeeded();
  DCHECK(child_id_);

  ResourceProvider* resource_provider = layer_tree_impl()->resource_provider();
    const ResourceProvider::ResourceIdMap& resource_map =
        resource_provider->GetChildToParentMap(child_id_);

  if (frame_data) {
    // A frame with an empty root render pass is invalid.
    DCHECK(frame_data->render_pass_list.empty() ||
           !frame_data->render_pass_list.back()->output_rect.IsEmpty());

    // Display size is already set so we can compute what the damage rect
    // will be in layer space.
    if (!frame_data->render_pass_list.empty()) {
      RenderPass* new_root_pass = frame_data->render_pass_list.back();
      gfx::RectF damage_in_layer = MathUtil::MapClippedRect(
          DelegatedFrameToLayerSpaceTransform(
              new_root_pass->output_rect.size()),
          damage_in_frame);
      set_update_rect(gfx::UnionRects(update_rect(), damage_in_layer));
    }

    resource_provider->ReceiveFromChild(child_id_, frame_data->resource_list);

    bool invalid_frame = false;
    ResourceProvider::ResourceIdSet used_resources;
    DrawQuad::ResourceIteratorCallback remap_resources_to_parent_callback =
        base::Bind(&ResourceRemapHelper,
                   &invalid_frame,
                   resource_map,
                   &used_resources);
    for (size_t i = 0; i < frame_data->render_pass_list.size(); ++i) {
      RenderPass* pass = frame_data->render_pass_list[i];
      for (size_t j = 0; j < pass->quad_list.size(); ++j) {
        DrawQuad* quad = pass->quad_list[j];
        quad->IterateResources(remap_resources_to_parent_callback);
      }
    }

    if (!invalid_frame) {
      // Save the remapped quads on the layer. This steals the quads and render
      // passes from the frame_data.
      SetRenderPasses(&frame_data->render_pass_list);
      resources_.swap(used_resources);
    }
  }

  ResourceProvider::ResourceIdArray unused_resources;
  for (ResourceProvider::ResourceIdMap::const_iterator it =
           resource_map.begin();
       it != resource_map.end();
       ++it) {
    bool resource_is_in_current_frame = resources_.count(it->second);
    bool resource_is_in_use = resource_provider->InUseByConsumer(it->second);
    if (!resource_is_in_current_frame && !resource_is_in_use)
      unused_resources.push_back(it->second);
  }
  resource_provider->PrepareSendToChild(
      child_id_, unused_resources, resources_for_ack);
}

void DelegatedRendererLayerImpl::SetDisplaySize(gfx::Size size) {
  if (display_size_ == size)
    return;
  display_size_ = size;
  NoteLayerPropertyChanged();
}

void DelegatedRendererLayerImpl::SetRenderPasses(
    ScopedPtrVector<RenderPass>* render_passes_in_draw_order) {
  gfx::RectF old_root_damage;
  if (!render_passes_in_draw_order_.empty())
    old_root_damage = render_passes_in_draw_order_.back()->damage_rect;

  ClearRenderPasses();

  for (size_t i = 0; i < render_passes_in_draw_order->size(); ++i) {
    ScopedPtrVector<RenderPass>::iterator to_take =
        render_passes_in_draw_order->begin() + i;
    render_passes_index_by_id_.insert(
        std::pair<RenderPass::Id, int>((*to_take)->id, i));
    scoped_ptr<RenderPass> taken_render_pass =
        render_passes_in_draw_order->take(to_take);
    render_passes_in_draw_order_.push_back(taken_render_pass.Pass());
  }

  if (!render_passes_in_draw_order_.empty())
    render_passes_in_draw_order_.back()->damage_rect.Union(old_root_damage);
}

void DelegatedRendererLayerImpl::ClearRenderPasses() {
  // FIXME: Release the resources back to the nested compositor.
  render_passes_index_by_id_.clear();
  render_passes_in_draw_order_.clear();
}

scoped_ptr<LayerImpl> DelegatedRendererLayerImpl::CreateLayerImpl(
    LayerTreeImpl* tree_impl) {
  return DelegatedRendererLayerImpl::Create(tree_impl, id()).PassAs<LayerImpl>();
}

void DelegatedRendererLayerImpl::DidLoseOutputSurface() {
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

RenderPass::Id DelegatedRendererLayerImpl::FirstContributingRenderPassId()
    const {
  return RenderPass::Id(id(), IndexToId(0));
}

RenderPass::Id DelegatedRendererLayerImpl::NextContributingRenderPassId(
    RenderPass::Id previous) const {
  return RenderPass::Id(previous.layer_id, previous.index + 1);
}

RenderPass::Id DelegatedRendererLayerImpl::ConvertDelegatedRenderPassId(
    RenderPass::Id delegated_render_pass_id) const {
  base::hash_map<RenderPass::Id, int>::const_iterator found =
      render_passes_index_by_id_.find(delegated_render_pass_id);
  DCHECK(found != render_passes_index_by_id_.end());
  unsigned delegated_render_pass_index = found->second;
  return RenderPass::Id(id(), IndexToId(delegated_render_pass_index));
}

void DelegatedRendererLayerImpl::AppendContributingRenderPasses(
    RenderPassSink* render_pass_sink) {
  DCHECK(HasContributingDelegatedRenderPasses());

  for (size_t i = 0; i < render_passes_in_draw_order_.size() - 1; ++i) {
    RenderPass::Id output_render_pass_id =
        ConvertDelegatedRenderPassId(render_passes_in_draw_order_[i]->id);

    // Don't clash with the RenderPass we generate if we own a RenderSurface.
    DCHECK(output_render_pass_id.index > 0);

    render_pass_sink->AppendRenderPass(
        render_passes_in_draw_order_[i]->Copy(output_render_pass_id));
  }
}

void DelegatedRendererLayerImpl::AppendQuads(
    QuadSink* quad_sink,
    AppendQuadsData* append_quads_data) {
  AppendRainbowDebugBorder(quad_sink, append_quads_data);

  if (render_passes_in_draw_order_.empty())
    return;

  RenderPass::Id target_render_pass_id = append_quads_data->renderPassId;

  const RenderPass* root_delegated_render_pass =
      render_passes_in_draw_order_.back();

  DCHECK(root_delegated_render_pass->output_rect.origin().IsOrigin());
  gfx::Size frame_size = root_delegated_render_pass->output_rect.size();

  // If the index of the EenderPassId is 0, then it is a RenderPass generated
  // for a layer in this compositor, not the delegated renderer. Then we want to
  // merge our root RenderPass with the target RenderPass. Otherwise, it is some
  // RenderPass which we added from the delegated renderer.
  bool should_merge_root_render_pass_with_target = !target_render_pass_id.index;
  if (should_merge_root_render_pass_with_target) {
    // Verify that the RenderPass we are appending to is created our
    // render_target.
    DCHECK(target_render_pass_id.layer_id == render_target()->id());

    AppendRenderPassQuads(
        quad_sink, append_quads_data, root_delegated_render_pass, frame_size);
  } else {
    // Verify that the RenderPass we are appending to was created by us.
    DCHECK(target_render_pass_id.layer_id == id());

    int render_pass_index = IdToIndex(target_render_pass_id.index);
    const RenderPass* delegated_render_pass =
        render_passes_in_draw_order_[render_pass_index];
    AppendRenderPassQuads(
        quad_sink, append_quads_data, delegated_render_pass, frame_size);
  }
}

void DelegatedRendererLayerImpl::AppendRainbowDebugBorder(
    QuadSink* quad_sink,
    AppendQuadsData* append_quads_data) {
  if (!ShowDebugBorders())
    return;

  SharedQuadState* shared_quad_state =
      quad_sink->UseSharedQuadState(CreateSharedQuadState());

  SkColor color;
  float border_width;
  GetDebugBorderProperties(&color, &border_width);

  SkColor colors[] = {
    0x80ff0000,  // Red.
    0x80ffa500,  // Orange.
    0x80ffff00,  // Yellow.
    0x80008000,  // Green.
    0x800000ff,  // Blue.
    0x80ee82ee,  // Violet.
  };
  const int kNumColors = arraysize(colors);

  const int kStripeWidth = 300;
  const int kStripeHeight = 300;

  for (size_t i = 0; ; ++i) {
    // For horizontal lines.
    int x =  kStripeWidth * i;
    int width = std::min(kStripeWidth, content_bounds().width() - x - 1);

    // For vertical lines.
    int y = kStripeHeight * i;
    int height = std::min(kStripeHeight, content_bounds().height() - y - 1);

    gfx::Rect top(x, 0, width, border_width);
    gfx::Rect bottom(x,
                     content_bounds().height() - border_width,
                     width,
                     border_width);
    gfx::Rect left(0, y, border_width, height);
    gfx::Rect right(content_bounds().width() - border_width,
                    y,
                    border_width,
                    height);

    if (top.IsEmpty() && left.IsEmpty())
      break;

    if (!top.IsEmpty()) {
      scoped_ptr<SolidColorDrawQuad> top_quad = SolidColorDrawQuad::Create();
      top_quad->SetNew(shared_quad_state, top, colors[i % kNumColors]);
      quad_sink->Append(top_quad.PassAs<DrawQuad>(), append_quads_data);

      scoped_ptr<SolidColorDrawQuad> bottom_quad = SolidColorDrawQuad::Create();
      bottom_quad->SetNew(
          shared_quad_state, bottom, colors[kNumColors - 1 - (i % kNumColors)]);
      quad_sink->Append(bottom_quad.PassAs<DrawQuad>(), append_quads_data);
    }
    if (!left.IsEmpty()) {
      scoped_ptr<SolidColorDrawQuad> left_quad = SolidColorDrawQuad::Create();
      left_quad->SetNew(
          shared_quad_state, left, colors[kNumColors - 1 - (i % kNumColors)]);
      quad_sink->Append(left_quad.PassAs<DrawQuad>(), append_quads_data);

      scoped_ptr<SolidColorDrawQuad> right_quad = SolidColorDrawQuad::Create();
      right_quad->SetNew(shared_quad_state, right, colors[i % kNumColors]);
      quad_sink->Append(right_quad.PassAs<DrawQuad>(), append_quads_data);
    }
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
      output_shared_quad_state = quad_sink->UseSharedQuadState(
          delegated_shared_quad_state->Copy());

      bool is_root_delegated_render_pass =
          delegated_render_pass == render_passes_in_draw_order_.back();
      if (is_root_delegated_render_pass) {
        // Don't allow areas inside the bounds that are empty.
        DCHECK(display_size_.IsEmpty() ||
               gfx::Rect(display_size_).Contains(gfx::Rect(bounds())));
        gfx::Transform delegated_frame_to_target_transform =
            draw_transform() * DelegatedFrameToLayerSpaceTransform(frame_size);

        output_shared_quad_state->content_to_target_transform.ConcatTransform(
            delegated_frame_to_target_transform);

        if (render_target() == this) {
          DCHECK(!is_clipped());
          DCHECK(render_surface());
          output_shared_quad_state->clip_rect = MathUtil::MapClippedRect(
              delegated_frame_to_target_transform,
              output_shared_quad_state->clip_rect);
        } else {
          gfx::Rect clip_rect = drawable_content_rect();
          if (output_shared_quad_state->is_clipped) {
            clip_rect.Intersect(MathUtil::MapClippedRect(
                delegated_frame_to_target_transform,
                output_shared_quad_state->clip_rect));
          }
          output_shared_quad_state->clip_rect = clip_rect;
          output_shared_quad_state->is_clipped = true;
        }

        output_shared_quad_state->opacity *= draw_opacity();
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

    quad_sink->Append(output_quad.Pass(), append_quads_data);
  }
}

const char* DelegatedRendererLayerImpl::LayerTypeAsString() const {
  return "DelegatedRendererLayer";
}

void DelegatedRendererLayerImpl::CreateChildIdIfNeeded() {
  if (child_id_)
    return;

  ResourceProvider* resource_provider = layer_tree_impl()->resource_provider();
  child_id_ = resource_provider->CreateChild();
}

void DelegatedRendererLayerImpl::ClearChildId() {
  if (!child_id_)
    return;

  ResourceProvider* resource_provider = layer_tree_impl()->resource_provider();
  resource_provider->DestroyChild(child_id_);
  child_id_ = 0;
}

}  // namespace cc
