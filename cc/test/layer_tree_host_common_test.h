// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_LAYER_TREE_HOST_COMMON_TEST_H_
#define CC_TEST_LAYER_TREE_HOST_COMMON_TEST_H_

#include <vector>

#include "base/memory/scoped_ptr.h"
#include "cc/layers/layer_lists.h"
#include "cc/test/fake_layer_tree_host_client.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gfx {
class PointF;
class Point3F;
class Size;
class Transform;
}

namespace cc {

class FakeLayerTreeHost;
class Layer;
class LayerImpl;
class RenderSurfaceLayerList;

class LayerTreeHostCommonTestBase {
 protected:
  LayerTreeHostCommonTestBase();
  virtual ~LayerTreeHostCommonTestBase();

  template <typename LayerType>
  void SetLayerPropertiesForTestingInternal(
      LayerType* layer,
      const gfx::Transform& transform,
      const gfx::Point3F& transform_origin,
      const gfx::PointF& position,
      const gfx::Size& bounds,
      bool flatten_transform,
      bool is_3d_sorted) {
    layer->SetTransform(transform);
    layer->SetTransformOrigin(transform_origin);
    layer->SetPosition(position);
    layer->SetBounds(bounds);
    layer->SetShouldFlattenTransform(flatten_transform);
    layer->Set3dSortingContextId(is_3d_sorted ? 1 : 0);
  }

  void SetLayerPropertiesForTesting(Layer* layer,
                                    const gfx::Transform& transform,
                                    const gfx::Point3F& transform_origin,
                                    const gfx::PointF& position,
                                    const gfx::Size& bounds,
                                    bool flatten_transform,
                                    bool is_3d_sorted);

  void SetLayerPropertiesForTesting(LayerImpl* layer,
                                    const gfx::Transform& transform,
                                    const gfx::Point3F& transform_origin,
                                    const gfx::PointF& position,
                                    const gfx::Size& bounds,
                                    bool flatten_transform,
                                    bool is_3d_sorted);

  void ExecuteCalculateDrawProperties(Layer* root_layer,
                                      float device_scale_factor,
                                      float page_scale_factor,
                                      Layer* page_scale_application_layer,
                                      bool can_use_lcd_text);

  void ExecuteCalculateDrawProperties(LayerImpl* root_layer,
                                      float device_scale_factor,
                                      float page_scale_factor,
                                      LayerImpl* page_scale_application_layer,
                                      bool can_use_lcd_text);

  template <class LayerType>
  void ExecuteCalculateDrawProperties(LayerType* root_layer) {
    LayerType* page_scale_application_layer = NULL;
    ExecuteCalculateDrawProperties(
        root_layer, 1.f, 1.f, page_scale_application_layer, false);
  }

  template <class LayerType>
  void ExecuteCalculateDrawProperties(LayerType* root_layer,
                                      float device_scale_factor) {
    LayerType* page_scale_application_layer = NULL;
    ExecuteCalculateDrawProperties(root_layer,
                                   device_scale_factor,
                                   1.f,
                                   page_scale_application_layer,
                                   false);
  }

  template <class LayerType>
  void ExecuteCalculateDrawProperties(LayerType* root_layer,
                                      float device_scale_factor,
                                      float page_scale_factor,
                                      LayerType* page_scale_application_layer) {
    ExecuteCalculateDrawProperties(root_layer,
                                   device_scale_factor,
                                   page_scale_factor,
                                   page_scale_application_layer,
                                   false);
  }

  RenderSurfaceLayerList* render_surface_layer_list() const {
    return render_surface_layer_list_.get();
  }

  LayerImplList* render_surface_layer_list_impl() const {
    return render_surface_layer_list_impl_.get();
  }

  int render_surface_layer_list_count() const {
    return render_surface_layer_list_count_;
  }

  scoped_ptr<FakeLayerTreeHost> CreateFakeLayerTreeHost();

 private:
  scoped_ptr<RenderSurfaceLayerList> render_surface_layer_list_;
  scoped_ptr<std::vector<LayerImpl*> > render_surface_layer_list_impl_;

  FakeLayerTreeHostClient client_;
  int render_surface_layer_list_count_;
};

class LayerTreeHostCommonTest : public LayerTreeHostCommonTestBase,
                                public testing::Test {};

}  // namespace cc

#endif  // CC_TEST_LAYER_TREE_HOST_COMMON_TEST_H_
