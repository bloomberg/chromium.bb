// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/delegated_renderer_layer.h"

#include "cc/layers/delegated_renderer_layer_impl.h"
#include "cc/output/delegated_frame_data.h"
#include "cc/quads/render_pass_draw_quad.h"
#include "cc/trees/blocking_task_runner.h"
#include "cc/trees/layer_tree_host.h"

namespace cc {

scoped_refptr<DelegatedRendererLayer> DelegatedRendererLayer::Create(
    const scoped_refptr<DelegatedFrameProvider>& frame_provider) {
  return scoped_refptr<DelegatedRendererLayer>(
      new DelegatedRendererLayer(frame_provider));
}

DelegatedRendererLayer::DelegatedRendererLayer(
    const scoped_refptr<DelegatedFrameProvider>& frame_provider)
    : Layer(),
      frame_provider_(frame_provider),
      should_collect_new_frame_(true),
      frame_data_(NULL),
      main_thread_runner_(BlockingTaskRunner::current()),
      weak_ptrs_(this) {
  frame_provider_->AddObserver(this);
}

DelegatedRendererLayer::~DelegatedRendererLayer() {
  frame_provider_->RemoveObserver(this);
}

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
    // There is no active frame in the new layer tree host to wait for so no
    // need to call SetNextCommitWaitsForActivation().
    should_collect_new_frame_ = true;
    SetNeedsUpdate();
  }

  Layer::SetLayerTreeHost(host);
}

void DelegatedRendererLayer::PushPropertiesTo(LayerImpl* impl) {
  Layer::PushPropertiesTo(impl);

  DelegatedRendererLayerImpl* delegated_impl =
      static_cast<DelegatedRendererLayerImpl*>(impl);

  delegated_impl->SetDisplaySize(display_size_);

  delegated_impl->CreateChildIdIfNeeded(
      frame_provider_->GetReturnResourcesCallbackForImplThread());

  if (frame_data_)
    delegated_impl->SetFrameData(frame_data_, frame_damage_);
  frame_data_ = NULL;
  frame_damage_ = gfx::RectF();
}

void DelegatedRendererLayer::ProviderHasNewFrame() {
  should_collect_new_frame_ = true;
  SetNeedsUpdate();
  // The active frame needs to be replaced and resources returned before the
  // commit is called complete.
  SetNextCommitWaitsForActivation();
}

void DelegatedRendererLayer::SetDisplaySize(const gfx::Size& size) {
  if (display_size_ == size)
    return;
  display_size_ = size;
  SetNeedsCommit();
}

static bool FrameDataRequiresFilterContext(const DelegatedFrameData* frame) {
  for (size_t i = 0; i < frame->render_pass_list.size(); ++i) {
    const QuadList& quad_list = frame->render_pass_list[i]->quad_list;
    for (size_t j = 0; j < quad_list.size(); ++j) {
      if (quad_list[j]->shared_quad_state->blend_mode !=
          SkXfermode::kSrcOver_Mode)
        return true;
      if (quad_list[j]->material != DrawQuad::RENDER_PASS)
        continue;
      const RenderPassDrawQuad* render_pass_quad =
          RenderPassDrawQuad::MaterialCast(quad_list[j]);
      if (!render_pass_quad->filters.IsEmpty() ||
          !render_pass_quad->background_filters.IsEmpty())
        return true;
    }
  }
  return false;
}

bool DelegatedRendererLayer::Update(ResourceUpdateQueue* queue,
                                    const OcclusionTracker<Layer>* occlusion) {
  bool updated = Layer::Update(queue, occlusion);
  if (!should_collect_new_frame_)
    return updated;

  frame_data_ =
      frame_provider_->GetFrameDataAndRefResources(this, &frame_damage_);
  should_collect_new_frame_ = false;

  // If any quad has a filter operation or a blend mode other than normal,
  // then we need an offscreen context to draw this layer's content.
  if (FrameDataRequiresFilterContext(frame_data_))
    layer_tree_host()->set_needs_filter_context();

  SetNeedsPushProperties();
  return true;
}

}  // namespace cc
