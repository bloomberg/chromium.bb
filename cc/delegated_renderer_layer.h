// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_DELEGATED_RENDERER_LAYER_H_
#define CC_DELEGATED_RENDERER_LAYER_H_

#include "cc/cc_export.h"
#include "cc/layer.h"
#include "cc/transferable_resource.h"

namespace cc {
class DelegatedFrameData;

class CC_EXPORT DelegatedRendererLayer : public Layer {
 public:
  static scoped_refptr<DelegatedRendererLayer> Create();

  virtual scoped_ptr<LayerImpl> createLayerImpl(LayerTreeImpl* tree_impl)
      OVERRIDE;
  virtual void pushPropertiesTo(LayerImpl* impl) OVERRIDE;
  virtual bool drawsContent() const OVERRIDE;

  // Set the size at which the frame should be displayed, with the origin at the
  // layer's origin. This must always contain at least the layer's bounds. A
  // value of (0, 0) implies that the frame should be displayed to fit exactly
  // in the layer's bounds.
  void SetDisplaySize(gfx::Size size);

  void SetFrameData(scoped_ptr<DelegatedFrameData> frame_data);

  // Passes ownership of any unused resources that had been given by the child
  // compositor to the given array, so they can be given back to the child.
  void TakeUnusedResourcesForChildCompositor(TransferableResourceArray* array);

 protected:
  DelegatedRendererLayer();
  virtual ~DelegatedRendererLayer();

 private:
  scoped_ptr<DelegatedFrameData> frame_data_;
  gfx::RectF damage_in_frame_;
  gfx::Size frame_size_;
  gfx::Size display_size_;
  TransferableResourceArray unused_resources_for_child_compositor_;
};

}
#endif  // CC_DELEGATED_RENDERER_LAYER_H_
