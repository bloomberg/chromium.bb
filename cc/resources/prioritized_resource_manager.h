// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_RESOURCES_PRIORITIZED_RESOURCE_MANAGER_H_
#define CC_RESOURCES_PRIORITIZED_RESOURCE_MANAGER_H_

#include <list>
#include <vector>

#include "base/basictypes.h"
#include "base/hash_tables.h"
#include "base/memory/scoped_ptr.h"
#include "base/synchronization/lock.h"
#include "cc/base/cc_export.h"
#include "cc/resources/prioritized_resource.h"
#include "cc/resources/priority_calculator.h"
#include "cc/resources/resource.h"
#include "cc/trees/proxy.h"
#include "third_party/khronos/GLES2/gl2.h"
#include "ui/gfx/size.h"

#if defined(COMPILER_GCC)
namespace BASE_HASH_NAMESPACE {
template<>
struct hash<cc::PrioritizedResource*> {
  size_t operator()(cc::PrioritizedResource* ptr) const {
    return hash<size_t>()(reinterpret_cast<size_t>(ptr));
  }
};
} // namespace BASE_HASH_NAMESPACE
#endif // COMPILER

namespace cc {

class PriorityCalculator;
class Proxy;

class CC_EXPORT PrioritizedResourceManager {
public:
    static scoped_ptr<PrioritizedResourceManager> create(const Proxy* proxy)
    {
        return make_scoped_ptr(new PrioritizedResourceManager(proxy));
    }
    scoped_ptr<PrioritizedResource> createTexture(gfx::Size size, GLenum format)
    {
        return make_scoped_ptr(new PrioritizedResource(this, size, format));
    }
    ~PrioritizedResourceManager();

    typedef std::list<PrioritizedResource::Backing*> BackingList;

    // FIXME (http://crbug.com/137094): This 64MB default is a straggler from the
    // old texture manager and is just to give us a default memory allocation before
    // we get a callback from the GPU memory manager. We should probaby either:
    // - wait for the callback before rendering anything instead
    // - push this into the GPU memory manager somehow.
    static size_t defaultMemoryAllocationLimit() { return 64 * 1024 * 1024; }

    // memoryUseBytes() describes the number of bytes used by existing allocated textures.
    // memoryAboveCutoffBytes() describes the number of bytes that would be used if all
    // textures that are above the cutoff were allocated.
    // memoryUseBytes() <= memoryAboveCutoffBytes() should always be true.
    size_t memoryUseBytes() const { return m_memoryUseBytes; }
    size_t memoryAboveCutoffBytes() const { return m_memoryAboveCutoffBytes; }
    size_t memoryForSelfManagedTextures() const { return m_maxMemoryLimitBytes - m_memoryAvailableBytes; }

    void setMaxMemoryLimitBytes(size_t bytes) { m_maxMemoryLimitBytes = bytes; }
    size_t maxMemoryLimitBytes() const { return m_maxMemoryLimitBytes; }

    // Sepecify a external priority cutoff. Only textures that have a strictly higher priority
    // than this cutoff will be allowed.
    void setExternalPriorityCutoff(int priorityCutoff) { m_externalPriorityCutoff = priorityCutoff; }

    // Return the amount of texture memory required at particular cutoffs.
    size_t memoryVisibleBytes() const;
    size_t memoryVisibleAndNearbyBytes() const;

    void prioritizeTextures();
    void clearPriorities();

    // Delete contents textures' backing resources until they use only bytesLimit bytes. This may
    // be called on the impl thread while the main thread is running. Returns true if resources are
    // indeed evicted as a result of this call.
    bool reduceMemoryOnImplThread(size_t limitBytes, int priorityCutoff, ResourceProvider*);

    // Delete contents textures' backing resources that can be recycled. This
    // may be called on the impl thread while the main thread is running.
    void reduceWastedMemoryOnImplThread(ResourceProvider*);

    // Returns true if there exist any textures that are linked to backings that have had their
    // resources evicted. Only when we commit a tree that has no textures linked to evicted backings
    // may we allow drawing. After an eviction, this will not become true until
    // unlinkAndClearEvictedBackings is called.
    bool linkedEvictedBackingsExist() const;

    // Unlink the list of contents textures' backings from their owning textures and delete the evicted
    // backings' structures. This is called just before updating layers, and is only ever called on the
    // main thread.
    void unlinkAndClearEvictedBackings();

    bool requestLate(PrioritizedResource*);

    void reduceWastedMemory(ResourceProvider*);
    void reduceMemory(ResourceProvider*);
    void clearAllMemory(ResourceProvider*);

