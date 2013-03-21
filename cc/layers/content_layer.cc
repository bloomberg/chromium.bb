// Copyright 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/content_layer.h"

#include "base/auto_reset.h"
#include "base/metrics/histogram.h"
#include "base/time.h"
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
                                gfx::Rect content_rect,
                                gfx::RectF* opaque) {
  base::TimeTicks paint_start = base::TimeTicks::HighResNow();
  client_->PaintContents(canvas, content_rect, opaque);
  base::TimeTicks paint_end = base::TimeTicks::HighResNow();
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
      client_(client) {}

ContentLayer::~ContentLayer() {}

bool ContentLayer::DrawsContent() const {
  return TiledLayer::DrawsContent() && client_;
}

void ContentLayer::SetTexturePriorities(
    const PriorityCalculator& priority_calc) {
  // Update the tile data before creating all the layer's tiles.
  UpdateTileSizeAndTilingOption();

  TiledLayer::SetTexturePriorities(priority_calc);
}

void ContentLayer::Update(ResourceUpdateQueue* queue,
                          const OcclusionTracker* occlusion,
                          RenderingStats* stats) {
  {
    base::AutoReset<bool> ignore_set_needs_commit(&ignore_set_needs_commit_,
                                                  true);

    CreateUpdaterIfNeeded();
  }

  TiledLayer::Update(queue, occlusion, stats);
  needs_display_ = false;
}

bool ContentLayer::NeedMoreUpdates() {
  return NeedsIdlePaint();
}

LayerUpdater* ContentLayer::Updater() const {
  return updater_.get();
}

void ContentLayer::CreateUpdaterIfNeeded() {
  if (updater_)
    return;
  scoped_ptr<LayerPainter> painter =
      ContentLayerPainter::Create(client_).PassAs<LayerPainter>();
  if (layer_tree_host()->settings().accelerate_painting)
    updater_ = SkPictureContentLayerUpdater::Create(painter.Pass());
  else if (layer_tree_host()->settings().per_tile_painting_enabled)
    updater_ = BitmapSkPictureContentLayerUpdater::Create(painter.Pass());
  else
    updater_ = BitmapContentLayerUpdater::Create(painter.Pass());
  updater_->SetOpaque(contents_opaque());

  unsigned texture_format =
      layer_tree_host()->GetRendererCapabilities().best_texture_format;
  SetTextureFormat(texture_format);
}

void ContentLayer::SetContentsOpaque(bool opaque) {
  Layer::SetContentsOpaque(opaque);
  if (updater_)
    updater_->SetOpaque(opaque);
}

}  // namespace cc
