// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEXTURE_LAYER_CLIENT_H_
#define CC_TEXTURE_LAYER_CLIENT_H_

namespace WebKit {
class WebGraphicsContext3D;
}

namespace cc {
class ResourceUpdateQueue;

class TextureLayerClient {
public:
    // Called to prepare this layer's texture for compositing. The client may queue a texture
    // upload or copy on the ResourceUpdateQueue.
    // Returns the texture ID to be used for compositing.
    virtual unsigned prepareTexture(ResourceUpdateQueue&) = 0;

    // Returns the context that is providing the texture. Used for rate limiting and detecting lost context.
    virtual WebKit::WebGraphicsContext3D* context() = 0;

protected:
    virtual ~TextureLayerClient() { }
};

}

#endif  // CC_TEXTURE_LAYER_CLIENT_H_
