// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_DELEGATED_RENDERER_LAYER_H_
#define CC_DELEGATED_RENDERER_LAYER_H_

#include "cc/cc_export.h"
#include "cc/layer.h"

namespace cc {

class CC_EXPORT DelegatedRendererLayer : public Layer {
 public:
  static scoped_refptr<DelegatedRendererLayer> Create();

  virtual scoped_ptr<LayerImpl> createLayerImpl(LayerTreeImpl* tree_impl)
      OVERRIDE;
  virtual void pushPropertiesTo(LayerImpl* impl) OVERRIDE;

 protected:
  DelegatedRendererLayer();

 private:
  virtual ~DelegatedRendererLayer();
};

}
#endif  // CC_DELEGATED_RENDERER_LAYER_H_
