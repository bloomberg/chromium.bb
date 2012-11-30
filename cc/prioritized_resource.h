// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_PRIORITIZED_RESOURCE_H_
#define CC_PRIORITIZED_RESOURCE_H_

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "cc/cc_export.h"
#include "cc/priority_calculator.h"
#include "cc/resource.h"
#include "cc/resource_provider.h"
#include "third_party/khronos/GLES2/gl2.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/size.h"
#include "ui/gfx/vector2d.h"

namespace cc {

class PrioritizedResourceManager;
class Proxy;

class CC_EXPORT PrioritizedResource {
public:
    static scoped_ptr<PrioritizedResource> create(PrioritizedResourceManager* manager, gfx::Size size, GLenum format)
    {
        return make_scoped_ptr(new PrioritizedResource(manager, size, format));
    }
    static scoped_ptr<PrioritizedResource> create(PrioritizedResourceManager* manager)
    {
        return make_scoped_ptr(new PrioritizedResource(manager, gfx::Size(), 0));
    }
    ~PrioritizedResource();

    // Texture properties. Changing these causes the backing texture to be lost.
    // Setting these to the same value is a no-op.
    void setTextureManager(PrioritizedResourceManager*);
    PrioritizedResourceManager* resourceManager() { return m_manager; }
    void setDimensions(gfx::Size, GLenum format);
    GLenum format() const { return m_format; }
    gfx::Size size() const { return m_size; }
    size_t bytes() const { return m_bytes; }
    bool contentsSwizzled() const { return m_contentsSwizzled; }

    // Set priority for the requested texture.
    void setRequestPriority(int priority) { m_priority = priority; }
    int requestPriority() const { return m_priority; }

    // After PrioritizedResource::prioritizeTextures() is called, this returns
    // if the the request succeeded and this texture can be acquired for use.
    bool canAcquireBackingTexture() const { return m_isAbovePriorityCutoff; }

    // This returns whether we still have a backing texture. This can continue
    // to be true even after canAcquireBackingTexture() becomes false. In this
    // case the texture can be used but shouldn't be updated since it will get
    // taken away "soon".
    bool haveBackingTexture() const { return !!backing(); }

    bool backingResourceWasEvicted() const;

    // If canAcquireBackingTexture() is true acquireBackingTexture() will acquire
    // a backing texture for use. Call this whenever the texture is actually needed.
    void acquireBackingTexture(ResourceProvider*);

    // FIXME: Request late is really a hack for when we are totally out of memory
    //        (all textures are visible) but we can still squeeze into the limit
    //        by not painting occluded textures. In this case the manager
    //        refuses all visible textures and requestLate() will enable
    //        canAcquireBackingTexture() on a call-order basis. We might want to
    //        just remove this in the future (carefully) and just make sure we don't
    //        regress OOMs situations.
    bool requestLate();

    // Update pixels of backing resource from image. This functions will aquire the backing if needed.
    void setPixels(ResourceProvider*, const uint8_t* image, const gfx::Rect& imageRect, const gfx::Rect& sourceRect, const gfx::Vector2d& destOffset);

    ResourceProvider::ResourceId resourceId() const;

    // Self-managed textures are accounted for when prioritizing other textures,
    // but they are not allocated/recycled/deleted, so this needs to be done
    // externally. canAcquireBackingTexture() indicates if the texture would have
    // been allowed given its priority.
    void setIsSelfManaged(bool isSelfManaged) { m_isSelfManaged = isSelfManaged; }
    bool isSelfManaged() { return m_isSelfManaged; }
    void setToSelfManagedMemoryPlaceholder(size_t bytes);

    void returnBackingTexture();

private:
    friend class PrioritizedResourceManager;
    friend class PrioritizedResourceTest;

    class Backing : public Resource {
    public:
        Backing(unsigned id, ResourceProvider*, gfx::Size, GLenum format);
        ~Backing();
        void updatePriority();
        void updateInDrawingImplTree();

        PrioritizedResource* owner() { return m_owner; }
        bool canBeRecycled() const;
        int requestPriorityAtLastPriorityUpdate() const { return m_priorityAtLastPriorityUpdate; }
        bool wasAbovePriorityCutoffAtLastPriorityUpdate() const { return m_wasAbovePriorityCutoffAtLastPriorityUpdate; }
        bool inDrawingImplTree() const { return m_inDrawingImplTree; }

        void deleteResource(ResourceProvider*);
        bool resourceHasBeenDeleted() const;

    private:
        const Proxy* proxy() const;

        friend class PrioritizedResource;
        friend class PrioritizedResourceManager;
        PrioritizedResource* m_owner;
        int m_priorityAtLastPriorityUpdate;
        bool m_wasAbovePriorityCutoffAtLastPriorityUpdate;

        // Set if this is currently-drawing impl tree.
        bool m_inDrawingImplTree;

        bool m_resourceHasBeenDeleted;

#ifndef NDEBUG
        ResourceProvider* m_resourceProvider;
#endif
        DISALLOW_COPY_AND_ASSIGN(Backing);
    };

    PrioritizedResource(PrioritizedResourceManager*, gfx::Size, GLenum format);

    bool isAbovePriorityCutoff() { return m_isAbovePriorityCutoff; }
    void setAbovePriorityCutoff(bool isAbovePriorityCutoff) { m_isAbovePriorityCutoff = isAbovePriorityCutoff; }
    void setManagerInternal(PrioritizedResourceManager* manager) { m_manager = manager; }

    Backing* backing() const { return m_backing; }
    void link(Backing*);
    void unlink();

    gfx::Size m_size;
    GLenum m_format;
    size_t m_bytes;
    bool m_contentsSwizzled;

    int m_priority;
    bool m_isAbovePriorityCutoff;
    bool m_isSelfManaged;

    Backing* m_backing;
    PrioritizedResourceManager* m_manager;

    DISALLOW_COPY_AND_ASSIGN(PrioritizedResource);
};

}  // namespace cc

#endif  // CC_PRIORITIZED_RESOURCE_H_
