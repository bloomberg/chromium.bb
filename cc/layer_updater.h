// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_LAYER_UPDATER_H_
#define CC_LAYER_UPDATER_H_

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "cc/cc_export.h"

namespace gfx {
class Rect;
class Size;
class Vector2d;
}

namespace cc {

class PrioritizedResource;
class PrioritizedResourceManager;
class ResourceUpdateQueue;
class TextureManager;
struct RenderingStats;

class CC_EXPORT LayerUpdater : public base::RefCounted<LayerUpdater> {
public:
    // Allows updaters to store per-resource update properties.
    class CC_EXPORT Resource {
    public:
        virtual ~Resource();

        PrioritizedResource* texture() { return m_texture.get(); }
        void swapTextureWith(scoped_ptr<PrioritizedResource>& texture);
        // TODO(reveman): partialUpdate should be a property of this class
        // instead of an argument passed to update().
        virtual void update(ResourceUpdateQueue&, const gfx::Rect& sourceRect, const gfx::Vector2d& destOffset, bool partialUpdate, RenderingStats&) = 0;
    protected:
        explicit Resource(scoped_ptr<PrioritizedResource> texture);

    private:
        scoped_ptr<PrioritizedResource> m_texture;

        DISALLOW_COPY_AND_ASSIGN(Resource);
    };

    LayerUpdater() { }

    virtual scoped_ptr<Resource> createResource(PrioritizedResourceManager*) = 0;
    // The |resultingOpaqueRect| gives back a region of the layer that was painted opaque. If the layer is marked opaque in the updater,
    // then this region should be ignored in preference for the entire layer's area.
    virtual void prepareToUpdate(const gfx::Rect& contentRect, const gfx::Size& tileSize, float contentsWidthScale, float contentsHeightScale, gfx::Rect& resultingOpaqueRect, RenderingStats&) { }

    // Set true by the layer when it is known that the entire output is going to be opaque.
    virtual void setOpaque(bool) { }

protected:
    virtual ~LayerUpdater() { }

private:
    friend class base::RefCounted<LayerUpdater>;
};

}  // namespace cc

#endif  // CC_LAYER_UPDATER_H_
