// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TextureLayerChromiumClient_h
#define TextureLayerChromiumClient_h

namespace WebKit {
class WebGraphicsContext3D;
}

namespace cc {
class CCTextureUpdateQueue;

class TextureLayerChromiumClient {
public:
    // Called to prepare this layer's texture for compositing. The client may queue a texture
    // upload or copy on the CCTextureUpdateQueue.
    // Returns the texture ID to be used for compositing.
    virtual unsigned prepareTexture(CCTextureUpdateQueue&) = 0;

    // Returns the context that is providing the texture. Used for rate limiting and detecting lost context.
    virtual WebKit::WebGraphicsContext3D* context() = 0;

protected:
    virtual ~TextureLayerChromiumClient() { }
};

}

#endif // TextureLayerChromiumClient_h
