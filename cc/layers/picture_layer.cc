// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/picture_layer.h"

#include "base/auto_reset.h"
#include "cc/layers/content_layer_client.h"
#include "cc/layers/picture_layer_impl.h"
#include "cc/trees/layer_tree_impl.h"
#include "third_party/skia/include/core/SkPictureRecorder.h"
#include "ui/gfx/geometry/rect_conversions.h"

namespace cc {

scoped_refptr<PictureLayer> PictureLayer::Create(ContentLayerClient* client) {
  return make_scoped_refptr(new PictureLayer(client));
}

PictureLayer::PictureLayer(ContentLayerClient* client)
    : client_(client),
      instrumentation_object_tracker_(id()),
      update_source_frame_number_(-1),
      can_use_lcd_text_last_frame_(can_use_lcd_text()) {
}

PictureLayer::~PictureLayer() {
}

scoped_ptr<LayerImpl> PictureLayer::CreateLayerImpl(LayerTreeImpl* tree_impl) {
  return PictureLayerImpl::Create(tree_impl, id());
}

void PictureLayer::PushPropertiesTo(LayerImpl* base_layer) {
  Layer::PushPropertiesTo(base_layer);
  PictureLayerImpl* layer_impl = static_cast<PictureLayerImpl*>(base_layer);

  int source_frame_number = layer_tree_host()->source_frame_number();
  gfx::Size impl_bounds = layer_impl->bounds();
  gfx::Size pile_bounds = pile_.tiling_size();

  // If update called, then pile size must match bounds pushed to impl layer.
  DCHECK_IMPLIES(update_source_frame_number_ == source_frame_number,
                 impl_bounds == pile_bounds)
      << " bounds " << impl_bounds.ToString() << " pile "
      << pile_bounds.ToString();

  if (update_source_frame_number_ != source_frame_number &&
      pile_bounds != impl_bounds) {
    // Update may not get called for the layer (if it's not in the viewport
    // for example, even though it has resized making the pile no longer
    // valid. In this case just destroy the pile.
    pile_.SetEmptyBounds();
  }

  // Unlike other properties, invalidation must always be set on layer_impl.
  // See PictureLayerImpl::PushPropertiesTo for more details.
  layer_impl->invalidation_.Clear();
  layer_impl->invalidation_.Swap(&pile_invalidation_);
  layer_impl->UpdatePile(PicturePileImpl::CreateFromOther(&pile_));
}

void PictureLayer::SetLayerTreeHost(LayerTreeHost* host) {
  Layer::SetLayerTreeHost(host);
  if (host) {
    pile_.SetMinContentsScale(host->settings().minimum_contents_scale);
    pile_.SetTileGridSize(host->settings().default_tile_grid_size);
    pile_.set_slow_down_raster_scale_factor(
        host->debug_state().slow_down_raster_scale_factor);
    pile_.set_show_debug_picture_borders(
        host->debug_state().show_picture_borders);
  }
}

void PictureLayer::SetNeedsDisplayRect(const gfx::Rect& layer_rect) {
  if (!layer_rect.IsEmpty()) {
    // Clamp invalidation to the layer bounds.
    pending_invalidation_.Union(
        gfx::IntersectRects(layer_rect, gfx::Rect(bounds())));
  }
  Layer::SetNeedsDisplayRect(layer_rect);
}

bool PictureLayer::Update(ResourceUpdateQueue* queue,
                          const OcclusionTracker<Layer>* occlusion) {
  update_source_frame_number_ = layer_tree_host()->source_frame_number();
  bool updated = Layer::Update(queue, occlusion);

  {
    base::AutoReset<bool> ignore_set_needs_commit(&ignore_set_needs_commit_,
                                                  true);
    UpdateCanUseLCDText();
  }

  gfx::Rect visible_layer_rect = gfx::ScaleToEnclosingRect(
      visible_content_rect(), 1.f / contents_scale_x());
  gfx::Size layer_size = paint_properties().bounds;

  if (last_updated_visible_content_rect_ == visible_content_rect() &&
      pile_.tiling_size() == layer_size && pending_invalidation_.IsEmpty()) {
    // Only early out if the visible content rect of this layer hasn't changed.
    return updated;
  }

  TRACE_EVENT1("cc", "PictureLayer::Update",
               "source_frame_number",
               layer_tree_host()->source_frame_number());
  devtools_instrumentation::ScopedLayerTreeTask update_layer(
      devtools_instrumentation::kUpdateLayer, id(), layer_tree_host()->id());

  // Calling paint in WebKit can sometimes cause invalidations, so save
  // off the invalidation prior to calling update.
  pending_invalidation_.Swap(&pile_invalidation_);
  pending_invalidation_.Clear();

  if (layer_tree_host()->settings().record_full_layer) {
    // Workaround for http://crbug.com/235910 - to retain backwards compat
    // the full page content must always be provided in the picture layer.
    visible_layer_rect = gfx::Rect(layer_size);
  }

  // UpdateAndExpandInvalidation will give us an invalidation that covers
  // anything not explicitly recorded in this frame. We give this region
  // to the impl side so that it drops tiles that may not have a recording
  // for them.
  DCHECK(client_);
  updated |=
      pile_.UpdateAndExpandInvalidation(client_,
                                        &pile_invalidation_,
                                        SafeOpaqueBackgroundColor(),
                                        contents_opaque(),
                                        client_->FillsBoundsCompletely(),
                                        layer_size,
                                        visible_layer_rect,
                                        update_source_frame_number_,
                                        Picture::RECORD_NORMALLY,
                                        rendering_stats_instrumentation());
  last_updated_visible_content_rect_ = visible_content_rect();

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
  pile_.set_is_mask(is_mask);
}

bool PictureLayer::SupportsLCDText() const {
  return true;
}

void PictureLayer::UpdateCanUseLCDText() {
  if (can_use_lcd_text_last_frame_ == can_use_lcd_text())
    return;

  can_use_lcd_text_last_frame_ = can_use_lcd_text();
  if (client_)
    client_->DidChangeLayerCanUseLCDText();
}

skia::RefPtr<SkPicture> PictureLayer::GetPicture() const {
  // We could either flatten the PicturePile into a single SkPicture,
  // or paint a fresh one depending on what we intend to do with the
  // picture. For now we just paint a fresh one to get consistent results.
  if (!DrawsContent())
    return skia::RefPtr<SkPicture>();

  int width = bounds().width();
  int height = bounds().height();

  SkPictureRecorder recorder;
  SkCanvas* canvas = recorder.beginRecording(width, height, nullptr, 0);
  client_->PaintContents(canvas,
                         gfx::Rect(width, height),
                         ContentLayerClient::GRAPHICS_CONTEXT_ENABLED);
  skia::RefPtr<SkPicture> picture = skia::AdoptRef(recorder.endRecording());
  return picture;
}

bool PictureLayer::IsSuitableForGpuRasterization() const {
  return pile_.is_suitable_for_gpu_rasterization();
}

void PictureLayer::ClearClient() {
  client_ = nullptr;
  UpdateDrawsContent(HasDrawableContent());
}

bool PictureLayer::HasDrawableContent() const {
  return client_ && Layer::HasDrawableContent();
}

void PictureLayer::RunMicroBenchmark(MicroBenchmark* benchmark) {
  benchmark->RunOnLayer(this);
}

}  // namespace cc
