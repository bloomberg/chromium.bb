// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_VIDEO_LAYER_IMPL_H_
#define CC_VIDEO_LAYER_IMPL_H_

#include "base/callback.h"
#include "base/synchronization/lock.h"
#include "cc/cc_export.h"
#include "cc/layer_impl.h"
#include "cc/video_frame_provider.h"
#include "media/base/video_frame.h"
#include "third_party/khronos/GLES2/gl2.h"
#include "ui/gfx/size.h"
#include "ui/gfx/transform.h"

namespace media {
class SkCanvasVideoRenderer;
}

namespace cc {
class LayerTreeHostImpl;
class VideoFrameProviderClientImpl;

class CC_EXPORT VideoLayerImpl : public LayerImpl {
public:
    static scoped_ptr<VideoLayerImpl> create(LayerTreeImpl* treeImpl, int id, VideoFrameProvider* provider);
    virtual ~VideoLayerImpl();

    // LayerImpl implementation.
    virtual scoped_ptr<LayerImpl> createLayerImpl(LayerTreeImpl*) OVERRIDE;
    virtual void pushPropertiesTo(LayerImpl*) OVERRIDE;
    virtual void willDraw(ResourceProvider*) OVERRIDE;
    virtual void appendQuads(QuadSink&, AppendQuadsData&) OVERRIDE;
    virtual void didDraw(ResourceProvider*) OVERRIDE;
    virtual void didBecomeActive() OVERRIDE;
    virtual void didLoseOutputSurface() OVERRIDE;

    void setNeedsRedraw();

    void setProviderClientImpl(scoped_refptr<VideoFrameProviderClientImpl>);

    struct FramePlane {
        ResourceProvider::ResourceId resourceId;
        gfx::Size size;
        GLenum format;

        FramePlane() : resourceId(0), format(GL_LUMINANCE) { }

        bool allocateData(ResourceProvider*);
        void freeData(ResourceProvider*);
    };

private:
    VideoLayerImpl(LayerTreeImpl*, int);

    virtual const char* layerTypeAsString() const OVERRIDE;

    void willDrawInternal(ResourceProvider*);
    bool allocatePlaneData(ResourceProvider*);
    bool copyPlaneData(ResourceProvider*);
    void freePlaneData(ResourceProvider*);
    void freeUnusedPlaneData(ResourceProvider*);
    size_t numPlanes() const;

    scoped_refptr<VideoFrameProviderClientImpl> m_providerClientImpl;

    media::VideoFrame* m_frame;
    GLenum m_format;
    bool m_convertYUV;
    ResourceProvider::ResourceId m_externalTextureResource;
    scoped_ptr<media::SkCanvasVideoRenderer> m_videoRenderer;

    // Each index in this array corresponds to a plane in media::VideoFrame.
    FramePlane m_framePlanes[media::VideoFrame::kMaxPlanes];
};

}  // namespace cc

#endif  // CC_VIDEO_LAYER_IMPL_H_
