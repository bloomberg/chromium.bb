// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CCSolidColorLayerImpl_h
#define CCSolidColorLayerImpl_h

#include "CCLayerImpl.h"
#include <public/WebTransformationMatrix.h>

namespace cc {

class CCSolidColorLayerImpl : public CCLayerImpl {
public:
    static PassOwnPtr<CCSolidColorLayerImpl> create(int id)
    {
        return adoptPtr(new CCSolidColorLayerImpl(id));
    }
    virtual ~CCSolidColorLayerImpl();

    virtual void appendQuads(CCQuadSink&, CCAppendQuadsData&) OVERRIDE;

protected:
    explicit CCSolidColorLayerImpl(int id);

private:
    virtual const char* layerTypeAsString() const OVERRIDE { return "SolidColorLayer"; }

    const int m_tileSize;
};

}

#endif // CCSolidColorLayerImpl_h
