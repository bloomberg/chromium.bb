// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CCPrioritizedTextureManager_h
#define CCPrioritizedTextureManager_h

#include "CCPrioritizedTexture.h"
#include "CCPriorityCalculator.h"
#include "CCTexture.h"
#include "GraphicsContext3D.h"
#include "IntRect.h"
#include "IntSize.h"
#include <wtf/HashSet.h>
#include <wtf/ListHashSet.h>
#include <wtf/Vector.h>

namespace cc {

class CCPrioritizedTexture;
class CCPriorityCalculator;

class CCPrioritizedTextureManager {
    WTF_MAKE_NONCOPYABLE(CCPrioritizedTextureManager);
public:
    static PassOwnPtr<CCPrioritizedTextureManager> create(size_t maxMemoryLimitBytes, int maxTextureSize, int pool)
    {
        return adoptPtr(new CCPrioritizedTextureManager(maxMemoryLimitBytes, maxTextureSize, pool));
    }
    PassOwnPtr<CCPrioritizedTexture> createTexture(IntSize size, GC3Denum format)
    {
        return adoptPtr(new CCPrioritizedTexture(this, size, format));
    }
    ~CCPrioritizedTextureManager();

    typedef Vector<CCPrioritizedTexture::Backing*> BackingVector;

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

    void prioritizeTextures();
    void clearPriorities();

    void reduceMemoryOnImplThread(size_t limitBytes, CCResourceProvider*);
    bool evictedBackingsExist() const { return !m_evictedBackings.isEmpty(); }
    void getEvictedBackings(BackingVector& evictedBackings);
    void unlinkEvictedBackings(const BackingVector& evictedBackings);
    // Deletes all evicted backings, unlinking them from their owning textures if needed.
    // Returns true if this function unlinked any backings from their owning texture while
    // destroying them.
    bool deleteEvictedBackings();

    bool requestLate(CCPrioritizedTexture*);

    void reduceMemory(CCResourceProvider*);
    void clearAllMemory(CCResourceProvider*);

    void acquireBackingTextureIfNeeded(CCPrioritizedTexture*, CCResourceProvider*);

    void registerTexture(CCPrioritizedTexture*);
    void unregisterTexture(CCPrioritizedTexture*);
    void returnBackingTexture(CCPrioritizedTexture*);

private:
    friend class CCPrioritizedTextureTest;

    enum EvictionPriorityPolicy {
        RespectManagerPriorityCutoff,
        DoNotRespectManagerPriorityCutoff,
    };

    // Compare textures. Highest priority first.
    static inline bool compareTextures(CCPrioritizedTexture* a, CCPrioritizedTexture* b)
    {
        if (a->requestPriority() == b->requestPriority())
            return a < b;
        return CCPriorityCalculator::priorityIsHigher(a->requestPriority(), b->requestPriority());
    }
    // Compare backings. Lowest priority first.
    static inline bool compareBackings(CCPrioritizedTexture::Backing* a, CCPrioritizedTexture::Backing* b)
    {
        int priorityA = a->requestPriorityAtLastPriorityUpdate();
        int priorityB = b->requestPriorityAtLastPriorityUpdate();
        if (priorityA != priorityB)
            return CCPriorityCalculator::priorityIsLower(priorityA, priorityB);
        bool aboveCutoffA = a->wasAbovePriorityCutoffAtLastPriorityUpdate();
        bool aboveCutoffB = b->wasAbovePriorityCutoffAtLastPriorityUpdate();
        if (!aboveCutoffA && aboveCutoffB)
            return true;
        if (aboveCutoffA && !aboveCutoffB)
            return false;
        return a < b;
    }

    CCPrioritizedTextureManager(size_t maxMemoryLimitBytes, int maxTextureSize, int pool);

    void updateBackingsPriorities();
    void evictBackingsToReduceMemory(size_t limitBytes, EvictionPriorityPolicy, CCResourceProvider*);
    CCPrioritizedTexture::Backing* createBacking(IntSize, GC3Denum format, CCResourceProvider*);
    void evictBackingResource(CCPrioritizedTexture::Backing*, CCResourceProvider*);

#if !ASSERT_DISABLED
    void assertInvariants();
#endif

    size_t m_maxMemoryLimitBytes;
    unsigned m_priorityCutoff;
    size_t m_memoryUseBytes;
    size_t m_memoryAboveCutoffBytes;
    size_t m_memoryAvailableBytes;
    int m_pool;

    typedef HashSet<CCPrioritizedTexture*> TextureSet;
    typedef ListHashSet<CCPrioritizedTexture::Backing*> BackingSet;
    typedef Vector<CCPrioritizedTexture*> TextureVector;

    TextureSet m_textures;
    BackingSet m_backings;
    BackingVector m_evictedBackings;

    TextureVector m_tempTextureVector;
    BackingVector m_tempBackingVector;

    // Set by the main thread when it adjust priorities in such a way that
    // the m_backings array's view of priorities is now out of date.
    bool m_needsUpdateBackingsPrioritites;
};

} // cc

#endif
