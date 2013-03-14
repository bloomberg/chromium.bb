// Copyright 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_CONTENT_LAYER_H_
#define CC_CONTENT_LAYER_H_

#include "base/basictypes.h"
#include "cc/cc_export.h"
#include "cc/layer_painter.h"
#include "cc/tiled_layer.h"

class SkCanvas;

namespace cc {

class ContentLayerClient;
class LayerUpdater;

class CC_EXPORT ContentLayerPainter : public LayerPainter {
 public:
  static scoped_ptr<ContentLayerPainter> Create(ContentLayerClient* client);

  virtual void Paint(SkCanvas* canvas,
                     gfx::Rect content_rect,
                     gfx::RectF* opaque) OVERRIDE;

 private:
  explicit ContentLayerPainter(ContentLayerClient* client);

  ContentLayerClient* client_;

  DISALLOW_COPY_AND_ASSIGN(ContentLayerPainter);
};

// A layer that renders its contents into an SkCanvas.
class CC_EXPORT ContentLayer : public TiledLayer {
 public:
  static scoped_refptr<ContentLayer> Create(ContentLayerClient* client);

  void ClearClient() { client_ = NULL; }

  virtual bool DrawsContent() const OVERRIDE;
  virtual void SetTexturePriorities(const PriorityCalculator& priority_calc)
      OVERRIDE;
  virtual void Update(ResourceUpdateQueue* queue,
                      const OcclusionTracker* occlusion,
                      RenderingStats* stats) OVERRIDE;
  virtual bool NeedMoreUpdates() OVERRIDE;

  virtual void SetContentsOpaque(bool contents_opaque) OVERRIDE;

 protected:
  explicit ContentLayer(ContentLayerClient* client);
  virtual ~ContentLayer();

 private:
  // TiledLayer implementation.
  virtual LayerUpdater* Updater() const OVERRIDE;
  virtual void CreateUpdaterIfNeeded() OVERRIDE;

  ContentLayerClient* client_;
  scoped_refptr<LayerUpdater> updater_;

  DISALLOW_COPY_AND_ASSIGN(ContentLayer);
};

}
#endif  // CC_CONTENT_LAYER_H_
