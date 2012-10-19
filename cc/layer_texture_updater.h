// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#ifndef LayerTextureUpdater_h
#define LayerTextureUpdater_h

#include "CCPrioritizedTexture.h"
#include "base/memory/ref_counted.h"
#include "third_party/khronos/GLES2/gl2.h"

namespace cc {

class IntRect;
class IntSize;
class TextureManager;
struct RenderingStats;
class TextureUpdateQueue;

class LayerTextureUpdater : public base::RefCounted<LayerTextureUpdater> {
public:
    // Allows texture uploaders to store per-tile resources.
    class Texture {
    public:
        virtual ~Texture();

        PrioritizedTexture* texture() { return m_texture.get(); }
        void swapTextureWith(scoped_ptr<PrioritizedTexture>& texture) { m_texture.swap(texture); }
        // TODO(reveman): partialUpdate should be a property of this class
        // instead of an argument passed to update().
        virtual void update(TextureUpdateQueue&, const IntRect& sourceRect, const IntSize& destOffset, bool partialUpdate, RenderingStats&) = 0;
    protected:
        explicit Texture(scoped_ptr<PrioritizedTexture> texture);

    private:
        scoped_ptr<PrioritizedTexture> m_texture;
    };

    LayerTextureUpdater() { }

    enum SampledTexelFormat {
        SampledTexelFormatRGBA,
        SampledTexelFormatBGRA,
        SampledTexelFormatInvalid,
    };
    virtual scoped_ptr<Texture> createTexture(PrioritizedTextureManager*) = 0;
    // Returns the format of the texel uploaded by this interface.
    // This format should not be confused by texture internal format.
    // This format specifies the component order in the sampled texel.
    // If the format is TexelFormatBGRA, vec4.x is blue and vec4.z is red.
    virtual SampledTexelFormat sampledTexelFormat(GLenum textureFormat) = 0;
    // The |resultingOpaqueRect| gives back a region of the layer that was painted opaque. If the layer is marked opaque in the updater,
    // then this region should be ignored in preference for the entire layer's area.
    virtual void prepareToUpdate(const IntRect& contentRect, const IntSize& tileSize, float contentsWidthScale, float contentsHeightScale, IntRect& resultingOpaqueRect, RenderingStats&) { }

    // Set true by the layer when it is known that the entire output is going to be opaque.
    virtual void setOpaque(bool) { }

protected:
    virtual ~LayerTextureUpdater() { }

private:
    friend class base::RefCounted<LayerTextureUpdater>;
};

} // namespace cc
#endif // LayerTextureUpdater_h
