// Copyright 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VideoLayerChromium_h
#define VideoLayerChromium_h

#include "cc/layer.h"

namespace WebKit {
class WebVideoFrameProvider;
}

namespace cc {

class VideoLayerImpl;

// A Layer that contains a Video element.
class VideoLayer : public Layer {
public:
    static scoped_refptr<VideoLayer> create(WebKit::WebVideoFrameProvider*);

    virtual scoped_ptr<LayerImpl> createLayerImpl() OVERRIDE;

private:
    explicit VideoLayer(WebKit::WebVideoFrameProvider*);
    virtual ~VideoLayer();

    // This pointer is only for passing to VideoLayerImpl's constructor. It should never be dereferenced by this class.
    WebKit::WebVideoFrameProvider* m_provider;
};

}
#endif
