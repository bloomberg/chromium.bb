// Copyright 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TextureLayerChromium_h
#define TextureLayerChromium_h

#if USE(ACCELERATED_COMPOSITING)

#include "LayerChromium.h"

namespace WebKit {
class WebGraphicsContext3D;
}

namespace cc {

class TextureLayerChromiumClient;

// A Layer containing a the rendered output of a plugin instance.
class TextureLayerChromium : public LayerChromium {
public:
    // If this texture layer requires special preparation logic for each frame driven by
    // the compositor, pass in a non-nil client. Pass in a nil client pointer if texture updates
    // are driven by an external process.
    static PassRefPtr<TextureLayerChromium> create(TextureLayerChromiumClient*);
    virtual ~TextureLayerChromium();

    void clearClient() { m_client = 0; }

    virtual PassOwnPtr<CCLayerImpl> createCCLayerImpl() OVERRIDE;

    // Sets whether this texture should be Y-flipped at draw time. Defaults to true.
    void setFlipped(bool);

    // Sets a UV transform to be used at draw time. Defaults to (0, 0, 1, 1).
    void setUVRect(const FloatRect&);

    // Sets whether the alpha channel is premultiplied or unpremultiplied. Defaults to true.
    void setPremultipliedAlpha(bool);

    // Sets whether this context should rate limit on damage to prevent too many frames from
    // being queued up before the compositor gets a chance to run. Requires a non-nil client.
    // Defaults to false.
    void setRateLimitContext(bool);

    // Code path for plugins which supply their own texture ID.
    void setTextureId(unsigned);

    void willModifyTexture();

    virtual void setNeedsDisplayRect(const FloatRect&) OVERRIDE;

    virtual void setLayerTreeHost(CCLayerTreeHost*) OVERRIDE;
    virtual bool drawsContent() const OVERRIDE;
    virtual void update(CCTextureUpdateQueue&, const CCOcclusionTracker*, CCRenderingStats&) OVERRIDE;
    virtual void pushPropertiesTo(CCLayerImpl*) OVERRIDE;

protected:
    explicit TextureLayerChromium(TextureLayerChromiumClient*);

private:
    TextureLayerChromiumClient* m_client;

    bool m_flipped;
    FloatRect m_uvRect;
    bool m_premultipliedAlpha;
    bool m_rateLimitContext;
    bool m_contextLost;

    unsigned m_textureId;
};

}
#endif // USE(ACCELERATED_COMPOSITING)

#endif
