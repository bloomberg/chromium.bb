// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_DELEGATED_RENDERER_LAYER_H_
#define CC_DELEGATED_RENDERER_LAYER_H_

#include "cc/cc_export.h"
#include "cc/layer.h"

namespace cc {
class DelegatedFrameData;

class CC_EXPORT DelegatedRendererLayer : public Layer {
 public:
  static scoped_refptr<DelegatedRendererLayer> Create();

  virtual scoped_ptr<LayerImpl> createLayerImpl(LayerTreeImpl* tree_impl)
      OVERRIDE;
  virtual void pushPropertiesTo(LayerImpl* impl) OVERRIDE;

  // Set the size at which the frame should be displayed, with the origin at the
  // layer's origin. This must always contain at least the layer's bounds. A
  // value of (0, 0) implies that the frame should be displayed to fit exactly
  // in the layer's bounds.
  void SetDisplaySize(gfx::Size size);

  void SetFrameData(scoped_ptr<DelegatedFrameData> frame_data);

 protected:
  DelegatedRendererLayer();
  virtual ~DelegatedRendererLayer();

 private:
  scoped_ptr<DelegatedFrameData> frame_data_;
  gfx::RectF damage_in_frame_;
  gfx::Size display_size_;
};

}
#endif  // CC_DELEGATED_RENDERER_LAYER_H_
