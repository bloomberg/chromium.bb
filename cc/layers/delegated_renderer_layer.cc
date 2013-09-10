// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/delegated_renderer_layer.h"

#include "cc/layers/delegated_renderer_layer_client.h"
#include "cc/layers/delegated_renderer_layer_impl.h"
#include "cc/output/delegated_frame_data.h"
#include "cc/quads/render_pass_draw_quad.h"
#include "cc/trees/layer_tree_host.h"

namespace cc {

scoped_refptr<DelegatedRendererLayer> DelegatedRendererLayer::Create(
    DelegatedRendererLayerClient* client) {
  return scoped_refptr<DelegatedRendererLayer>(
      new DelegatedRendererLayer(client));
}

DelegatedRendererLayer::DelegatedRendererLayer(
    DelegatedRendererLayerClient* client)
    : Layer(),
      client_(client),
      needs_filter_context_(false) {}

DelegatedRendererLayer::~DelegatedRendererLayer() {}

scoped_ptr<LayerImpl> DelegatedRendererLayer::CreateLayerImpl(
    LayerTreeImpl* tree_impl) {
  return DelegatedRendererLayerImpl::Create(
      tree_impl, layer_id_).PassAs<LayerImpl>();
}

void DelegatedRendererLayer::SetLayerTreeHost(LayerTreeHost* host) {
  if (layer_tree_host() == host) {
    Layer::SetLayerTreeHost(host);
    return;
  }

  if (!host) {
    // The active frame needs to be removed from the active tree and resources
    // returned before the commit is called complete.
    // TODO(danakj): Don't need to do this if the last frame commited was empty
    // or we never commited a frame with resources.
    SetNextCommitWaitsForActivation();
  } else {
    if (needs_filter_context_)
      host->set_needs_filter_context();
  }

  Layer::SetLayerTreeHost(host);
}

bool DelegatedRendererLayer::DrawsContent() const {
  return Layer::DrawsContent() && !frame_size_.IsEmpty();
}

void DelegatedRendererLayer::PushPropertiesTo(LayerImpl* impl) {
  Layer::PushPropertiesTo(impl);

  DelegatedRendererLayerImpl* delegated_impl =
      static_cast<DelegatedRendererLayerImpl*>(impl);

  delegated_impl->SetDisplaySize(display_size_);

  if (frame_data_)
    delegated_impl->SetFrameData(frame_data_.Pass(), damage_in_frame_);
  frame_data_.reset();
  damage_in_frame_ = gfx::RectF();

  delegated_impl->CollectUnusedResources(
      &unused_resources_for_child_compositor_);

  if (client_)
    client_->DidCommitFrameData();

  // TODO(danakj): TakeUnusedResourcesForChildCompositor requires a push
  // properties to happen in order to collect unused resources returned
  // from the parent compositor. crbug.com/259090
  needs_push_properties_ = true;
}

void DelegatedRendererLayer::SetDisplaySize(gfx::Size size) {
  if (display_size_ == size)
    return;
  display_size_ = size;
  SetNeedsCommit();
}

void DelegatedRendererLayer::SetFrameData(
    scoped_ptr<DelegatedFrameData> new_frame_data) {
  if (frame_data_) {
    // Copy the resources from the last provided frame into the unused resources
    // list, as the new frame will provide its own resources.
    TransferableResource::ReturnResources(
        frame_data_->resource_list,
        &unused_resources_for_child_compositor_);
  }
  frame_data_ = new_frame_data.Pass();
  if (!frame_data_->render_pass_list.empty()) {
    RenderPass* root_pass = frame_data_->render_pass_list.back();
    damage_in_frame_.Union(root_pass->damage_rect);
    frame_size_ = root_pass->output_rect.size();
  } else {
    frame_size_ = gfx::Size();
  }

  // If any RenderPassDrawQuad has a filter operation, then we need a filter
  // context to draw this layer's content.
  for (size_t i = 0;
       !needs_filter_context_ && i < frame_data_->render_pass_list.size();
       ++i) {
    const QuadList& quad_list = frame_data_->render_pass_list[i]->quad_list;
    for (size_t j = 0; !needs_filter_context_ && j < quad_list.size(); ++j) {
      if (quad_list[j]->material != DrawQuad::RENDER_PASS)
        continue;
      const RenderPassDrawQuad* render_pass_quad =
          RenderPassDrawQuad::MaterialCast(quad_list[j]);
      if (!render_pass_quad->filters.IsEmpty() ||
          !render_pass_quad->background_filters.IsEmpty())
        needs_filter_context_ = true;
    }
  }
  if (needs_filter_context_ && layer_tree_host())
    layer_tree_host()->set_needs_filter_context();

  SetNeedsCommit();
  // The active frame needs to be replaced and resources returned before the
  // commit is called complete.
  SetNextCommitWaitsForActivation();
}

void DelegatedRendererLayer::TakeUnusedResourcesForChildCompositor(
    ReturnedResourceArray* array) {
  DCHECK(array->empty());
  array->clear();

  array->swap(unused_resources_for_child_compositor_);
}

}  // namespace cc
