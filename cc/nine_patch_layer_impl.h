// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_NINE_PATCH_LAYER_IMPL_H_
#define CC_NINE_PATCH_LAYER_IMPL_H_

#include "cc/cc_export.h"
#include "cc/layer_impl.h"
#include "cc/resource_provider.h"
#include "ui/gfx/size.h"
#include "ui/gfx/rect.h"

namespace base {
class DictionaryValue;
}

namespace cc {

class CC_EXPORT NinePatchLayerImpl : public LayerImpl {
public:
    static scoped_ptr<NinePatchLayerImpl> create(LayerTreeImpl* treeImpl, int id)
    {
        return make_scoped_ptr(new NinePatchLayerImpl(treeImpl, id));
    }
    virtual ~NinePatchLayerImpl();

    void setResourceId(unsigned id) { m_resourceId = id; }
    void setLayout(const gfx::Size& imageBounds, const gfx::Rect& aperture);

    virtual scoped_ptr<LayerImpl> createLayerImpl(LayerTreeImpl* treeImpl) OVERRIDE;
    virtual void pushPropertiesTo(LayerImpl*) OVERRIDE;

    virtual void willDraw(ResourceProvider*) OVERRIDE;
    virtual void appendQuads(QuadSink&, AppendQuadsData&) OVERRIDE;
    virtual void didDraw(ResourceProvider*) OVERRIDE;
    virtual ResourceProvider::ResourceId contentsResourceId() const OVERRIDE;
    virtual void dumpLayerProperties(std::string*, int indent) const OVERRIDE;
    virtual void didLoseOutputSurface() OVERRIDE;

    virtual base::DictionaryValue* layerTreeAsJson() const OVERRIDE;

protected:
    NinePatchLayerImpl(LayerTreeImpl* treeImpl, int id);

private:
    virtual const char* layerTypeAsString() const OVERRIDE;

    // The size of the NinePatch bitmap in pixels.
    gfx::Size m_imageBounds;

    // The transparent center region that shows the parent layer's contents in image space.
    gfx::Rect m_imageAperture;

    ResourceProvider::ResourceId m_resourceId;
};

}  // namespace cc

#endif  // CC_NINE_PATCH_LAYER_IMPL_H_
