// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_IO_SURFACE_LAYER_H_
#define CC_IO_SURFACE_LAYER_H_

#include "cc/cc_export.h"
#include "cc/layer.h"

namespace cc {

class CC_EXPORT IOSurfaceLayer : public Layer {
 public:
  static scoped_refptr<IOSurfaceLayer> Create();

  void SetIOSurfaceProperties(uint32_t io_surface_id, gfx::Size size);

  virtual scoped_ptr<LayerImpl> CreateLayerImpl(LayerTreeImpl* tree_impl)
      OVERRIDE;
  virtual bool DrawsContent() const OVERRIDE;
  virtual void PushPropertiesTo(LayerImpl* layer) OVERRIDE;

 protected:
  IOSurfaceLayer();

 private:
  virtual ~IOSurfaceLayer();

  uint32_t io_surface_id_;
  gfx::Size io_surface_size_;
};

}
#endif  // CC_IO_SURFACE_LAYER_H_
