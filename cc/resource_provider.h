// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_RESOURCE_PROVIDER_H_
#define CC_RESOURCE_PROVIDER_H_

#include <deque>
#include <set>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/hash_tables.h"
#include "base/memory/scoped_ptr.h"
#include "base/threading/thread_checker.h"
#include "cc/cc_export.h"
#include "cc/output_surface.h"
#include "cc/texture_copier.h"
#include "cc/texture_mailbox.h"
#include "cc/transferable_resource.h"
#include "third_party/khronos/GLES2/gl2.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "ui/gfx/size.h"

namespace WebKit {
class WebGraphicsContext3D;
}

namespace gfx {
class Rect;
class Vector2d;
}

namespace cc {

class ContextProvider;
class TextureUploader;

// This class is not thread-safe and can only be called from the thread it was
// created on (in practice, the impl thread).
class CC_EXPORT ResourceProvider {
public:
    typedef unsigned ResourceId;
    typedef std::vector<ResourceId> ResourceIdArray;
    typedef std::set<ResourceId> ResourceIdSet;
    typedef base::hash_map<ResourceId, ResourceId> ResourceIdMap;
    enum TextureUsageHint {
        TextureUsageAny,
        TextureUsageFramebuffer,
    };
    enum ResourceType {
        GLTexture = 1,
        Bitmap,
    };

    static scoped_ptr<ResourceProvider> create(OutputSurface*);

    virtual ~ResourceProvider();

    WebKit::WebGraphicsContext3D* graphicsContext3D();
    TextureCopier* textureCopier() const { return m_textureCopier.get(); }
    int maxTextureSize() const { return m_maxTextureSize; }
    GLenum bestTextureFormat() const { return m_bestTextureFormat; }
    unsigned numResources() const { return m_resources.size(); }

    // Checks whether a resource is in use by a consumer.
    bool inUseByConsumer(ResourceId);


    // Producer interface.

    void setDefaultResourceType(ResourceType type) { m_defaultResourceType = type; }
    ResourceType defaultResourceType() const { return m_defaultResourceType; }
    ResourceType resourceType(ResourceId);

    // Creates a resource of the default resource type.
    ResourceId createResource(const gfx::Size&, GLenum format, TextureUsageHint);

    // Creates a resource which is tagged as being managed for GPU memory accounting purposes.
    ResourceId createManagedResource(const gfx::Size&, GLenum format, TextureUsageHint);

    // You can also explicitly create a specific resource type.
    ResourceId createGLTexture(const gfx::Size&, GLenum format, GLenum texturePool, TextureUsageHint);

    ResourceId createBitmap(const gfx::Size&);
    // Wraps an external texture into a GL resource.
    ResourceId createResourceFromExternalTexture(unsigned textureId);

    // Wraps an external texture mailbox into a GL resource.
    ResourceId createResourceFromTextureMailbox(const TextureMailbox&);

    void deleteResource(ResourceId);

    // Update pixels from image, copying sourceRect (in image) into destRect (in the resource).
    void setPixels(ResourceId, const uint8_t* image, const gfx::Rect& imageRect, const gfx::Rect& sourceRect, const gfx::Vector2d& destOffset);

    // Check upload status.
    size_t numBlockingUploads();
    void markPendingUploadsAsNonBlocking();
    double estimatedUploadsPerSecond();
    void flushUploads();
    void releaseCachedData();

    // Flush all context operations, kicking uploads and ensuring ordering with
    // respect to other contexts.
    void flush();

    // Only flush the command buffer if supported.
    // Returns true if the shallow flush occurred, false otherwise.
    bool shallowFlushIfSupported();

    // Creates accounting for a child. Returns a child ID.
    int createChild();

    // Destroys accounting for the child, deleting all accounted resources.
    void destroyChild(int child);

    // Gets the child->parent resource ID map.
    const ResourceIdMap& getChildToParentMap(int child) const;

    // Prepares resources to be transfered to the parent, moving them to
    // mailboxes and serializing meta-data into TransferableResources.
    // Resources are not removed from the ResourceProvider, but are marked as
    // "in use".
    void prepareSendToParent(const ResourceIdArray& resources, TransferableResourceArray* transferableResources);

