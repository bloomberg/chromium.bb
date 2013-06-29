// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_LAYERS_HEADS_UP_DISPLAY_LAYER_H_
#define CC_LAYERS_HEADS_UP_DISPLAY_LAYER_H_

#include "base/memory/scoped_ptr.h"
#include "cc/base/cc_export.h"
#include "cc/layers/layer.h"

namespace cc {

class CC_EXPORT HeadsUpDisplayLayer : public Layer {
 public:
  static scoped_refptr<HeadsUpDisplayLayer> Create();

  virtual void Update(ResourceUpdateQueue* queue,
                      const OcclusionTracker* tracker) OVERRIDE;
  virtual bool DrawsContent() const OVERRIDE;

  virtual scoped_ptr<LayerImpl> CreateLayerImpl(LayerTreeImpl* tree_impl)
      OVERRIDE;

 protected:
  HeadsUpDisplayLayer();

 private:
  virtual ~HeadsUpDisplayLayer();

  DISALLOW_COPY_AND_ASSIGN(HeadsUpDisplayLayer);
};

}  // namespace cc

#endif  // CC_LAYERS_HEADS_UP_DISPLAY_LAYER_H_
