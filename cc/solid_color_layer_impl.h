// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CCSolidColorLayerImpl_h
#define CCSolidColorLayerImpl_h

#include "cc/layer_impl.h"
#include <public/WebTransformationMatrix.h>

namespace cc {

class SolidColorLayerImpl : public LayerImpl {
public:
    static scoped_ptr<SolidColorLayerImpl> create(int id)
    {
        return make_scoped_ptr(new SolidColorLayerImpl(id));
    }
    virtual ~SolidColorLayerImpl();

    virtual void appendQuads(QuadSink&, AppendQuadsData&) OVERRIDE;

protected:
    explicit SolidColorLayerImpl(int id);

private:
    virtual const char* layerTypeAsString() const OVERRIDE;

    const int m_tileSize;
};

}

#endif // CCSolidColorLayerImpl_h
