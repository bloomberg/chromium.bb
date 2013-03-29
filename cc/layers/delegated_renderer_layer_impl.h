// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_LAYERS_DELEGATED_RENDERER_LAYER_IMPL_H_
#define CC_LAYERS_DELEGATED_RENDERER_LAYER_IMPL_H_

#include "base/memory/scoped_ptr.h"
#include "cc/base/cc_export.h"
#include "cc/base/scoped_ptr_vector.h"
#include "cc/layers/layer_impl.h"

namespace cc {
class DelegatedFrameData;
class RenderPassSink;

class CC_EXPORT DelegatedRendererLayerImpl : public LayerImpl {
 public:
  static scoped_ptr<DelegatedRendererLayerImpl> Create(
      LayerTreeImpl* tree_impl, int id) {
    return make_scoped_ptr(new DelegatedRendererLayerImpl(tree_impl, id));
  }
  virtual ~DelegatedRendererLayerImpl();

  // LayerImpl overrides.
  virtual scoped_ptr<LayerImpl> CreateLayerImpl(LayerTreeImpl* tree_impl)
      OVERRIDE;
  virtual bool HasDelegatedContent() const OVERRIDE;
  virtual bool HasContributingDelegatedRenderPasses() const OVERRIDE;
  virtual RenderPass::Id FirstContributingRenderPassId() const OVERRIDE;
  virtual RenderPass::Id NextContributingRenderPassId(
      RenderPass::Id previous) const OVERRIDE;
  virtual void DidLoseOutputSurface() OVERRIDE;
  virtual void AppendQuads(QuadSink* quad_sink,
                           AppendQuadsData* append_quads_data) OVERRIDE;

  void AppendContributingRenderPasses(RenderPassSink* render_pass_sink);

  void SetFrameData(scoped_ptr<DelegatedFrameData> frame_data,
                    gfx::RectF damage_in_frame,
                    TransferableResourceArray* resources_for_ack);

  void SetDisplaySize(gfx::Size size);

 protected:
  DelegatedRendererLayerImpl(LayerTreeImpl* tree_impl, int id);

  int ChildIdForTesting() const { return child_id_; }
  const ScopedPtrVector<RenderPass>& RenderPassesInDrawOrderForTesting() const {
    return render_passes_in_draw_order_;
  }
  const ResourceProvider::ResourceIdSet& ResourcesForTesting() const {
    return resources_;
  }

 private:
  // Creates an ID with the resource provider for the child renderer
  // that will be sending quads to the layer.
  void CreateChildIdIfNeeded();
  void ClearChildId();

  void AppendRainbowDebugBorder(QuadSink* quad_sink,
                                AppendQuadsData* append_quads_data);

  void SetRenderPasses(
      ScopedPtrVector<RenderPass>* render_passes_in_draw_order);
  void ClearRenderPasses();

  RenderPass::Id ConvertDelegatedRenderPassId(
      RenderPass::Id delegated_render_pass_id) const;

  gfx::Transform DelegatedFrameToLayerSpaceTransform(gfx::Size frame_size)
      const;

  void AppendRenderPassQuads(
      QuadSink* quad_sink,
      AppendQuadsData* append_quads_data,
      const RenderPass* delegated_render_pass,
      gfx::Size frame_size) const;

  // LayerImpl overrides.
  virtual const char* LayerTypeAsString() const OVERRIDE;

  ScopedPtrVector<RenderPass> render_passes_in_draw_order_;
  base::hash_map<RenderPass::Id, int> render_passes_index_by_id_;
  ResourceProvider::ResourceIdSet resources_;

  gfx::Size display_size_;
  int child_id_;

  DISALLOW_COPY_AND_ASSIGN(DelegatedRendererLayerImpl);
};

}  // namespace cc

#endif  // CC_LAYERS_DELEGATED_RENDERER_LAYER_IMPL_H_
