// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/picture_layer.h"

#include "cc/debug/benchmark_instrumentation.h"
#include "cc/debug/devtools_instrumentation.h"
#include "cc/layers/picture_layer_impl.h"
#include "cc/trees/layer_tree_impl.h"
#include "ui/gfx/rect_conversions.h"

namespace cc {

scoped_refptr<PictureLayer> PictureLayer::Create(ContentLayerClient* client) {
  return make_scoped_refptr(new PictureLayer(client));
}

PictureLayer::PictureLayer(ContentLayerClient* client)
  : client_(client),
    pile_(make_scoped_refptr(new PicturePile())),
    instrumentation_object_tracker_(id()),
    is_mask_(false) {
}

PictureLayer::~PictureLayer() {
}

bool PictureLayer::DrawsContent() const {
  return Layer::DrawsContent() && client_;
}

scoped_ptr<LayerImpl> PictureLayer::CreateLayerImpl(LayerTreeImpl* tree_impl) {
  return PictureLayerImpl::Create(tree_impl, id()).PassAs<LayerImpl>();
}

void PictureLayer::PushPropertiesTo(LayerImpl* base_layer) {
  Layer::PushPropertiesTo(base_layer);
  PictureLayerImpl* layer_impl = static_cast<PictureLayerImpl*>(base_layer);

  layer_impl->SetIsMask(is_mask_);
  // Unlike other properties, invalidation must always be set on layer_impl.
  // See PictureLayerImpl::PushPropertiesTo for more details.
  layer_impl->invalidation_.Clear();
  layer_impl->invalidation_.Swap(&pile_invalidation_);
  layer_impl->pile_ = PicturePileImpl::CreateFromOther(pile_.get());
}

void PictureLayer::SetLayerTreeHost(LayerTreeHost* host) {
  Layer::SetLayerTreeHost(host);
  if (host) {
    pile_->SetMinContentsScale(host->settings().minimum_contents_scale);
    pile_->SetTileGridSize(host->settings().default_tile_size);
    pile_->set_num_raster_threads(host->settings().num_raster_threads);
    pile_->set_slow_down_raster_scale_factor(
        host->debug_state().slow_down_raster_scale_factor);
    pile_->set_show_debug_picture_borders(
        host->debug_state().show_picture_borders);
  }
}

void PictureLayer::SetNeedsDisplayRect(const gfx::RectF& layer_rect) {
  gfx::Rect rect = gfx::ToEnclosedRect(layer_rect);
  if (!rect.IsEmpty()) {
    // Clamp invalidation to the layer bounds.
    rect.Intersect(gfx::Rect(bounds()));
    pending_invalidation_.Union(rect);
  }
  Layer::SetNeedsDisplayRect(layer_rect);
}

bool PictureLayer::Update(ResourceUpdateQueue* queue,
                          const OcclusionTracker* occlusion) {
  // Do not early-out of this function so that PicturePile::Update has a chance
  // to record pictures due to changing visibility of this layer.

  TRACE_EVENT1(benchmark_instrumentation::kCategory,
               benchmark_instrumentation::kPictureLayerUpdate,
               benchmark_instrumentation::kSourceFrameNumber,
               layer_tree_host()->source_frame_number());

  bool updated = Layer::Update(queue, occlusion);

  pile_->Resize(paint_properties().bounds);

  // Calling paint in WebKit can sometimes cause invalidations, so save
  // off the invalidation prior to calling update.
  pending_invalidation_.Swap(&pile_invalidation_);
  pending_invalidation_.Clear();

  gfx::Rect visible_layer_rect = gfx::ScaleToEnclosingRect(
      visible_content_rect(), 1.f / contents_scale_x());
  if (layer_tree_host()->settings().using_synchronous_renderer_compositor) {
    // Workaround for http://crbug.com/235910 - to retain backwards compat
    // the full page content must always be provided in the picture layer.
    visible_layer_rect = gfx::Rect(bounds());
  }
  devtools_instrumentation::ScopedLayerTask paint_layer(
      devtools_instrumentation::kPaintLayer, id());
  updated |= pile_->Update(client_,
                           SafeOpaqueBackgroundColor(),
                           contents_opaque(),
                           pile_invalidation_,
                           visible_layer_rect,
                           rendering_stats_instrumentation());
  if (updated) {
    SetNeedsPushProperties();
  } else {
    // If this invalidation did not affect the pile, then it can be cleared as
    // an optimization.
    pile_invalidation_.Clear();
  }

  return updated;
}

void PictureLayer::SetIsMask(bool is_mask) {
  is_mask_ = is_mask;
}

bool PictureLayer::SupportsLCDText() const {
  return true;
}

}  // namespace cc
