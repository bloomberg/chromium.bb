// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_DELEGATED_RENDERER_LAYER_IMPL_H_
#define CC_DELEGATED_RENDERER_LAYER_IMPL_H_

#include "base/memory/scoped_ptr.h"
#include "cc/cc_export.h"
#include "cc/layer_impl.h"
#include "cc/scoped_ptr_vector.h"

namespace cc {

class CC_EXPORT DelegatedRendererLayerImpl : public LayerImpl {
 public:
  static scoped_ptr<DelegatedRendererLayerImpl> Create(
      LayerTreeImpl* tree_impl, int id) {
    return make_scoped_ptr(new DelegatedRendererLayerImpl(tree_impl, id));
  }
  virtual ~DelegatedRendererLayerImpl();

  // LayerImpl overrides.
  virtual scoped_ptr<LayerImpl> createLayerImpl(LayerTreeImpl*) OVERRIDE;
  virtual bool hasDelegatedContent() const OVERRIDE;
  virtual bool hasContributingDelegatedRenderPasses() const OVERRIDE;
  virtual RenderPass::Id firstContributingRenderPassId() const OVERRIDE;
  virtual RenderPass::Id nextContributingRenderPassId(
      RenderPass::Id previous) const OVERRIDE;
  virtual void didLoseOutputSurface() OVERRIDE;
  virtual void appendQuads(
      QuadSink& quad_sink, AppendQuadsData& append_quads_data) OVERRIDE;

  // This gives ownership of the RenderPasses to the layer.
  void SetRenderPasses(ScopedPtrVector<RenderPass>&);
  void ClearRenderPasses();

  // Set the size at which the frame should be displayed, with the origin at the
  // layer's origin. This must always contain at least the layer's bounds. A
  // value of (0, 0) implies that the frame should be displayed to fit exactly
  // in the layer's bounds.
  void set_display_size(gfx::Size size) { display_size_ = size; }

  void AppendContributingRenderPasses(RenderPassSink* render_pass_sink);

  // Creates an ID with the resource provider for the child renderer
  // that will be sending quads to the layer.
  void CreateChildIdIfNeeded();
  int child_id() const { return child_id_; }

 private:
  DelegatedRendererLayerImpl(LayerTreeImpl* tree_impl, int id);

  void ClearChildId();

  RenderPass::Id ConvertDelegatedRenderPassId(
      RenderPass::Id delegated_render_pass_id) const;

  void AppendRenderPassQuads(
      QuadSink* quad_sink,
      AppendQuadsData* append_quads_data,
      const RenderPass* delegated_render_pass,
      gfx::Size frame_size) const;

  // LayerImpl overrides.
  virtual const char* layerTypeAsString() const OVERRIDE;

  ScopedPtrVector<RenderPass> render_passes_in_draw_order_;
  base::hash_map<RenderPass::Id, int> render_passes_index_by_id_;
  gfx::Size display_size_;
  int child_id_;
};

}

#endif  // CC_DELEGATED_RENDERER_LAYER_IMPL_H_
