// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_LAYER_TEST_COMMON_H_
#define CC_TEST_LAYER_TEST_COMMON_H_

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "cc/test/fake_layer_tree_host.h"
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
class QuadList;

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

    void CalcDrawProps(const gfx::Size& viewport_size) {
      LayerImplList layer_list;
      LayerTreeHostCommon::CalcDrawPropsImplInputsForTesting inputs(
          root_layer_impl_.get(), viewport_size, &layer_list);
      LayerTreeHostCommon::CalculateDrawProperties(&inputs);
    }

   private:
    scoped_ptr<FakeLayerTreeHost> host_;
    scoped_ptr<LayerImpl> root_layer_impl_;
  };
};

}  // namespace cc

#endif  // CC_TEST_LAYER_TEST_COMMON_H_
