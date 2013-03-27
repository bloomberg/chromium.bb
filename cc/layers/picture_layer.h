// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_LAYERS_PICTURE_LAYER_H_
#define CC_LAYERS_PICTURE_LAYER_H_

#include "cc/debug/devtools_instrumentation.h"
#include "cc/layers/contents_scaling_layer.h"
#include "cc/layers/layer.h"
#include "cc/resources/picture_pile.h"
#include "cc/trees/occlusion_tracker.h"

namespace cc {

class ContentLayerClient;
class ResourceUpdateQueue;
struct RenderingStats;

class CC_EXPORT PictureLayer : public ContentsScalingLayer {
 public:
  static scoped_refptr<PictureLayer> Create(ContentLayerClient* client);

  void ClearClient() { client_ = NULL; }

  // Implement Layer interface
  virtual bool DrawsContent() const OVERRIDE;
  virtual scoped_ptr<LayerImpl> CreateLayerImpl(
      LayerTreeImpl* tree_impl) OVERRIDE;
  virtual void SetLayerTreeHost(LayerTreeHost* host) OVERRIDE;
  virtual void PushPropertiesTo(LayerImpl* layer) OVERRIDE;
  virtual void SetNeedsDisplayRect(const gfx::RectF& layer_rect) OVERRIDE;
  virtual void Update(
      ResourceUpdateQueue* queue,
      const OcclusionTracker* occlusion,
      RenderingStats* stats) OVERRIDE;
  virtual void SetIsMask(bool is_mask) OVERRIDE;

 protected:
  explicit PictureLayer(ContentLayerClient* client);
  virtual ~PictureLayer();

 private:
  ContentLayerClient* client_;
  scoped_refptr<PicturePile> pile_;
  devtools_instrumentation::
      ScopedLayerObjectTracker instrumentation_object_tracker_;
  // Invalidation to use the next time update is called.
  Region pending_invalidation_;
  // Invalidation from the last time update was called.
  Region pile_invalidation_;
  bool is_mask_;

  DISALLOW_COPY_AND_ASSIGN(PictureLayer);
};

}  // namespace cc

#endif  // CC_LAYERS_PICTURE_LAYER_H_