    // Prepares resources to be transfered back to the child, moving them to
    // mailboxes and serializing meta-data into TransferableResources.
    // Resources are removed from the ResourceProvider. Note: the resource IDs
    // passed are in the parent namespace and will be translated to the child
    // namespace when returned.
    void prepareSendToChild(int child, const ResourceIdArray& resources, TransferableResourceArray* transferableResources);

    // Receives resources from a child, moving them from mailboxes. Resource IDs
    // passed are in the child namespace, and will be translated to the parent
    // namespace, added to the child->parent map.
    // NOTE: if the sync_point is set on any TransferableResource, this will
    // wait on it.
    void receiveFromChild(int child, const TransferableResourceArray& transferableResources);

    // Receives resources from the parent, moving them from mailboxes. Resource IDs
    // passed are in the child namespace.
    // NOTE: if the sync_point is set on any TransferableResource, this will
    // wait on it.
    void receiveFromParent(const TransferableResourceArray& transferableResources);

    // Bind the given GL resource to a texture target for sampling using the
    // specified filter for both minification and magnification. The resource
    // must be locked for reading.
    void bindForSampling(ResourceProvider::ResourceId, GLenum target, GLenum filter);

    // The following lock classes are part of the ResourceProvider API and are
    // needed to read and write the resource contents. The user must ensure
    // that they only use GL locks on GL resources, etc, and this is enforced
    // by assertions.
    class CC_EXPORT ScopedReadLockGL {
    public:
        ScopedReadLockGL(ResourceProvider*, ResourceProvider::ResourceId);
        ~ScopedReadLockGL();

        unsigned textureId() const { return m_textureId; }

    private:
        ResourceProvider* m_resourceProvider;
        ResourceProvider::ResourceId m_resourceId;
        unsigned m_textureId;

        DISALLOW_COPY_AND_ASSIGN(ScopedReadLockGL);
    };

    class CC_EXPORT ScopedSamplerGL : public ScopedReadLockGL {
    public:
        ScopedSamplerGL(ResourceProvider*, ResourceProvider::ResourceId, GLenum target, GLenum filter);

    private:
        DISALLOW_COPY_AND_ASSIGN(ScopedSamplerGL);
    };

    class CC_EXPORT ScopedWriteLockGL {
    public:
        ScopedWriteLockGL(ResourceProvider*, ResourceProvider::ResourceId);
        ~ScopedWriteLockGL();

        unsigned textureId() const { return m_textureId; }

    private:
        ResourceProvider* m_resourceProvider;
        ResourceProvider::ResourceId m_resourceId;
        unsigned m_textureId;

        DISALLOW_COPY_AND_ASSIGN(ScopedWriteLockGL);
    };

    class CC_EXPORT ScopedReadLockSoftware {
    public:
        ScopedReadLockSoftware(ResourceProvider*, ResourceProvider::ResourceId);
        ~ScopedReadLockSoftware();

        const SkBitmap* skBitmap() const { return &m_skBitmap; }

    private:
        ResourceProvider* m_resourceProvider;
        ResourceProvider::ResourceId m_resourceId;
        SkBitmap m_skBitmap;

        DISALLOW_COPY_AND_ASSIGN(ScopedReadLockSoftware);
    };

    class CC_EXPORT ScopedWriteLockSoftware {
    public:
        ScopedWriteLockSoftware(ResourceProvider*, ResourceProvider::ResourceId);
        ~ScopedWriteLockSoftware();

        SkCanvas* skCanvas() { return m_skCanvas.get(); }

    private:
        ResourceProvider* m_resourceProvider;
        ResourceProvider::ResourceId m_resourceId;
        SkBitmap m_skBitmap;
        scoped_ptr<SkCanvas> m_skCanvas;

        DISALLOW_COPY_AND_ASSIGN(ScopedWriteLockSoftware);
    };

    class Fence : public base::RefCounted<Fence> {
    public:
        virtual bool hasPassed() = 0;
    protected:
        friend class base::RefCounted<Fence>;
        virtual ~Fence() {}
    };

    // Acquire pixel buffer for resource. The pixel buffer can be used to
    // set resource pixels without performing unnecessary copying.
    void acquirePixelBuffer(ResourceId id);
    void releasePixelBuffer(ResourceId id);

