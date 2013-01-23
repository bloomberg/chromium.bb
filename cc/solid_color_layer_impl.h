// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_SOLID_COLOR_LAYER_IMPL_H_
#define CC_SOLID_COLOR_LAYER_IMPL_H_

#include "base/memory/scoped_ptr.h"
#include "cc/cc_export.h"
#include "cc/layer_impl.h"

namespace cc {

class CC_EXPORT SolidColorLayerImpl : public LayerImpl {
public:
    static scoped_ptr<SolidColorLayerImpl> create(LayerTreeImpl* treeImpl, int id)
    {
        return make_scoped_ptr(new SolidColorLayerImpl(treeImpl, id));
    }
    virtual ~SolidColorLayerImpl();

    // LayerImpl overrides.
    virtual scoped_ptr<LayerImpl> createLayerImpl(LayerTreeImpl*) OVERRIDE;
    virtual void appendQuads(QuadSink&, AppendQuadsData&) OVERRIDE;

protected:
    SolidColorLayerImpl(LayerTreeImpl* treeImpl, int id);

private:
    virtual const char* layerTypeAsString() const OVERRIDE;

    const int m_tileSize;
};

}

#endif  // CC_SOLID_COLOR_LAYER_IMPL_H_