    void acquireBackingTextureIfNeeded(PrioritizedResource*, ResourceProvider*);

    void registerTexture(PrioritizedResource*);
    void unregisterTexture(PrioritizedResource*);
    void returnBackingTexture(PrioritizedResource*);

    // Update all backings' priorities from their owning texture.
    void pushTexturePrioritiesToBackings();

    // Mark all textures' backings as being in the drawing impl tree.
    void updateBackingsInDrawingImplTree();

    const Proxy* proxyForDebug() const;

private:
    friend class PrioritizedResourceTest;

    enum EvictionPolicy {
        EvictOnlyRecyclable,
        EvictAnything,
    };
    enum UnlinkPolicy {
        DoNotUnlinkBackings,
        UnlinkBackings,
    };

    // Compare textures. Highest priority first.
    static inline bool compareTextures(PrioritizedResource* a, PrioritizedResource* b)
    {
        if (a->request_priority() == b->request_priority())
            return a < b;
        return PriorityCalculator::priority_is_higher(a->request_priority(), b->request_priority());
    }
    // Compare backings. Lowest priority first.
    static inline bool compareBackings(PrioritizedResource::Backing* a, PrioritizedResource::Backing* b)
    {
        // Make textures that can be recycled appear first
        if (a->CanBeRecycled() != b->CanBeRecycled())
            return (a->CanBeRecycled() > b->CanBeRecycled());
        // Then sort by being above or below the priority cutoff.
        if (a->was_above_priority_cutoff_at_last_priority_update() != b->was_above_priority_cutoff_at_last_priority_update())
            return (a->was_above_priority_cutoff_at_last_priority_update() < b->was_above_priority_cutoff_at_last_priority_update());
        // Then sort by priority (note that backings that no longer have owners will
        // always have the lowest priority)
        if (a->request_priority_at_last_priority_update() != b->request_priority_at_last_priority_update())
            return PriorityCalculator::priority_is_lower(a->request_priority_at_last_priority_update(), b->request_priority_at_last_priority_update());
        // Finally sort by being in the impl tree versus being completely unreferenced
        if (a->in_drawing_impl_tree() != b->in_drawing_impl_tree())
            return (a->in_drawing_impl_tree() < b->in_drawing_impl_tree());
        return a < b;
    }

    PrioritizedResourceManager(const Proxy* proxy);

    bool evictBackingsToReduceMemory(size_t limitBytes,
                                     int priorityCutoff,
                                     EvictionPolicy,
                                     UnlinkPolicy,
                                     ResourceProvider*);
    PrioritizedResource::Backing* createBacking(gfx::Size, GLenum format, ResourceProvider*);
    void evictFirstBackingResource(ResourceProvider*);
    void sortBackings();

    void assertInvariants();

    size_t m_maxMemoryLimitBytes;
    // The priority cutoff based on memory pressure. This is not a strict
    // cutoff -- requestLate allows textures with priority equal to this
    // cutoff to be allowed.
    int m_priorityCutoff;
    // The priority cutoff based on external memory policy. This is a strict
    // cutoff -- no textures with priority equal to this cutoff will be allowed.
    int m_externalPriorityCutoff;
    size_t m_memoryUseBytes;
    size_t m_memoryAboveCutoffBytes;
    size_t m_memoryAvailableBytes;

    typedef base::hash_set<PrioritizedResource*> TextureSet;
    typedef std::vector<PrioritizedResource*> TextureVector;

    const Proxy* m_proxy;

    TextureSet m_textures;
    // This list is always sorted in eviction order, with the exception the
    // newly-allocated or recycled textures at the very end of the tail that 
    // are not sorted by priority.
    BackingList m_backings;
    bool m_backingsTailNotSorted;

    // The list of backings that have been evicted, but may still be linked
    // to textures. This can be accessed concurrently by the main and impl
    // threads, and may only be accessed while holding m_evictedBackingsLock.
    mutable base::Lock m_evictedBackingsLock;
    BackingList m_evictedBackings;

    TextureVector m_tempTextureVector;

    // Statistics about memory usage at priority cutoffs, computed at prioritizeTextures.
    size_t m_memoryVisibleBytes;
    size_t m_memoryVisibleAndNearbyBytes;

    // Statistics copied at the time of pushTexturePrioritiesToBackings.
    size_t m_memoryVisibleLastPushedBytes;
    size_t m_memoryVisibleAndNearbyLastPushedBytes;

    DISALLOW_COPY_AND_ASSIGN(PrioritizedResourceManager);
};

}  // namespace cc

#endif  // CC_RESOURCES_PRIORITIZED_RESOURCE_MANAGER_H_
