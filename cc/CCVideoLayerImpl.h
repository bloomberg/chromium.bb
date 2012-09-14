// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CCVideoLayerImpl_h
#define CCVideoLayerImpl_h

#include "CCLayerImpl.h"
#include "GraphicsContext3D.h"
#include "IntSize.h"
#include <public/WebTransformationMatrix.h>
#include <public/WebVideoFrameProvider.h>

namespace WebKit {
class WebVideoFrame;
}

namespace cc {

class CCLayerTreeHostImpl;
class CCVideoLayerImpl;

class CCVideoLayerImpl : public CCLayerImpl
                       , public WebKit::WebVideoFrameProvider::Client {
public:
    static PassOwnPtr<CCVideoLayerImpl> create(int id, WebKit::WebVideoFrameProvider* provider)
    {
        return adoptPtr(new CCVideoLayerImpl(id, provider));
    }
    virtual ~CCVideoLayerImpl();

    virtual void willDraw(CCResourceProvider*) OVERRIDE;
    virtual void appendQuads(CCQuadSink&, CCAppendQuadsData&) OVERRIDE;
    virtual void didDraw(CCResourceProvider*) OVERRIDE;

    virtual void dumpLayerProperties(std::string*, int indent) const OVERRIDE;

    Mutex& providerMutex() { return m_providerMutex; }

    // WebKit::WebVideoFrameProvider::Client implementation.
    virtual void stopUsingProvider(); // Callable on any thread.
    virtual void didReceiveFrame(); // Callable on impl thread.
    virtual void didUpdateMatrix(const float*); // Callable on impl thread.

    virtual void didLoseContext() OVERRIDE;

    void setNeedsRedraw();

    struct FramePlane {
        CCResourceProvider::ResourceId resourceId;
        IntSize size;
        GC3Denum format;
        IntSize visibleSize;

        FramePlane() : resourceId(0) { }

        bool allocateData(CCResourceProvider*);
        void freeData(CCResourceProvider*);
    };

private:
    CCVideoLayerImpl(int, WebKit::WebVideoFrameProvider*);

    static IntSize computeVisibleSize(const WebKit::WebVideoFrame&, unsigned plane);
    virtual const char* layerTypeAsString() const OVERRIDE { return "VideoLayer"; }

    void willDrawInternal(CCResourceProvider*);
    bool allocatePlaneData(CCResourceProvider*);
    bool copyPlaneData(CCResourceProvider*);
    void freePlaneData(CCResourceProvider*);
    void freeUnusedPlaneData(CCResourceProvider*);

    // Guards the destruction of m_provider and the frame that it provides
    Mutex m_providerMutex;
    WebKit::WebVideoFrameProvider* m_provider;

    WebKit::WebTransformationMatrix m_streamTextureMatrix;

    WebKit::WebVideoFrame* m_frame;
    GC3Denum m_format;
    CCResourceProvider::ResourceId m_externalTextureResource;

    // Each index in this array corresponds to a plane in WebKit::WebVideoFrame.
    FramePlane m_framePlanes[WebKit::WebVideoFrame::maxPlanes];
};

}

#endif // CCVideoLayerImpl_h
