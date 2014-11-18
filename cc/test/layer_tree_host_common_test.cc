// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/layer_tree_host_common_test.h"

#include "cc/layers/layer.h"
#include "cc/layers/layer_impl.h"
#include "cc/test/fake_layer_tree_host.h"
#include "cc/trees/layer_tree_host_common.h"

namespace cc {

LayerTreeHostCommonTestBase::LayerTreeHostCommonTestBase()
    : client_(FakeLayerTreeHostClient::DIRECT_3D),
      render_surface_layer_list_count_(0) {
}

LayerTreeHostCommonTestBase::~LayerTreeHostCommonTestBase() {
}

void LayerTreeHostCommonTestBase::SetLayerPropertiesForTesting(
    Layer* layer,
    const gfx::Transform& transform,
    const gfx::Point3F& transform_origin,
    const gfx::PointF& position,
    const gfx::Size& bounds,
    bool flatten_transform,
    bool is_3d_sorted) {
  SetLayerPropertiesForTestingInternal<Layer>(layer,
                                              transform,
                                              transform_origin,
                                              position,
                                              bounds,
                                              flatten_transform,
                                              is_3d_sorted);
}

void LayerTreeHostCommonTestBase::SetLayerPropertiesForTesting(
    LayerImpl* layer,
    const gfx::Transform& transform,
    const gfx::Point3F& transform_origin,
    const gfx::PointF& position,
    const gfx::Size& bounds,
    bool flatten_transform,
    bool is_3d_sorted) {
  SetLayerPropertiesForTestingInternal<LayerImpl>(layer,
                                                  transform,
                                                  transform_origin,
                                                  position,
                                                  bounds,
                                                  flatten_transform,
                                                  is_3d_sorted);
  layer->SetContentBounds(bounds);
}

void LayerTreeHostCommonTestBase::ExecuteCalculateDrawProperties(
    Layer* root_layer,
    float device_scale_factor,
    float page_scale_factor,
    Layer* page_scale_application_layer,
    bool can_use_lcd_text) {
  EXPECT_TRUE(page_scale_application_layer || (page_scale_factor == 1.f));
  gfx::Transform identity_matrix;
  gfx::Size device_viewport_size =
      gfx::Size(root_layer->bounds().width() * device_scale_factor,
                root_layer->bounds().height() * device_scale_factor);

  render_surface_layer_list_.reset(new RenderSurfaceLayerList);

  // We are probably not testing what is intended if the root_layer bounds are
  // empty.
  DCHECK(!root_layer->bounds().IsEmpty());
  LayerTreeHostCommon::CalcDrawPropsMainInputsForTesting inputs(
      root_layer, device_viewport_size, render_surface_layer_list_.get());
  inputs.device_scale_factor = device_scale_factor;
  inputs.page_scale_factor = page_scale_factor;
  inputs.page_scale_application_layer = page_scale_application_layer;
  inputs.can_use_lcd_text = can_use_lcd_text;
  inputs.can_adjust_raster_scales = true;
  LayerTreeHostCommon::CalculateDrawProperties(&inputs);
}

void LayerTreeHostCommonTestBase::ExecuteCalculateDrawProperties(
    LayerImpl* root_layer,
    float device_scale_factor,
    float page_scale_factor,
    LayerImpl* page_scale_application_layer,
    bool can_use_lcd_text) {
  gfx::Transform identity_matrix;
  gfx::Size device_viewport_size =
      gfx::Size(root_layer->bounds().width() * device_scale_factor,
                root_layer->bounds().height() * device_scale_factor);

  render_surface_layer_list_impl_.reset(new LayerImplList);

  // We are probably not testing what is intended if the root_layer bounds are
  // empty.
  DCHECK(!root_layer->bounds().IsEmpty());
  LayerTreeHostCommon::CalcDrawPropsImplInputsForTesting inputs(
      root_layer, device_viewport_size, render_surface_layer_list_impl_.get());
  inputs.device_scale_factor = device_scale_factor;
  inputs.page_scale_factor = page_scale_factor;
  inputs.page_scale_application_layer = page_scale_application_layer;
  inputs.can_use_lcd_text = can_use_lcd_text;
  inputs.can_adjust_raster_scales = true;

  ++render_surface_layer_list_count_;
  inputs.current_render_surface_layer_list_id =
      render_surface_layer_list_count_;

  LayerTreeHostCommon::CalculateDrawProperties(&inputs);
}

scoped_ptr<FakeLayerTreeHost>
LayerTreeHostCommonTestBase::CreateFakeLayerTreeHost() {
  return FakeLayerTreeHost::Create(&client_);
}

}  // namespace cc
