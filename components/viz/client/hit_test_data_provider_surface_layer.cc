// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "hit_test_data_provider_surface_layer.h"

#include "base/containers/adapters.h"
#include "cc/layers/surface_layer_impl.h"
#include "cc/trees/layer_tree_host_impl.h"
#include "cc/trees/layer_tree_impl.h"

namespace {

uint32_t GetFlagsForSurfaceLayer(const cc::SurfaceLayerImpl* layer) {
  uint32_t flags = viz::mojom::kHitTestMouse | viz::mojom::kHitTestTouch;
  const auto& surface_id = layer->primary_surface_id();
  if (layer->is_clipped()) {
    flags |= viz::mojom::kHitTestAsk;
  }
  if (surface_id.local_surface_id().is_valid()) {
    flags |= viz::mojom::kHitTestChildSurface;
  } else {
    flags |= viz::mojom::kHitTestMine;
  }
  return flags;
}

viz::mojom::HitTestRegionPtr CreateHitTestRegion(
    const cc::LayerImpl* layer,
    uint32_t flags,
    const gfx::Rect& rect,
    const viz::SurfaceId& surface_id,
    float device_scale_factor) {
  auto hit_test_region = viz::mojom::HitTestRegion::New();
  hit_test_region->frame_sink_id = surface_id.frame_sink_id();
  hit_test_region->flags = flags;

  hit_test_region->rect = rect;
  // The transform of hit test region maps a point from parent hit test region
  // to the local space. This is the inverse of screen space transform. Because
  // hit test query wants the point in target to be in Pixel space, we
  // counterscale the transform here. Note that the rect is scaled by dsf, so
  // the point and the rect are still in the same space.
  gfx::Transform surface_to_root_transform = layer->ScreenSpaceTransform();
  surface_to_root_transform.Scale(SK_MScalar1 / device_scale_factor,
                                  SK_MScalar1 / device_scale_factor);
  // TODO(sunxd): Avoid losing precision by not using inverse if possible.
  if (!surface_to_root_transform.GetInverse(&(hit_test_region->transform)))
    hit_test_region->transform = gfx::Transform();
  return hit_test_region;
}

}  // namespace

namespace viz {

// TODO(sunxd): Remove the compositor_frame parameter in base class when surface
// layer hit testing is enabled.
mojom::HitTestRegionListPtr HitTestDataProviderSurfaceLayer::GetHitTestData(
    const CompositorFrame& compositor_frame) const {
  DCHECK(host_impl_);
  float device_scale_factor = host_impl_->active_tree()->device_scale_factor();
  auto hit_test_region_list = mojom::HitTestRegionList::New();
  hit_test_region_list->flags =
      mojom::kHitTestMine | mojom::kHitTestMouse | mojom::kHitTestTouch;
  hit_test_region_list->bounds = host_impl_->DeviceViewport();
  hit_test_region_list->transform = host_impl_->DrawTransform();

  cc::Region overlapping_region;
  for (const auto* layer : base::Reversed(*host_impl_->active_tree())) {
    if (!layer->should_hit_test())
      continue;

    if (layer->is_surface_layer()) {
      const auto* surface_layer =
          static_cast<const cc::SurfaceLayerImpl*>(layer);

      if (!surface_layer->hit_testable()) {
        overlapping_region.Union(cc::MathUtil::MapEnclosingClippedRect(
            layer->ScreenSpaceTransform(), gfx::Rect(surface_layer->bounds())));
        continue;
      }

      gfx::Rect content_rect(
          gfx::ScaleToEnclosingRect(gfx::Rect(surface_layer->bounds()),
                                    device_scale_factor, device_scale_factor));

      gfx::Rect layer_screen_space_rect = cc::MathUtil::MapEnclosingClippedRect(
          surface_layer->ScreenSpaceTransform(),
          gfx::Rect(surface_layer->bounds()));
      if (overlapping_region.Contains(layer_screen_space_rect))
        continue;

      auto flag = GetFlagsForSurfaceLayer(surface_layer);
      if (overlapping_region.Intersects(layer_screen_space_rect))
        flag |= mojom::kHitTestAsk;
      auto surface_id = surface_layer->primary_surface_id();
      hit_test_region_list->regions.push_back(CreateHitTestRegion(
          layer, flag, content_rect, surface_id, device_scale_factor));
      continue;
    }
    // TODO(sunxd): Submit all overlapping layer bounds as hit test regions to
    // viz.
    overlapping_region.Union(cc::MathUtil::MapEnclosingClippedRect(
        layer->ScreenSpaceTransform(), gfx::Rect(layer->bounds())));
  }

  return hit_test_region_list;
}

void HitTestDataProviderSurfaceLayer::UpdateLayerTreeHostImpl(
    const cc::LayerTreeHostImpl* host_impl) {
  host_impl_ = host_impl;
}

}  // namespace viz
