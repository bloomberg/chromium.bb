// Copyright 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#ifndef VideoLayerChromium_h
#define VideoLayerChromium_h

#if USE(ACCELERATED_COMPOSITING)

#include "LayerChromium.h"

namespace WebKit {
class WebVideoFrameProvider;
}

namespace cc {

class CCVideoLayerImpl;

// A Layer that contains a Video element.
class VideoLayerChromium : public LayerChromium {
public:

    static PassRefPtr<VideoLayerChromium> create(WebKit::WebVideoFrameProvider*);
    virtual ~VideoLayerChromium();

    virtual PassOwnPtr<CCLayerImpl> createCCLayerImpl() OVERRIDE;

private:
    explicit VideoLayerChromium(WebKit::WebVideoFrameProvider*);

    // This pointer is only for passing to CCVideoLayerImpl's constructor. It should never be dereferenced by this class.
    WebKit::WebVideoFrameProvider* m_provider;
};

}
#endif // USE(ACCELERATED_COMPOSITING)

#endif
