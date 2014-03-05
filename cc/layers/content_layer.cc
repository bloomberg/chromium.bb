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

namespace cc {

ContentLayerPainter::ContentLayerPainter(ContentLayerClient* client)
    : client_(client) {}

scoped_ptr<ContentLayerPainter> ContentLayerPainter::Create(
    ContentLayerClient* client) {
  return make_scoped_ptr(new ContentLayerPainter(client));
}

void ContentLayerPainter::Paint(SkCanvas* canvas,
                                const gfx::Rect& content_rect,
                                gfx::RectF* opaque) {
  base::TimeTicks paint_start = base::TimeTicks::HighResNow();
  client_->PaintContents(canvas, content_rect, opaque);
  base::TimeTicks paint_end = base::TimeTicks::HighResNow();
  // The start and end times might be the same if the paint was very fast or if
  // our timer granularity is poor. Treat this as a very short time duration
  // instead of none to avoid dividing by zero.
  if (paint_end == paint_start)
    paint_end += base::TimeDelta::FromMicroseconds(1);

  double pixels_per_sec = (content_rect.width() * content_rect.height()) /
                          (paint_end - paint_start).InSecondsF();
  UMA_HISTOGRAM_CUSTOM_COUNTS("Renderer4.AccelContentPaintDurationMS",
                              (paint_end - paint_start).InMilliseconds(),
                              0,
                              120,
                              30);
  UMA_HISTOGRAM_CUSTOM_COUNTS("Renderer4.AccelContentPaintMegapixPerSecond",
                              pixels_per_sec / 1000000,
                              10,
                              210,
                              30);
}

scoped_refptr<ContentLayer> ContentLayer::Create(ContentLayerClient* client) {
  return make_scoped_refptr(new ContentLayer(client));
}

ContentLayer::ContentLayer(ContentLayerClient* client)
    : TiledLayer(),
      client_(client),
      can_use_lcd_text_last_frame_(can_use_lcd_text()) {
}

ContentLayer::~ContentLayer() {}

bool ContentLayer::DrawsContent() const {
  return TiledLayer::DrawsContent() && client_;
}

void ContentLayer::SetLayerTreeHost(LayerTreeHost* host) {
  TiledLayer::SetLayerTreeHost(host);

  if (!updater_.get())
    return;

  if (host) {
    updater_->set_rendering_stats_instrumentation(
        host->rendering_stats_instrumentation());
  } else {
    updater_->set_rendering_stats_instrumentation(NULL);
  }
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
    UpdateCanUseLCDText();
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
  scoped_ptr<LayerPainter> painter =
      ContentLayerPainter::Create(client_).PassAs<LayerPainter>();
  if (layer_tree_host()->settings().per_tile_painting_enabled) {
    updater_ = BitmapSkPictureContentLayerUpdater::Create(
        painter.Pass(),
        rendering_stats_instrumentation(),
        id());
  } else {
    updater_ = BitmapContentLayerUpdater::Create(
        painter.Pass(),
        rendering_stats_instrumentation(),
        id());
  }
  updater_->SetOpaque(contents_opaque());

  SetTextureFormat(
      layer_tree_host()->GetRendererCapabilities().best_texture_format);
}

void ContentLayer::SetContentsOpaque(bool opaque) {
  Layer::SetContentsOpaque(opaque);
  if (updater_.get())
    updater_->SetOpaque(opaque);
}

void ContentLayer::UpdateCanUseLCDText() {
  if (can_use_lcd_text_last_frame_ == can_use_lcd_text())
    return;

  can_use_lcd_text_last_frame_ = can_use_lcd_text();
  if (client_)
    client_->DidChangeLayerCanUseLCDText();
}

bool ContentLayer::SupportsLCDText() const {
  return true;
}

skia::RefPtr<SkPicture> ContentLayer::GetPicture() const {
  if (!DrawsContent())
    return skia::RefPtr<SkPicture>();

  int width = bounds().width();
  int height = bounds().height();
  gfx::RectF opaque;

  skia::RefPtr<SkPicture> picture = skia::AdoptRef(new SkPicture);
  SkCanvas* canvas = picture->beginRecording(width, height);
  client_->PaintContents(canvas, gfx::Rect(width, height), &opaque);
  picture->endRecording();
  return picture;
}

void ContentLayer::OnOutputSurfaceCreated() {
  SetTextureFormat(
      layer_tree_host()->GetRendererCapabilities().best_texture_format);
  TiledLayer::OnOutputSurfaceCreated();
}

}  // namespace cc
