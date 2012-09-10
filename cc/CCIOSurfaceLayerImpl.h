// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CCIOSurfaceLayerImpl_h
#define CCIOSurfaceLayerImpl_h

#include "CCLayerImpl.h"
#include "IntSize.h"

namespace WebCore {

class CCIOSurfaceLayerImpl : public CCLayerImpl {
public:
    static PassOwnPtr<CCIOSurfaceLayerImpl> create(int id)
    {
        return adoptPtr(new CCIOSurfaceLayerImpl(id));
    }
    virtual ~CCIOSurfaceLayerImpl();

    void setIOSurfaceProperties(unsigned ioSurfaceId, const IntSize&);

    virtual void appendQuads(CCQuadSink&, CCAppendQuadsData&) OVERRIDE;

    virtual void willDraw(CCResourceProvider*) OVERRIDE;
    virtual void didLoseContext() OVERRIDE;

    virtual void dumpLayerProperties(std::string*, int indent) const OVERRIDE;

private:
    explicit CCIOSurfaceLayerImpl(int);

    virtual const char* layerTypeAsString() const OVERRIDE { return "IOSurfaceLayer"; }

    unsigned m_ioSurfaceId;
    IntSize m_ioSurfaceSize;
    bool m_ioSurfaceChanged;
    unsigned m_ioSurfaceTextureId;
};

}

#endif // CCIOSurfaceLayerImpl_h
