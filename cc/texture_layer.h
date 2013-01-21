// Copyright 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEXTURE_LAYER_H_
#define CC_TEXTURE_LAYER_H_

#include <string>

#include "base/callback.h"
#include "cc/cc_export.h"
#include "cc/layer.h"
#include "cc/texture_mailbox.h"

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

    // Used when mailbox names are specified instead of texture IDs.
    static scoped_refptr<TextureLayer> createForMailbox();

    void clearClient() { m_client = 0; }

    virtual scoped_ptr<LayerImpl> createLayerImpl(LayerTreeImpl* treeImpl) OVERRIDE;

    // Sets whether this texture should be Y-flipped at draw time. Defaults to true.
    void setFlipped(bool);

    // Sets a UV transform to be used at draw time. Defaults to (0, 0) and (1, 1).
    void setUV(gfx::PointF topLeft, gfx::PointF bottomRight);

    // Sets an opacity value per vertex. It will be multiplied by the layer opacity value.
    void setVertexOpacity(float bottomLeft, float topLeft, float topRight, float bottomRight);

    // Sets whether the alpha channel is premultiplied or unpremultiplied. Defaults to true.
    void setPremultipliedAlpha(bool);

    // Sets whether this context should rate limit on damage to prevent too many frames from
    // being queued up before the compositor gets a chance to run. Requires a non-nil client.
    // Defaults to false.
    void setRateLimitContext(bool);

    // Code path for plugins which supply their own texture ID.
    void setTextureId(unsigned);

    // Code path for plugins which supply their own texture ID.
    void setTextureMailbox(const TextureMailbox&);

    void willModifyTexture();

    virtual void setNeedsDisplayRect(const gfx::RectF&) OVERRIDE;

    virtual void setLayerTreeHost(LayerTreeHost*) OVERRIDE;
    virtual bool drawsContent() const OVERRIDE;
    virtual void update(ResourceUpdateQueue&, const OcclusionTracker*, RenderingStats&) OVERRIDE;
    virtual void pushPropertiesTo(LayerImpl*) OVERRIDE;
    virtual bool blocksPendingCommit() const OVERRIDE;

    virtual bool canClipSelf() const OVERRIDE;

protected:
    TextureLayer(TextureLayerClient*, bool usesMailbox);
    virtual ~TextureLayer();

private:
    TextureLayerClient* m_client;
    bool m_usesMailbox;

    bool m_flipped;
    gfx::PointF m_uvTopLeft;
    gfx::PointF m_uvBottomRight;
    // [bottom left, top left, top right, bottom right]
    float m_vertexOpacity[4];
    bool m_premultipliedAlpha;
    bool m_rateLimitContext;
    bool m_contextLost;
    bool m_contentCommitted;

    unsigned m_textureId;
    TextureMailbox m_textureMailbox;
};

}
#endif  // CC_TEXTURE_LAYER_H_
