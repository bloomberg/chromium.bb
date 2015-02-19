// Copyright 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_LAYERS_CONTENT_LAYER_H_
#define CC_LAYERS_CONTENT_LAYER_H_

#include "base/basictypes.h"
#include "cc/base/cc_export.h"
#include "cc/layers/tiled_layer.h"
#include "cc/resources/layer_painter.h"

class SkCanvas;

namespace cc {

class ContentLayerClient;
class ContentLayerUpdater;

class CC_EXPORT ContentLayerPainter : public LayerPainter {
 public:
  static scoped_ptr<ContentLayerPainter> Create(ContentLayerClient* client);

  void Paint(SkCanvas* canvas, const gfx::Rect& content_rect) override;

 private:
  explicit ContentLayerPainter(ContentLayerClient* client);

  ContentLayerClient* client_;

  DISALLOW_COPY_AND_ASSIGN(ContentLayerPainter);
};

// A layer that renders its contents into an SkCanvas.
class CC_EXPORT ContentLayer : public TiledLayer {
 public:
  static scoped_refptr<ContentLayer> Create(ContentLayerClient* client);

  void ClearClient();

  void SetLayerTreeHost(LayerTreeHost* layer_tree_host) override;
  void SetTexturePriorities(const PriorityCalculator& priority_calc) override;
  bool Update(ResourceUpdateQueue* queue,
              const OcclusionTracker<Layer>* occlusion) override;
  bool NeedMoreUpdates() override;

  void SetContentsOpaque(bool contents_opaque) override;

  skia::RefPtr<SkPicture> GetPicture() const override;

  void OnOutputSurfaceCreated() override;

 protected:
  explicit ContentLayer(ContentLayerClient* client);
  ~ContentLayer() override;

  bool HasDrawableContent() const override;

  // TiledLayer implementation.
  LayerUpdater* Updater() const override;

 private:
  // TiledLayer implementation.
  void CreateUpdaterIfNeeded() override;

  ContentLayerClient* client_;
  scoped_refptr<ContentLayerUpdater> updater_;

  DISALLOW_COPY_AND_ASSIGN(ContentLayer);
};

}  // namespace cc
#endif  // CC_LAYERS_CONTENT_LAYER_H_
