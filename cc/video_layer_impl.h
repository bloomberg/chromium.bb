// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CCVideoLayerImpl_h
#define CCVideoLayerImpl_h

#include "IntSize.h"
#include "base/callback.h"
#include "base/synchronization/lock.h"
#include "cc/layer_impl.h"
#include "media/base/video_frame.h"
#include "third_party/khronos/GLES2/gl2.h"
#include <public/WebTransformationMatrix.h>
#include <public/WebVideoFrameProvider.h>

namespace WebKit {
class WebVideoFrame;
}

namespace media {
class SkCanvasVideoRenderer;
}

namespace cc {

class LayerTreeHostImpl;
class VideoLayerImpl;

class VideoLayerImpl : public LayerImpl
                     , public WebKit::WebVideoFrameProvider::Client {
public:
    typedef base::Callback<media::VideoFrame* (WebKit::WebVideoFrame*)> FrameUnwrapper;

    static scoped_ptr<VideoLayerImpl> create(int id, WebKit::WebVideoFrameProvider* provider,
                                             const FrameUnwrapper& unwrapper)
    {
        return make_scoped_ptr(new VideoLayerImpl(id, provider, unwrapper));
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
    VideoLayerImpl(int, WebKit::WebVideoFrameProvider*, const FrameUnwrapper&);

    virtual const char* layerTypeAsString() const OVERRIDE;

    void willDrawInternal(ResourceProvider*);
    bool allocatePlaneData(ResourceProvider*);
    bool copyPlaneData(ResourceProvider*);
    void freePlaneData(ResourceProvider*);
    void freeUnusedPlaneData(ResourceProvider*);
    size_t numPlanes() const;

    // Guards the destruction of m_provider and the frame that it provides
    base::Lock m_providerLock;
    WebKit::WebVideoFrameProvider* m_provider;

    WebKit::WebTransformationMatrix m_streamTextureMatrix;

    FrameUnwrapper m_unwrapper;
    WebKit::WebVideoFrame *m_webFrame;
    media::VideoFrame* m_frame;
    GLenum m_format;
    bool m_convertYUV;
    ResourceProvider::ResourceId m_externalTextureResource;
    scoped_ptr<media::SkCanvasVideoRenderer> m_videoRenderer;

    // Each index in this array corresponds to a plane in media::VideoFrame.
    FramePlane m_framePlanes[media::VideoFrame::kMaxPlanes];
};

}  // namespace cc

#endif // CCVideoLayerImpl_h
