// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_HEADS_UP_DISPLAY_LAYER_H_
#define CC_HEADS_UP_DISPLAY_LAYER_H_

#include "base/memory/scoped_ptr.h"
#include "cc/cc_export.h"
#include "cc/layer.h"

namespace cc {

class CC_EXPORT HeadsUpDisplayLayer : public Layer {
public:
    static scoped_refptr<HeadsUpDisplayLayer> create();

    virtual void update(ResourceUpdateQueue&, const OcclusionTracker*, RenderingStats*) OVERRIDE;
    virtual bool drawsContent() const OVERRIDE;

    virtual scoped_ptr<LayerImpl> createLayerImpl(LayerTreeImpl* treeImpl) OVERRIDE;

protected:
    HeadsUpDisplayLayer();

private:
    virtual ~HeadsUpDisplayLayer();
};

}  // namespace cc

#endif  // CC_HEADS_UP_DISPLAY_LAYER_H_
