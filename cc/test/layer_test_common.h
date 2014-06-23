// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_LAYER_TEST_COMMON_H_
#define CC_TEST_LAYER_TEST_COMMON_H_

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "cc/quads/render_pass.h"
#include "cc/test/fake_layer_tree_host.h"
#include "cc/test/mock_occlusion_tracker.h"
#include "cc/trees/layer_tree_host_impl.h"

#define EXPECT_SET_NEEDS_COMMIT(expect, code_to_test)                 \
  do {                                                                \
    EXPECT_CALL(*layer_tree_host_, SetNeedsCommit()).Times((expect)); \
    code_to_test;                                                     \
    Mock::VerifyAndClearExpectations(layer_tree_host_.get());         \
  } while (false)

#define EXPECT_SET_NEEDS_UPDATE(expect, code_to_test)                       \
  do {                                                                      \
    EXPECT_CALL(*layer_tree_host_, SetNeedsUpdateLayers()).Times((expect)); \
    code_to_test;                                                           \
    Mock::VerifyAndClearExpectations(layer_tree_host_.get());               \
  } while (false)

namespace gfx { class Rect; }

namespace cc {
class LayerImpl;
class OutputSurface;
class QuadList;
class RenderSurfaceImpl;
class ResourceProvider;

class LayerTestCommon {
 public:
  static const char* quad_string;

  static void VerifyQuadsExactlyCoverRect(const QuadList& quads,
                                          const gfx::Rect& rect);

  static void VerifyQuadsCoverRectWithOcclusion(
      const QuadList& quads,
      const gfx::Rect& rect,
      const gfx::Rect& occluded,
      size_t* partially_occluded_count);

  class LayerImplTest {
   public:
    LayerImplTest();
    ~LayerImplTest();

    template <typename T>
    T* AddChildToRoot() {
      scoped_ptr<T> layer = T::Create(host_->host_impl()->active_tree(), 2);
      T* ptr = layer.get();
      root_layer_impl_->AddChild(layer.template PassAs<LayerImpl>());
      return ptr;
    }

    template <typename T, typename A>
    T* AddChildToRoot(const A& a) {
      scoped_ptr<T> layer = T::Create(host_->host_impl()->active_tree(), 2, a);
      T* ptr = layer.get();
      root_layer_impl_->AddChild(layer.template PassAs<LayerImpl>());
      return ptr;
    }

    template <typename T, typename A, typename B, typename C, typename D>
    T* AddChildToRoot(const A& a, const B& b, const C& c, const D& d) {
      scoped_ptr<T> layer =
          T::Create(host_->host_impl()->active_tree(), 2, a, b, c, d);
      T* ptr = layer.get();
      root_layer_impl_->AddChild(layer.template PassAs<LayerImpl>());
      return ptr;
    }

    template <typename T,
              typename A,
              typename B,
              typename C,
              typename D,
              typename E>
    T* AddChildToRoot(const A& a,
                      const B& b,
                      const C& c,
                      const D& d,
                      const E& e) {
      scoped_ptr<T> layer =
          T::Create(host_->host_impl()->active_tree(), 2, a, b, c, d, e);
      T* ptr = layer.get();
      root_layer_impl_->AddChild(layer.template PassAs<LayerImpl>());
      return ptr;
    }

    void CalcDrawProps(const gfx::Size& viewport_size);
    void AppendQuadsWithOcclusion(LayerImpl* layer_impl,
                                  const gfx::Rect& occluded);
    void AppendQuadsForPassWithOcclusion(LayerImpl* layer_impl,
                                         const RenderPass::Id& id,
                                         const gfx::Rect& occluded);
    void AppendSurfaceQuadsWithOcclusion(RenderSurfaceImpl* surface_impl,
                                         const gfx::Rect& occluded);

    OutputSurface* output_surface() const {
      return host_->host_impl()->output_surface();
    }
    ResourceProvider* resource_provider() const {
      return host_->host_impl()->resource_provider();
    }
    LayerImpl* root_layer() const { return root_layer_impl_.get(); }
    FakeLayerTreeHostImpl* host_impl() const { return host_->host_impl(); }
    Proxy* proxy() const { return host_->host_impl()->proxy(); }
    const QuadList& quad_list() const { return render_pass_->quad_list; }

   private:
    scoped_ptr<FakeLayerTreeHost> host_;
    scoped_ptr<LayerImpl> root_layer_impl_;
    scoped_ptr<RenderPass> render_pass_;
    MockOcclusionTracker<LayerImpl> occlusion_tracker_;
  };
};

}  // namespace cc

#endif  // CC_TEST_LAYER_TEST_COMMON_H_
