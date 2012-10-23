// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CCVideoLayerImpl_h
#define CCVideoLayerImpl_h

#include "IntSize.h"
#include "base/synchronization/lock.h"
#include "cc/layer_impl.h"
#include "third_party/khronos/GLES2/gl2.h"
#include <public/WebTransformationMatrix.h>
#include <public/WebVideoFrameProvider.h>
#include <wtf/ThreadingPrimitives.h>

namespace WebKit {
class WebVideoFrame;
}

namespace cc {

class LayerTreeHostImpl;
class VideoLayerImpl;

class VideoLayerImpl : public LayerImpl
                       , public WebKit::WebVideoFrameProvider::Client {
public:
    static scoped_ptr<VideoLayerImpl> create(int id, WebKit::WebVideoFrameProvider* provider)
    {
        return make_scoped_ptr(new VideoLayerImpl(id, provider));
    }
    virtual ~VideoLayerImpl();

    virtual void willDraw(ResourceProvider*) OVERRIDE;
    virtual void appendQuads(QuadSink&, AppendQuadsData&) OVERRIDE;
    virtual void didDraw(ResourceProvider*) OVERRIDE;

    virtual void dumpLayerProperties(std::string*, int indent) const OVERRIDE;

    // WebKit::WebVideoFrameProvider::Client implementation.
    virtual void stopUsingProvider(); // Callable on any thread.
    virtual void didReceiveFrame(); // Callable on impl thread.
    virtual void didUpdateMatrix(const float*); // Callable on impl thread.

    virtual void didLoseContext() OVERRIDE;

    void setNeedsRedraw();

    struct FramePlane {
        ResourceProvider::ResourceId resourceId;
        IntSize size;
        GLenum format;
        IntSize visibleSize;

        FramePlane() : resourceId(0) { }

        bool allocateData(ResourceProvider*);
        void freeData(ResourceProvider*);
    };

private:
    VideoLayerImpl(int, WebKit::WebVideoFrameProvider*);

    static IntSize computeVisibleSize(const WebKit::WebVideoFrame&, unsigned plane);
    virtual const char* layerTypeAsString() const OVERRIDE;

    void willDrawInternal(ResourceProvider*);
    bool allocatePlaneData(ResourceProvider*);
    bool copyPlaneData(ResourceProvider*);
    void freePlaneData(ResourceProvider*);
    void freeUnusedPlaneData(ResourceProvider*);

    // Guards the destruction of m_provider and the frame that it provides
    base::Lock m_providerLock;
    WebKit::WebVideoFrameProvider* m_provider;

    WebKit::WebTransformationMatrix m_streamTextureMatrix;

    WebKit::WebVideoFrame* m_frame;
    GLenum m_format;
    ResourceProvider::ResourceId m_externalTextureResource;

    // Each index in this array corresponds to a plane in WebKit::WebVideoFrame.
    FramePlane m_framePlanes[WebKit::WebVideoFrame::maxPlanes];
};

}

#endif // CCVideoLayerImpl_h
