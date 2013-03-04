// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/fake_delegated_renderer_layer_impl.h"

#include "base/bind.h"
#include "cc/delegated_frame_data.h"
#include "cc/draw_quad.h"

namespace cc {

FakeDelegatedRendererLayerImpl::FakeDelegatedRendererLayerImpl(
    LayerTreeImpl* tree_impl, int id)
    : DelegatedRendererLayerImpl(tree_impl, id) {}

FakeDelegatedRendererLayerImpl::~FakeDelegatedRendererLayerImpl() {}

static ResourceProvider::ResourceId AddResourceToFrame(
    DelegatedFrameData* frame,
    ResourceProvider::ResourceId resource_id) {
  TransferableResource resource;
  resource.id = resource_id;
  frame->resource_list.push_back(resource);
  return resource_id;
}

void FakeDelegatedRendererLayerImpl::SetFrameDataForRenderPasses(
    ScopedPtrVector<RenderPass>* pass_list) {
  scoped_ptr<DelegatedFrameData> delegated_frame(new DelegatedFrameData);
  delegated_frame->render_pass_list.swap(*pass_list);

  DrawQuad::ResourceIteratorCallback add_resource_to_frame_callback =
      base::Bind(&AddResourceToFrame,
                 delegated_frame.get());
  for (size_t i = 0; i < delegated_frame->render_pass_list.size(); ++i) {
    RenderPass* pass = delegated_frame->render_pass_list[i];
    for (size_t j = 0; j < pass->quad_list.size(); ++j)
      pass->quad_list[j]->IterateResources(add_resource_to_frame_callback);
  }

  TransferableResourceArray resources_for_ack;
  SetFrameData(delegated_frame.Pass(), gfx::RectF(), &resources_for_ack);
}

}  // namespace cc
