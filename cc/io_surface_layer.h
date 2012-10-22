// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOSurfaceLayerChromium_h
#define IOSurfaceLayerChromium_h

#include "cc/layer.h"

namespace cc {

class IOSurfaceLayer : public Layer {
public:
    static scoped_refptr<IOSurfaceLayer> create();

    void setIOSurfaceProperties(uint32_t ioSurfaceId, const IntSize&);

    virtual scoped_ptr<LayerImpl> createLayerImpl() OVERRIDE;
    virtual bool drawsContent() const OVERRIDE;
    virtual void pushPropertiesTo(LayerImpl*) OVERRIDE;

protected:
    IOSurfaceLayer();

private:
    virtual ~IOSurfaceLayer();

    uint32_t m_ioSurfaceId;
    IntSize m_ioSurfaceSize;
};

}
#endif
