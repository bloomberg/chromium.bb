// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CCPrioritizedTextureManager_h
#define CCPrioritizedTextureManager_h

#include <list>
#include <vector>

#include "IntRect.h"
#include "IntSize.h"
#include "base/basictypes.h"
#include "base/hash_tables.h"
#include "base/memory/scoped_ptr.h"
#include "cc/prioritized_texture.h"
#include "cc/priority_calculator.h"
#include "cc/texture.h"
#include "third_party/khronos/GLES2/gl2.h"

#if defined(COMPILER_GCC)
namespace BASE_HASH_NAMESPACE {
template<>
struct hash<cc::PrioritizedTexture*> {
  size_t operator()(cc::PrioritizedTexture* ptr) const {
    return hash<size_t>()(reinterpret_cast<size_t>(ptr));
  }
};
} // namespace BASE_HASH_NAMESPACE
#endif // COMPILER

namespace cc {

class PriorityCalculator;

class PrioritizedTextureManager {
public:
    static scoped_ptr<PrioritizedTextureManager> create(size_t maxMemoryLimitBytes, int maxTextureSize, int pool)
    {
        return make_scoped_ptr(new PrioritizedTextureManager(maxMemoryLimitBytes, maxTextureSize, pool));
    }
    scoped_ptr<PrioritizedTexture> createTexture(IntSize size, GLenum format)
    {
        return make_scoped_ptr(new PrioritizedTexture(this, size, format));
    }
    ~PrioritizedTextureManager();

    typedef std::list<PrioritizedTexture::Backing*> BackingList;

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

    void prioritizeTextures();
    void clearPriorities();

    // Delete contents textures' backing resources until they use only bytesLimit bytes. This may
    // be called on the impl thread while the main thread is running. Returns true if resources are
    // indeed evicted as a result of this call.
    bool reduceMemoryOnImplThread(size_t limitBytes, int priorityCutoff, ResourceProvider*);
    // Returns true if there exist any textures that are linked to backings that have had their
    // resources evicted. Only when we commit a tree that has no textures linked to evicted backings
    // may we allow drawing.
    bool linkedEvictedBackingsExist() const;
    // Retrieve the list of all contents textures' backings that have been evicted, to pass to the
    // main thread to unlink them from their owning textures.
    void getEvictedBackings(BackingList& evictedBackings);
    // Unlink the list of contents textures' backings from their owning textures on the main thread
    // before updating layers.
    void unlinkEvictedBackings(const BackingList& evictedBackings);

    bool requestLate(PrioritizedTexture*);

    void reduceMemory(ResourceProvider*);
    void clearAllMemory(ResourceProvider*);

    void acquireBackingTextureIfNeeded(PrioritizedTexture*, ResourceProvider*);

    void registerTexture(PrioritizedTexture*);
    void unregisterTexture(PrioritizedTexture*);
    void returnBackingTexture(PrioritizedTexture*);

    // Update all backings' priorities from their owning texture.
    void pushTexturePrioritiesToBackings();

    // Mark all textures' backings as being in the drawing impl tree.
    void updateBackingsInDrawingImplTree();

private:
    friend class PrioritizedTextureTest;

    enum EvictionPolicy {
        EvictOnlyRecyclable,
        EvictAnything,
    };

    // Compare textures. Highest priority first.
    static inline bool compareTextures(PrioritizedTexture* a, PrioritizedTexture* b)
    {
        if (a->requestPriority() == b->requestPriority())
            return a < b;
        return PriorityCalculator::priorityIsHigher(a->requestPriority(), b->requestPriority());
    }
    // Compare backings. Lowest priority first.
    static inline bool compareBackings(PrioritizedTexture::Backing* a, PrioritizedTexture::Backing* b)
    {
        // Make textures that can be recycled appear first
        if (a->canBeRecycled() != b->canBeRecycled())
            return (a->canBeRecycled() > b->canBeRecycled());
        // Then sort by being above or below the priority cutoff.
        if (a->wasAbovePriorityCutoffAtLastPriorityUpdate() != b->wasAbovePriorityCutoffAtLastPriorityUpdate())
            return (a->wasAbovePriorityCutoffAtLastPriorityUpdate() < b->wasAbovePriorityCutoffAtLastPriorityUpdate());
        // Then sort by priority (note that backings that no longer have owners will
        // always have the lowest priority)
        if (a->requestPriorityAtLastPriorityUpdate() != b->requestPriorityAtLastPriorityUpdate())
            return PriorityCalculator::priorityIsLower(a->requestPriorityAtLastPriorityUpdate(), b->requestPriorityAtLastPriorityUpdate());
        // Finally sort by being in the impl tree versus being completely unreferenced
        if (a->inDrawingImplTree() != b->inDrawingImplTree())
            return (a->inDrawingImplTree() < b->inDrawingImplTree());
        return a < b;
    }

    PrioritizedTextureManager(size_t maxMemoryLimitBytes, int maxTextureSize, int pool);

    bool evictBackingsToReduceMemory(size_t limitBytes, int priorityCutoff, EvictionPolicy, ResourceProvider*);
    PrioritizedTexture::Backing* createBacking(IntSize, GLenum format, ResourceProvider*);
    void evictFirstBackingResource(ResourceProvider*);
    void deleteUnlinkedEvictedBackings();
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
    int m_pool;

    typedef base::hash_set<PrioritizedTexture*> TextureSet;
    typedef std::vector<PrioritizedTexture*> TextureVector;

    TextureSet m_textures;
    // This list is always sorted in eviction order, with the exception the
    // newly-allocated or recycled textures at the very end of the tail that 
    // are not sorted by priority.
    BackingList m_backings;
    bool m_backingsTailNotSorted;
    BackingList m_evictedBackings;

    TextureVector m_tempTextureVector;

    DISALLOW_COPY_AND_ASSIGN(PrioritizedTextureManager);
};

}  // namespace cc

#endif
