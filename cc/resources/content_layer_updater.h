// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_RESOURCES_CONTENT_LAYER_UPDATER_H_
#define CC_RESOURCES_CONTENT_LAYER_UPDATER_H_

#include "cc/base/cc_export.h"
#include "cc/resources/layer_updater.h"
#include "ui/gfx/rect.h"

class SkCanvas;

namespace cc {

class LayerPainter;
class RenderingStatsInstrumentation;

// Base class for BitmapContentLayerUpdater and
// SkPictureContentLayerUpdater that reduces code duplication between
// their respective PaintContents implementations.
class CC_EXPORT ContentLayerUpdater : public LayerUpdater {
 public:
  void set_rendering_stats_instrumentation(RenderingStatsInstrumentation* rsi);
  virtual void SetOpaque(bool opaque) OVERRIDE;
  virtual void SetFillsBoundsCompletely(bool fills_bounds) OVERRIDE;
  virtual void SetBackgroundColor(SkColor background_color) OVERRIDE;

 protected:
  ContentLayerUpdater(scoped_ptr<LayerPainter> painter,
                      RenderingStatsInstrumentation* stats_instrumentation,
                      int layer_id);
  virtual ~ContentLayerUpdater();

  // Paints the contents. |content_size| size of the underlying layer in
  // layer's content space. |paint_rect| bounds to paint in content space of the
  // layer. Both |content_size| and |paint_rect| are in pixels.
  void PaintContents(SkCanvas* canvas,
                     const gfx::Size& layer_content_size,
                     const gfx::Rect& paint_rect,
                     float contents_width_scale,
                     float contents_height_scale);
  gfx::Rect paint_rect() const { return paint_rect_; }

  bool layer_is_opaque() const { return layer_is_opaque_; }
  bool layer_fills_bounds_completely() const {
    return layer_fills_bounds_completely_;
  }

  SkColor background_color() const { return background_color_; }

  RenderingStatsInstrumentation* rendering_stats_instrumentation_;
  int layer_id_;

  // True when it is known that all output pixels will be opaque.
  bool layer_is_opaque_;
  // True when it is known that all output pixels will be filled.
  bool layer_fills_bounds_completely_;

 private:
  gfx::Rect paint_rect_;
  scoped_ptr<LayerPainter> painter_;
  SkColor background_color_;

  DISALLOW_COPY_AND_ASSIGN(ContentLayerUpdater);
};

}  // namespace cc

#endif  // CC_RESOURCES_CONTENT_LAYER_UPDATER_H_
