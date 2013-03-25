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

// Base class for BitmapContentLayerUpdater and
// SkPictureContentLayerUpdater that reduces code duplication between
// their respective PaintContents implementations.
class CC_EXPORT ContentLayerUpdater : public LayerUpdater {
 protected:
  explicit ContentLayerUpdater(scoped_ptr<LayerPainter> painter);
  virtual ~ContentLayerUpdater();

  void PaintContents(SkCanvas* canvas,
                     gfx::Rect content_rect,
                     float contents_width_scale,
                     float contents_height_scale,
                     gfx::Rect* resulting_opaque_rect,
                     RenderingStats* stats);
  gfx::Rect content_rect() const { return content_rect_; }

 private:
  gfx::Rect content_rect_;
  scoped_ptr<LayerPainter> painter_;

  DISALLOW_COPY_AND_ASSIGN(ContentLayerUpdater);
};

}  // namespace cc

#endif  // CC_RESOURCES_CONTENT_LAYER_UPDATER_H_
