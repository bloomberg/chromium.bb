// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/picture_layer.h"

#include "base/auto_reset.h"
#include "base/trace_event/trace_event.h"
#include "cc/layers/content_layer_client.h"
#include "cc/layers/picture_layer_impl.h"
#include "cc/playback/display_list_recording_source.h"
#include "cc/proto/cc_conversions.h"
#include "cc/proto/gfx_conversions.h"
#include "cc/proto/layer.pb.h"
#include "cc/trees/layer_tree_host.h"
#include "cc/trees/layer_tree_impl.h"
#include "third_party/skia/include/core/SkPictureRecorder.h"
#include "ui/gfx/geometry/rect_conversions.h"

namespace cc {

scoped_refptr<PictureLayer> PictureLayer::Create(const LayerSettings& settings,
                                                 ContentLayerClient* client) {
  return make_scoped_refptr(new PictureLayer(settings, client));
}

PictureLayer::PictureLayer(const LayerSettings& settings,
                           ContentLayerClient* client)
    : Layer(settings),
      client_(client),
      instrumentation_object_tracker_(id()),
      update_source_frame_number_(-1),
      is_mask_(false),
      nearest_neighbor_(false) {
}

PictureLayer::PictureLayer(const LayerSettings& settings,
                           ContentLayerClient* client,
                           scoped_ptr<DisplayListRecordingSource> source)
    : PictureLayer(settings, client) {
  recording_source_ = std::move(source);
}

PictureLayer::~PictureLayer() {
}

scoped_ptr<LayerImpl> PictureLayer::CreateLayerImpl(LayerTreeImpl* tree_impl) {
  return PictureLayerImpl::Create(tree_impl, id(), is_mask_,
                                  new LayerImpl::SyncedScrollOffset);
}

void PictureLayer::PushPropertiesTo(LayerImpl* base_layer) {
  Layer::PushPropertiesTo(base_layer);
  TRACE_EVENT0("cc", "PictureLayer::PushPropertiesTo");
  PictureLayerImpl* layer_impl = static_cast<PictureLayerImpl*>(base_layer);
  // TODO(danakj): Make is_mask_ a constructor parameter for PictureLayer.
  DCHECK_EQ(layer_impl->is_mask(), is_mask_);
  DropRecordingSourceContentIfInvalid();

  layer_impl->SetNearestNeighbor(nearest_neighbor_);

  // Preserve lcd text settings from the current raster source.
  bool can_use_lcd_text = layer_impl->RasterSourceUsesLCDText();
  scoped_refptr<DisplayListRasterSource> raster_source =
      recording_source_->CreateRasterSource(can_use_lcd_text);
  layer_impl->set_gpu_raster_max_texture_size(
      layer_tree_host()->device_viewport_size());
  layer_impl->UpdateRasterSource(raster_source, invalidation_.region(),
                                 nullptr);
  DCHECK(invalidation_.IsEmpty());
}

void PictureLayer::SetLayerTreeHost(LayerTreeHost* host) {
  Layer::SetLayerTreeHost(host);
  if (!host)
    return;

  if (!recording_source_)
    recording_source_.reset(new DisplayListRecordingSource);
  recording_source_->SetSlowdownRasterScaleFactor(
      host->debug_state().slow_down_raster_scale_factor);
  // If we need to enable image decode tasks, then we have to generate the
  // discardable images metadata.
  const LayerTreeSettings& settings = layer_tree_host()->settings();
  recording_source_->SetGenerateDiscardableImagesMetadata(
      settings.image_decode_tasks_enabled);
}

void PictureLayer::SetNeedsDisplayRect(const gfx::Rect& layer_rect) {
  DCHECK(!layer_tree_host() || !layer_tree_host()->in_paint_layer_contents());
  if (!layer_rect.IsEmpty()) {
    // Clamp invalidation to the layer bounds.
    invalidation_.Union(gfx::IntersectRects(layer_rect, gfx::Rect(bounds())));
  }
  Layer::SetNeedsDisplayRect(layer_rect);
}

bool PictureLayer::Update() {
  update_source_frame_number_ = layer_tree_host()->source_frame_number();
  bool updated = Layer::Update();

  gfx::Rect update_rect = visible_layer_rect();
  gfx::Size layer_size = paint_properties().bounds;

  if (last_updated_visible_layer_rect_ == update_rect &&
      recording_source_->GetSize() == layer_size && invalidation_.IsEmpty()) {
    // Only early out if the visible content rect of this layer hasn't changed.
    return updated;
  }

  recording_source_->SetBackgroundColor(SafeOpaqueBackgroundColor());
  recording_source_->SetRequiresClear(!contents_opaque() &&
                                      !client_->FillsBoundsCompletely());

  TRACE_EVENT1("cc", "PictureLayer::Update",
               "source_frame_number",
               layer_tree_host()->source_frame_number());
  devtools_instrumentation::ScopedLayerTreeTask update_layer(
      devtools_instrumentation::kUpdateLayer, id(), layer_tree_host()->id());

  // UpdateAndExpandInvalidation will give us an invalidation that covers
  // anything not explicitly recorded in this frame. We give this region
  // to the impl side so that it drops tiles that may not have a recording
  // for them.
  DCHECK(client_);
  updated |= recording_source_->UpdateAndExpandInvalidation(
      client_, invalidation_.region(), layer_size, update_rect,
      update_source_frame_number_, DisplayListRecordingSource::RECORD_NORMALLY);
  last_updated_visible_layer_rect_ = visible_layer_rect();

  if (updated) {
    SetNeedsPushProperties();
  } else {
    // If this invalidation did not affect the recording source, then it can be
    // cleared as an optimization.
    invalidation_.Clear();
  }

  return updated;
}

void PictureLayer::SetIsMask(bool is_mask) {
  is_mask_ = is_mask;
}

skia::RefPtr<SkPicture> PictureLayer::GetPicture() const {
  // We could either flatten the DisplayListRecordingSource into a single
  // SkPicture, or paint a fresh one depending on what we intend to do with the
  // picture. For now we just paint a fresh one to get consistent results.
  if (!DrawsContent())
    return skia::RefPtr<SkPicture>();

  gfx::Size layer_size = bounds();
  scoped_ptr<DisplayListRecordingSource> recording_source(
      new DisplayListRecordingSource);
  Region recording_invalidation;
  recording_source->UpdateAndExpandInvalidation(
      client_, &recording_invalidation, layer_size, gfx::Rect(layer_size),
      update_source_frame_number_, DisplayListRecordingSource::RECORD_NORMALLY);

  scoped_refptr<DisplayListRasterSource> raster_source =
      recording_source->CreateRasterSource(false);

  return raster_source->GetFlattenedPicture();
}

bool PictureLayer::IsSuitableForGpuRasterization() const {
  return recording_source_->IsSuitableForGpuRasterization();
}

void PictureLayer::ClearClient() {
  client_ = nullptr;
  UpdateDrawsContent(HasDrawableContent());
}

void PictureLayer::SetNearestNeighbor(bool nearest_neighbor) {
  if (nearest_neighbor_ == nearest_neighbor)
    return;

  nearest_neighbor_ = nearest_neighbor;
  SetNeedsCommit();
}

bool PictureLayer::HasDrawableContent() const {
  return client_ && Layer::HasDrawableContent();
}

void PictureLayer::SetTypeForProtoSerialization(proto::LayerNode* proto) const {
  proto->set_type(proto::LayerType::PICTURE_LAYER);
}

void PictureLayer::LayerSpecificPropertiesToProto(
    proto::LayerProperties* proto) {
  Layer::LayerSpecificPropertiesToProto(proto);
  DropRecordingSourceContentIfInvalid();

  proto::PictureLayerProperties* picture = proto->mutable_picture();
  recording_source_->ToProtobuf(
      picture->mutable_recording_source(),
      layer_tree_host()->image_serialization_processor());
  RegionToProto(*invalidation_.region(), picture->mutable_invalidation());
  RectToProto(last_updated_visible_layer_rect_,
              picture->mutable_last_updated_visible_layer_rect());
  picture->set_is_mask(is_mask_);
  picture->set_nearest_neighbor(nearest_neighbor_);

  picture->set_update_source_frame_number(update_source_frame_number_);

  invalidation_.Clear();
}

void PictureLayer::FromLayerSpecificPropertiesProto(
    const proto::LayerProperties& proto) {
  Layer::FromLayerSpecificPropertiesProto(proto);
  const proto::PictureLayerProperties& picture = proto.picture();
  recording_source_->FromProtobuf(
      picture.recording_source(),
      layer_tree_host()->image_serialization_processor());

  Region new_invalidation = RegionFromProto(picture.invalidation());
  invalidation_.Swap(&new_invalidation);
  last_updated_visible_layer_rect_ =
      ProtoToRect(picture.last_updated_visible_layer_rect());
  is_mask_ = picture.is_mask();
  nearest_neighbor_ = picture.nearest_neighbor();

  update_source_frame_number_ = picture.update_source_frame_number();
}

void PictureLayer::RunMicroBenchmark(MicroBenchmark* benchmark) {
  benchmark->RunOnLayer(this);
}

void PictureLayer::DropRecordingSourceContentIfInvalid() {
  int source_frame_number = layer_tree_host()->source_frame_number();
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
  }
}

}  // namespace cc
