// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/picture_layer.h"

#include "base/auto_reset.h"
#include "base/trace_event/trace_event.h"
#include "cc/layers/content_layer_client.h"
#include "cc/layers/picture_layer_impl.h"
#include "cc/paint/paint_record.h"
#include "cc/playback/recording_source.h"
#include "cc/trees/layer_tree_host.h"
#include "cc/trees/layer_tree_impl.h"
#include "ui/gfx/geometry/rect_conversions.h"

namespace cc {

PictureLayer::PictureLayerInputs::PictureLayerInputs() = default;

PictureLayer::PictureLayerInputs::~PictureLayerInputs() = default;

scoped_refptr<PictureLayer> PictureLayer::Create(ContentLayerClient* client) {
  return make_scoped_refptr(new PictureLayer(client));
}

PictureLayer::PictureLayer(ContentLayerClient* client)
    : instrumentation_object_tracker_(id()),
      update_source_frame_number_(-1),
      mask_type_(LayerMaskType::NOT_MASK) {
  picture_layer_inputs_.client = client;
}

PictureLayer::PictureLayer(ContentLayerClient* client,
                           std::unique_ptr<RecordingSource> source)
    : PictureLayer(client) {
  recording_source_ = std::move(source);
}

PictureLayer::~PictureLayer() {
}

std::unique_ptr<LayerImpl> PictureLayer::CreateLayerImpl(
    LayerTreeImpl* tree_impl) {
  return PictureLayerImpl::Create(tree_impl, id(), mask_type_);
}

void PictureLayer::PushPropertiesTo(LayerImpl* base_layer) {
  Layer::PushPropertiesTo(base_layer);
  TRACE_EVENT0("cc", "PictureLayer::PushPropertiesTo");
  PictureLayerImpl* layer_impl = static_cast<PictureLayerImpl*>(base_layer);
  // TODO(danakj): Make mask_type_ a constructor parameter for PictureLayer.
  DCHECK_EQ(layer_impl->mask_type(), mask_type());
  DropRecordingSourceContentIfInvalid();

  layer_impl->SetNearestNeighbor(picture_layer_inputs_.nearest_neighbor);

  // Preserve lcd text settings from the current raster source.
  bool can_use_lcd_text = layer_impl->RasterSourceUsesLCDText();
  scoped_refptr<RasterSource> raster_source =
      recording_source_->CreateRasterSource(can_use_lcd_text);
  layer_impl->set_gpu_raster_max_texture_size(
      layer_tree_host()->device_viewport_size());
  layer_impl->UpdateRasterSource(raster_source, &last_updated_invalidation_,
                                 nullptr);
  DCHECK(last_updated_invalidation_.IsEmpty());
}

void PictureLayer::SetLayerTreeHost(LayerTreeHost* host) {
  Layer::SetLayerTreeHost(host);

  if (!host)
    return;

  if (!host->GetSettings().enable_mask_tiling &&
      mask_type_ == LayerMaskType::MULTI_TEXTURE_MASK)
    mask_type_ = LayerMaskType::SINGLE_TEXTURE_MASK;

  if (!recording_source_)
    recording_source_.reset(new RecordingSource);
  recording_source_->SetSlowdownRasterScaleFactor(
      host->GetDebugState().slow_down_raster_scale_factor);
  // If we need to enable image decode tasks, then we have to generate the
  // discardable images metadata.
  const LayerTreeSettings& settings = layer_tree_host()->GetSettings();
  recording_source_->SetGenerateDiscardableImagesMetadata(
      settings.image_decode_tasks_enabled);
}

void PictureLayer::SetNeedsDisplayRect(const gfx::Rect& layer_rect) {
  DCHECK(!layer_tree_host() || !layer_tree_host()->in_paint_layer_contents());
  if (recording_source_)
    recording_source_->SetNeedsDisplayRect(layer_rect);
  Layer::SetNeedsDisplayRect(layer_rect);
}

bool PictureLayer::Update() {
  update_source_frame_number_ = layer_tree_host()->SourceFrameNumber();
  bool updated = Layer::Update();

  gfx::Size layer_size = paint_properties().bounds;

  recording_source_->SetBackgroundColor(SafeOpaqueBackgroundColor());
  recording_source_->SetRequiresClear(
      !contents_opaque() &&
      !picture_layer_inputs_.client->FillsBoundsCompletely());

  TRACE_EVENT1("cc", "PictureLayer::Update", "source_frame_number",
               layer_tree_host()->SourceFrameNumber());
  devtools_instrumentation::ScopedLayerTreeTask update_layer(
      devtools_instrumentation::kUpdateLayer, id(), layer_tree_host()->GetId());

  // UpdateAndExpandInvalidation will give us an invalidation that covers
  // anything not explicitly recorded in this frame. We give this region
  // to the impl side so that it drops tiles that may not have a recording
  // for them.
  DCHECK(picture_layer_inputs_.client);

  picture_layer_inputs_.recorded_viewport =
      picture_layer_inputs_.client->PaintableRegion();

  updated |= recording_source_->UpdateAndExpandInvalidation(
      &last_updated_invalidation_, layer_size,
      picture_layer_inputs_.recorded_viewport);

  if (updated) {
    picture_layer_inputs_.display_list =
        picture_layer_inputs_.client->PaintContentsToDisplayList(
            ContentLayerClient::PAINTING_BEHAVIOR_NORMAL);
    picture_layer_inputs_.painter_reported_memory_usage =
        picture_layer_inputs_.client->GetApproximateUnsharedMemoryUsage();
    recording_source_->UpdateDisplayItemList(
        picture_layer_inputs_.display_list,
        picture_layer_inputs_.painter_reported_memory_usage);
    SetNeedsPushProperties();
  } else {
    // If this invalidation did not affect the recording source, then it can be
    // cleared as an optimization.
    last_updated_invalidation_.Clear();
  }

  return updated;
}

void PictureLayer::SetLayerMaskType(LayerMaskType mask_type) {
  mask_type_ = mask_type;
}

sk_sp<SkPicture> PictureLayer::GetPicture() const {
  // We could either flatten the RecordingSource into a single SkPicture, or
  // paint a fresh one depending on what we intend to do with it.  For now we
  // just paint a fresh one to get consistent results.
  if (!DrawsContent())
    return nullptr;

  gfx::Size layer_size = bounds();
  RecordingSource recording_source;
  Region recording_invalidation;

  gfx::Rect new_recorded_viewport =
      picture_layer_inputs_.client->PaintableRegion();
  scoped_refptr<DisplayItemList> display_list =
      picture_layer_inputs_.client->PaintContentsToDisplayList(
          ContentLayerClient::PAINTING_BEHAVIOR_NORMAL);
  size_t painter_reported_memory_usage =
      picture_layer_inputs_.client->GetApproximateUnsharedMemoryUsage();

  recording_source.UpdateAndExpandInvalidation(
      &recording_invalidation, layer_size, new_recorded_viewport);
  recording_source.UpdateDisplayItemList(display_list,
                                         painter_reported_memory_usage);

  scoped_refptr<RasterSource> raster_source =
      recording_source.CreateRasterSource(false);

  return raster_source->GetFlattenedPicture();
}

bool PictureLayer::IsSuitableForGpuRasterization() const {
  // The display list needs to be created (see: UpdateAndExpandInvalidation)
  // before checking for suitability. There are cases where an update will not
  // create a display list (e.g., if the size is empty). We return true in these
  // cases because the gpu suitability bit sticks false.
  return !picture_layer_inputs_.display_list ||
         picture_layer_inputs_.display_list->IsSuitableForGpuRasterization();
}

void PictureLayer::ClearClient() {
  picture_layer_inputs_.client = nullptr;
  UpdateDrawsContent(HasDrawableContent());
}

void PictureLayer::SetNearestNeighbor(bool nearest_neighbor) {
  if (picture_layer_inputs_.nearest_neighbor == nearest_neighbor)
    return;

  picture_layer_inputs_.nearest_neighbor = nearest_neighbor;
  SetNeedsCommit();
}

bool PictureLayer::HasDrawableContent() const {
  return picture_layer_inputs_.client && Layer::HasDrawableContent();
}

void PictureLayer::RunMicroBenchmark(MicroBenchmark* benchmark) {
  benchmark->RunOnLayer(this);
}

void PictureLayer::DropRecordingSourceContentIfInvalid() {
  int source_frame_number = layer_tree_host()->SourceFrameNumber();
  gfx::Size recording_source_bounds = recording_source_->GetSize();

  gfx::Size layer_bounds = bounds();
  if (paint_properties().source_frame_number == source_frame_number)
    layer_bounds = paint_properties().bounds;

  // If update called, then recording source size must match bounds pushed to
  // impl layer.
  DCHECK(update_source_frame_number_ != source_frame_number ||
         layer_bounds == recording_source_bounds)
      << " bounds " << layer_bounds.ToString() << " recording source "
      << recording_source_bounds.ToString();

  if (update_source_frame_number_ != source_frame_number &&
      recording_source_bounds != layer_bounds) {
    // Update may not get called for the layer (if it's not in the viewport
    // for example), even though it has resized making the recording source no
    // longer valid. In this case just destroy the recording source.
    recording_source_->SetEmptyBounds();
    picture_layer_inputs_.recorded_viewport = gfx::Rect();
    picture_layer_inputs_.display_list = nullptr;
    picture_layer_inputs_.painter_reported_memory_usage = 0;
  }
}

const DisplayItemList* PictureLayer::GetDisplayItemList() {
  return picture_layer_inputs_.display_list.get();
}

}  // namespace cc
