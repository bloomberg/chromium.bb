// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_IO_SURFACE_LAYER_IMPL_H_
#define CC_IO_SURFACE_LAYER_IMPL_H_

#include "cc/cc_export.h"
#include "cc/layer_impl.h"
#include "ui/gfx/size.h"

namespace cc {

class CC_EXPORT IOSurfaceLayerImpl : public LayerImpl {
public:
    static scoped_ptr<IOSurfaceLayerImpl> Create(LayerTreeImpl* treeImpl, int id)
    {
        return make_scoped_ptr(new IOSurfaceLayerImpl(treeImpl, id));
    }
    virtual ~IOSurfaceLayerImpl();

    void setIOSurfaceProperties(unsigned ioSurfaceId, const gfx::Size&);

    virtual scoped_ptr<LayerImpl> CreateLayerImpl(LayerTreeImpl* treeImpl) OVERRIDE;
    virtual void PushPropertiesTo(LayerImpl*) OVERRIDE;

    virtual void AppendQuads(QuadSink* quad_sink,
                             AppendQuadsData* append_quads_data) OVERRIDE;

    virtual void WillDraw(ResourceProvider*) OVERRIDE;
    virtual void DidLoseOutputSurface() OVERRIDE;

    virtual void DumpLayerProperties(std::string* str, int indent) const OVERRIDE;

private:
    IOSurfaceLayerImpl(LayerTreeImpl* treeImpl, int id);

    virtual const char* LayerTypeAsString() const OVERRIDE;

    unsigned m_ioSurfaceId;
    gfx::Size m_ioSurfaceSize;
    bool m_ioSurfaceChanged;
    unsigned m_ioSurfaceTextureId;
};

}

#endif  // CC_IO_SURFACE_LAYER_IMPL_H_
