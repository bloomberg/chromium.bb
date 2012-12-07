// Copyright 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEXTURE_LAYER_H_
#define CC_TEXTURE_LAYER_H_

#include "cc/cc_export.h"
#include "cc/layer.h"

namespace WebKit {
class WebGraphicsContext3D;
}

namespace cc {

class TextureLayerClient;

// A Layer containing a the rendered output of a plugin instance.
class CC_EXPORT TextureLayer : public Layer {
public:
    // If this texture layer requires special preparation logic for each frame driven by
    // the compositor, pass in a non-nil client. Pass in a nil client pointer if texture updates
    // are driven by an external process.
    static scoped_refptr<TextureLayer> create(TextureLayerClient*);

    void clearClient() { m_client = 0; }

    virtual scoped_ptr<LayerImpl> createLayerImpl(LayerTreeHostImpl* hostImpl) OVERRIDE;

    // Sets whether this texture should be Y-flipped at draw time. Defaults to true.
    void setFlipped(bool);

    // Sets a UV transform to be used at draw time. Defaults to (0, 0, 1, 1).
    void setUVRect(const gfx::RectF&);

    // Sets whether the alpha channel is premultiplied or unpremultiplied. Defaults to true.
    void setPremultipliedAlpha(bool);

    // Sets whether this context should rate limit on damage to prevent too many frames from
    // being queued up before the compositor gets a chance to run. Requires a non-nil client.
    // Defaults to false.
    void setRateLimitContext(bool);

    // Code path for plugins which supply their own texture ID.
    void setTextureId(unsigned);

    void willModifyTexture();

    virtual void setNeedsDisplayRect(const gfx::RectF&) OVERRIDE;

    virtual void setLayerTreeHost(LayerTreeHost*) OVERRIDE;
    virtual bool drawsContent() const OVERRIDE;
    virtual void update(ResourceUpdateQueue&, const OcclusionTracker*, RenderingStats&) OVERRIDE;
    virtual void pushPropertiesTo(LayerImpl*) OVERRIDE;

protected:
    explicit TextureLayer(TextureLayerClient*);
    virtual ~TextureLayer();

private:
    TextureLayerClient* m_client;

    bool m_flipped;
    gfx::RectF m_uvRect;
    bool m_premultipliedAlpha;
    bool m_rateLimitContext;
    bool m_contextLost;
    bool m_contentCommitted;

    unsigned m_textureId;
};

}
#endif  // CC_TEXTURE_LAYER_H_
