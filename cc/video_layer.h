// Copyright 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_VIDEO_LAYER_H_
#define CC_VIDEO_LAYER_H_

#include "base/callback.h"
#include "cc/cc_export.h"
#include "cc/layer.h"

namespace WebKit {
class WebVideoFrame;
class WebVideoFrameProvider;
}

namespace media {
class VideoFrame;
}

namespace cc {

class VideoLayerImpl;

// A Layer that contains a Video element.
class CC_EXPORT VideoLayer : public Layer {
public:
    typedef base::Callback<media::VideoFrame* (WebKit::WebVideoFrame*)> FrameUnwrapper;

    static scoped_refptr<VideoLayer> create(WebKit::WebVideoFrameProvider*,
                                            const FrameUnwrapper&);

    virtual scoped_ptr<LayerImpl> createLayerImpl(LayerTreeHostImpl* hostImpl) OVERRIDE;

private:
    VideoLayer(WebKit::WebVideoFrameProvider*, const FrameUnwrapper&);
    virtual ~VideoLayer();

    // This pointer is only for passing to VideoLayerImpl's constructor. It should never be dereferenced by this class.
    WebKit::WebVideoFrameProvider* m_provider;
    FrameUnwrapper m_unwrapper;
};

}  // namespace cc

#endif  // CC_VIDEO_LAYER_H_
