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
    static scoped_refptr<IOSurfaceLayer> create();

    void setIOSurfaceProperties(uint32_t ioSurfaceId, const gfx::Size&);

    virtual scoped_ptr<LayerImpl> createLayerImpl(LayerTreeImpl* treeImpl) OVERRIDE;
    virtual bool drawsContent() const OVERRIDE;
    virtual void pushPropertiesTo(LayerImpl*) OVERRIDE;

protected:
    IOSurfaceLayer();

private:
    virtual ~IOSurfaceLayer();

    uint32_t m_ioSurfaceId;
    gfx::Size m_ioSurfaceSize;
};

}
#endif  // CC_IO_SURFACE_LAYER_H_