    // Map/unmap the acquired pixel buffer.
    uint8_t* mapPixelBuffer(ResourceId id);
    void unmapPixelBuffer(ResourceId id);

    // Update pixels from acquired pixel buffer.
    void setPixelsFromBuffer(ResourceId id);

    // Asynchronously update pixels from acquired pixel buffer.
    void beginSetPixels(ResourceId id);
    bool didSetPixelsComplete(ResourceId id);
    void abortSetPixels(ResourceId id);

    // For tests only! This prevents detecting uninitialized reads.
    // Use setPixels or lockForWrite to allocate implicitly.
    void allocateForTesting(ResourceId id);

    // Sets the current read fence. If a resource is locked for read
    // and has read fences enabled, the resource will not allow writes
    // until this fence has passed.
    void setReadLockFence(scoped_refptr<Fence> fence) { m_currentReadLockFence = fence; }
    Fence* getReadLockFence() { return m_currentReadLockFence; }

    // Enable read lock fences for a specific resource.
    void enableReadLockFences(ResourceProvider::ResourceId, bool enable);

    // Indicates if we can currently lock this resource for write.
    bool canLockForWrite(ResourceId);

    cc::ContextProvider* offscreenContextProvider() { return m_offscreenContextProvider.get(); }
    void setOffscreenContextProvider(scoped_refptr<cc::ContextProvider> offscreenContextProvider);

private:
    struct Resource {
        Resource();
        ~Resource();
        Resource(unsigned textureId, const gfx::Size& size, GLenum format, GLenum filter);
        Resource(uint8_t* pixels, const gfx::Size& size, GLenum format, GLenum filter);

        unsigned glId;
        // Pixel buffer used for set pixels without unnecessary copying.
        unsigned glPixelBufferId;
        // Query used to determine when asynchronous set pixels complete.
        unsigned glUploadQueryId;
        TextureMailbox mailbox;
        uint8_t* pixels;
        uint8_t* pixelBuffer;
        int lockForReadCount;
        bool lockedForWrite;
        bool external;
        bool exported;
        bool markedForDeletion;
        bool pendingSetPixels;
        bool allocated;
        bool enableReadLockFences;
        scoped_refptr<Fence> readLockFence;
        gfx::Size size;
        GLenum format;
        // TODO(skyostil): Use a separate sampler object for filter state.
        GLenum filter;
        ResourceType type;
    };
    typedef base::hash_map<ResourceId, Resource> ResourceMap;
    struct Child {
        Child();
        ~Child();

        ResourceIdMap childToParentMap;
        ResourceIdMap parentToChildMap;
    };
    typedef base::hash_map<int, Child> ChildMap;

    bool readLockFenceHasPassed(Resource* resource) { return !resource->readLockFence ||
                                                              resource->readLockFence->hasPassed(); }

    explicit ResourceProvider(OutputSurface*);
    bool initialize();

    const Resource* lockForRead(ResourceId);
    void unlockForRead(ResourceId);
    const Resource* lockForWrite(ResourceId);
    void unlockForWrite(ResourceId);
    static void populateSkBitmapWithResource(SkBitmap*, const Resource*);

    bool transferResource(WebKit::WebGraphicsContext3D*, ResourceId, TransferableResource*);
    void deleteResourceInternal(ResourceMap::iterator it);
    void lazyAllocate(Resource*);

    OutputSurface* m_outputSurface;
    ResourceId m_nextId;
    ResourceMap m_resources;
    int m_nextChild;
    ChildMap m_children;

    ResourceType m_defaultResourceType;
    bool m_useTextureStorageExt;
    bool m_useTextureUsageHint;
    bool m_useShallowFlush;
    scoped_ptr<TextureUploader> m_textureUploader;
    scoped_ptr<AcceleratedTextureCopier> m_textureCopier;
    int m_maxTextureSize;
    GLenum m_bestTextureFormat;

    scoped_refptr<cc::ContextProvider> m_offscreenContextProvider;

    base::ThreadChecker m_threadChecker;

    scoped_refptr<Fence> m_currentReadLockFence;

    DISALLOW_COPY_AND_ASSIGN(ResourceProvider);
};

}

#endif  // CC_RESOURCE_PROVIDER_H_
