// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#ifndef LayerTextureUpdater_h
#define LayerTextureUpdater_h

#if USE(ACCELERATED_COMPOSITING)

#include "CCPrioritizedTexture.h"
#include "GraphicsTypes3D.h"
#include <wtf/RefCounted.h>

namespace WebCore {

class IntRect;
class IntSize;
class TextureManager;
struct CCRenderingStats;

class LayerTextureUpdater : public RefCounted<LayerTextureUpdater> {
public:
    // Allows texture uploaders to store per-tile resources.
    class Texture {
    public:
        virtual ~Texture() { }

        CCPrioritizedTexture* texture() { return m_texture.get(); }
        void swapTextureWith(OwnPtr<CCPrioritizedTexture>& texture) { m_texture.swap(texture); }
        virtual void prepareRect(const IntRect& /* sourceRect */, CCRenderingStats&) { }
        virtual void updateRect(CCResourceProvider*, const IntRect& sourceRect, const IntSize& destOffset) = 0;
    protected:
        explicit Texture(PassOwnPtr<CCPrioritizedTexture> texture) : m_texture(texture) { }

    private:
        OwnPtr<CCPrioritizedTexture> m_texture;
    };

    LayerTextureUpdater()
    {
        turnOffVerifier(); // In the component build we don't have WTF threading initialized in this DLL so the thread verifier explodes.
    }

    virtual ~LayerTextureUpdater() { }

    enum SampledTexelFormat {
        SampledTexelFormatRGBA,
        SampledTexelFormatBGRA,
        SampledTexelFormatInvalid,
    };
    virtual PassOwnPtr<Texture> createTexture(CCPrioritizedTextureManager*) = 0;
    // Returns the format of the texel uploaded by this interface.
    // This format should not be confused by texture internal format.
    // This format specifies the component order in the sampled texel.
    // If the format is TexelFormatBGRA, vec4.x is blue and vec4.z is red.
    virtual SampledTexelFormat sampledTexelFormat(GC3Denum textureFormat) = 0;
    // The |resultingOpaqueRect| gives back a region of the layer that was painted opaque. If the layer is marked opaque in the updater,
    // then this region should be ignored in preference for the entire layer's area.
    virtual void prepareToUpdate(const IntRect& contentRect, const IntSize& tileSize, float contentsWidthScale, float contentsHeightScale, IntRect& resultingOpaqueRect, CCRenderingStats&) { }

    // Set true by the layer when it is known that the entire output is going to be opaque.
    virtual void setOpaque(bool) { }
};

} // namespace WebCore
#endif // USE(ACCELERATED_COMPOSITING)
#endif // LayerTextureUpdater_h
