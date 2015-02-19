// Copyright 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/content_layer.h"

#include "base/auto_reset.h"
#include "base/metrics/histogram.h"
#include "base/time/time.h"
#include "cc/layers/content_layer_client.h"
#include "cc/resources/bitmap_content_layer_updater.h"
#include "cc/resources/bitmap_skpicture_content_layer_updater.h"
#include "cc/resources/layer_painter.h"
#include "cc/trees/layer_tree_host.h"
#include "third_party/skia/include/core/SkPictureRecorder.h"

namespace cc {

ContentLayerPainter::ContentLayerPainter(ContentLayerClient* client)
    : client_(client) {}

scoped_ptr<ContentLayerPainter> ContentLayerPainter::Create(
    ContentLayerClient* client) {
  return make_scoped_ptr(new ContentLayerPainter(client));
}

void ContentLayerPainter::Paint(SkCanvas* canvas,
                                const gfx::Rect& content_rect) {
  client_->PaintContents(canvas, content_rect,
                         ContentLayerClient::PAINTING_BEHAVIOR_NORMAL);
}

scoped_refptr<ContentLayer> ContentLayer::Create(ContentLayerClient* client) {
  return make_scoped_refptr(new ContentLayer(client));
}

ContentLayer::ContentLayer(ContentLayerClient* client)
    : TiledLayer(), client_(client) {
}

ContentLayer::~ContentLayer() {}

void ContentLayer::ClearClient() {
  client_ = nullptr;
  UpdateDrawsContent(HasDrawableContent());
}

bool ContentLayer::HasDrawableContent() const {
  return client_ && TiledLayer::HasDrawableContent();
}

void ContentLayer::SetLayerTreeHost(LayerTreeHost* host) {
  TiledLayer::SetLayerTreeHost(host);

  if (!updater_.get())
    return;
}

void ContentLayer::SetTexturePriorities(
    const PriorityCalculator& priority_calc) {
  // Update the tile data before creating all the layer's tiles.
  UpdateTileSizeAndTilingOption();

  TiledLayer::SetTexturePriorities(priority_calc);
}

bool ContentLayer::Update(ResourceUpdateQueue* queue,
                          const OcclusionTracker<Layer>* occlusion) {
  {
    base::AutoReset<bool> ignore_set_needs_commit(&ignore_set_needs_commit_,
                                                  true);

    CreateUpdaterIfNeeded();
  }

  bool updated = TiledLayer::Update(queue, occlusion);
  return updated;
}

bool ContentLayer::NeedMoreUpdates() {
  return NeedsIdlePaint();
}

LayerUpdater* ContentLayer::Updater() const {
  return updater_.get();
}

void ContentLayer::CreateUpdaterIfNeeded() {
  if (updater_.get())
    return;
  scoped_ptr<LayerPainter> painter = ContentLayerPainter::Create(client_);
  if (layer_tree_host()->settings().per_tile_painting_enabled) {
    updater_ = BitmapSkPictureContentLayerUpdater::Create(
        painter.Pass(),
        rendering_stats_instrumentation(),
        id());
  } else {
    updater_ = BitmapContentLayerUpdater::Create(
        painter.Pass(),
        id());
  }
  updater_->SetOpaque(contents_opaque());
  if (client_)
    updater_->SetFillsBoundsCompletely(client_->FillsBoundsCompletely());
  updater_->SetBackgroundColor(background_color());

  SetTextureFormat(
      layer_tree_host()->GetRendererCapabilities().best_texture_format);
}

void ContentLayer::SetContentsOpaque(bool opaque) {
  Layer::SetContentsOpaque(opaque);
  if (updater_.get())
    updater_->SetOpaque(opaque);
}

skia::RefPtr<SkPicture> ContentLayer::GetPicture() const {
  if (!DrawsContent())
    return skia::RefPtr<SkPicture>();

  int width = bounds().width();
  int height = bounds().height();

  SkPictureRecorder recorder;
  SkCanvas* canvas = recorder.beginRecording(width, height, nullptr, 0);
  client_->PaintContents(canvas, gfx::Rect(width, height),
                         ContentLayerClient::PAINTING_BEHAVIOR_NORMAL);
  skia::RefPtr<SkPicture> picture = skia::AdoptRef(recorder.endRecording());
  return picture;
}

void ContentLayer::OnOutputSurfaceCreated() {
  SetTextureFormat(
      layer_tree_host()->GetRendererCapabilities().best_texture_format);
  TiledLayer::OnOutputSurfaceCreated();
}

}  // namespace cc
