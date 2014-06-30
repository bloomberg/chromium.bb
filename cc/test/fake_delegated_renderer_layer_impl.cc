// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/fake_delegated_renderer_layer_impl.h"

#include "base/bind.h"
#include "cc/output/delegated_frame_data.h"
#include "cc/quads/draw_quad.h"
#include "cc/resources/returned_resource.h"
#include "cc/trees/layer_tree_impl.h"

namespace cc {

FakeDelegatedRendererLayerImpl::FakeDelegatedRendererLayerImpl(
    LayerTreeImpl* tree_impl,
    int id)
    : DelegatedRendererLayerImpl(tree_impl, id) {
}

FakeDelegatedRendererLayerImpl::~FakeDelegatedRendererLayerImpl() {}

scoped_ptr<LayerImpl> FakeDelegatedRendererLayerImpl::CreateLayerImpl(
    LayerTreeImpl* tree_impl) {
  return FakeDelegatedRendererLayerImpl::Create(
      tree_impl, id()).PassAs<LayerImpl>();
}

static ResourceProvider::ResourceId AddResourceToFrame(
    ResourceProvider* resource_provider,
    DelegatedFrameData* frame,
    ResourceProvider::ResourceId resource_id) {
  TransferableResource resource;
  resource.id = resource_id;
  resource.mailbox_holder.texture_target =
      resource_provider->TargetForTesting(resource_id);
  frame->resource_list.push_back(resource);
  return resource_id;
}

ResourceProvider::ResourceIdSet FakeDelegatedRendererLayerImpl::Resources()
    const {
  ResourceProvider::ResourceIdSet set;
  ResourceProvider::ResourceIdArray array;
  array = ResourcesForTesting();
  for (size_t i = 0; i < array.size(); ++i)
    set.insert(array[i]);
  return set;
}

void NoopReturnCallback(const ReturnedResourceArray& returned) {}

void FakeDelegatedRendererLayerImpl::SetFrameDataForRenderPasses(
    float device_scale_factor,
    RenderPassList* pass_list) {
  scoped_ptr<DelegatedFrameData> delegated_frame(new DelegatedFrameData);
  delegated_frame->device_scale_factor = device_scale_factor;
  delegated_frame->render_pass_list.swap(*pass_list);

  ResourceProvider* resource_provider = layer_tree_impl()->resource_provider();

  DrawQuad::ResourceIteratorCallback add_resource_to_frame_callback =
      base::Bind(&AddResourceToFrame, resource_provider, delegated_frame.get());
  for (size_t i = 0; i < delegated_frame->render_pass_list.size(); ++i) {
    RenderPass* pass = delegated_frame->render_pass_list[i];
    for (size_t j = 0; j < pass->quad_list.size(); ++j)
      pass->quad_list[j]->IterateResources(add_resource_to_frame_callback);
  }

  CreateChildIdIfNeeded(base::Bind(&NoopReturnCallback));
  SetFrameData(delegated_frame.get(), gfx::RectF());
}

}  // namespace cc
