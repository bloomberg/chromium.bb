// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CCPrioritizedTexture_h
#define CCPrioritizedTexture_h

#include "IntRect.h"
#include "IntSize.h"
#include "base/basictypes.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "cc/priority_calculator.h"
#include "cc/resource_provider.h"
#include "cc/texture.h"
#include "third_party/khronos/GLES2/gl2.h"

namespace cc {

class PrioritizedTextureManager;

class PrioritizedTexture {
public:
    static scoped_ptr<PrioritizedTexture> create(PrioritizedTextureManager* manager, IntSize size, GLenum format)
    {
        return make_scoped_ptr(new PrioritizedTexture(manager, size, format));
    }
    static scoped_ptr<PrioritizedTexture> create(PrioritizedTextureManager* manager)
    {
        return make_scoped_ptr(new PrioritizedTexture(manager, IntSize(), 0));
    }
    ~PrioritizedTexture();

    // Texture properties. Changing these causes the backing texture to be lost.
    // Setting these to the same value is a no-op.
    void setTextureManager(PrioritizedTextureManager*);
    PrioritizedTextureManager* textureManager() { return m_manager; }
    void setDimensions(IntSize, GLenum format);
    GLenum format() const { return m_format; }
    IntSize size() const { return m_size; }
    size_t bytes() const { return m_bytes; }
    bool contentsSwizzled() const { return m_contentsSwizzled; }

    // Set priority for the requested texture. 
    void setRequestPriority(int priority) { m_priority = priority; }
    int requestPriority() const { return m_priority; }

    // After PrioritizedTexture::prioritizeTextures() is called, this returns
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

    // Uploads pixels into the backing resource. This functions will aquire the backing if needed.
    void upload(ResourceProvider*, const uint8_t* image, const IntRect& imageRect, const IntRect& sourceRect, const IntSize& destOffset);

    ResourceProvider::ResourceId resourceId() const;

    // Self-managed textures are accounted for when prioritizing other textures,
    // but they are not allocated/recycled/deleted, so this needs to be done
    // externally. canAcquireBackingTexture() indicates if the texture would have
    // been allowed given its priority.
    void setIsSelfManaged(bool isSelfManaged) { m_isSelfManaged = isSelfManaged; }
    bool isSelfManaged() { return m_isSelfManaged; }
    void setToSelfManagedMemoryPlaceholder(size_t bytes);

private:
    friend class PrioritizedTextureManager;
    friend class PrioritizedTextureTest;

    class Backing : public Texture {
    public:
        Backing(unsigned id, ResourceProvider*, IntSize, GLenum format);
        ~Backing();
        void updatePriority();
        void updateInDrawingImplTree();

        PrioritizedTexture* owner() { return m_owner; }
        bool canBeRecycled() const;
        int requestPriorityAtLastPriorityUpdate() const { return m_priorityAtLastPriorityUpdate; }
        bool wasAbovePriorityCutoffAtLastPriorityUpdate() const { return m_wasAbovePriorityCutoffAtLastPriorityUpdate; }
        bool inDrawingImplTree() const { return m_inDrawingImplTree; }

        void deleteResource(ResourceProvider*);
        bool resourceHasBeenDeleted() const;

    private:
        friend class PrioritizedTexture;
        PrioritizedTexture* m_owner;
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

    PrioritizedTexture(PrioritizedTextureManager*, IntSize, GLenum format);

    bool isAbovePriorityCutoff() { return m_isAbovePriorityCutoff; }
    void setAbovePriorityCutoff(bool isAbovePriorityCutoff) { m_isAbovePriorityCutoff = isAbovePriorityCutoff; }
    void setManagerInternal(PrioritizedTextureManager* manager) { m_manager = manager; }

    Backing* backing() const { return m_backing; }
    void link(Backing*);
    void unlink();

    IntSize m_size;
    GLenum m_format;
    size_t m_bytes;
    bool m_contentsSwizzled;

    int m_priority;
    bool m_isAbovePriorityCutoff;
    bool m_isSelfManaged;

    Backing* m_backing;
    PrioritizedTextureManager* m_manager;

    DISALLOW_COPY_AND_ASSIGN(PrioritizedTexture);
};

}  // namespace cc

#endif
