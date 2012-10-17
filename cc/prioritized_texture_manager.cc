// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "CCPrioritizedTextureManager.h"

#include "base/stl_util.h"
#include "CCPrioritizedTexture.h"
#include "CCPriorityCalculator.h"
#include "CCProxy.h"
#include "TraceEvent.h"
#include <algorithm>

using namespace std;

namespace cc {

CCPrioritizedTextureManager::CCPrioritizedTextureManager(size_t maxMemoryLimitBytes, int, int pool)
    : m_maxMemoryLimitBytes(maxMemoryLimitBytes)
    , m_memoryUseBytes(0)
    , m_memoryAboveCutoffBytes(0)
    , m_memoryAvailableBytes(0)
    , m_pool(pool)
    , m_backingsTailNotSorted(false)
{
}

CCPrioritizedTextureManager::~CCPrioritizedTextureManager()
{
    while (m_textures.size() > 0)
        unregisterTexture(*m_textures.begin());

    deleteUnlinkedEvictedBackings();
    ASSERT(m_evictedBackings.empty());

    // Each remaining backing is a leaked opengl texture. There should be none.
    ASSERT(m_backings.empty());
}

void CCPrioritizedTextureManager::prioritizeTextures()
{
    TRACE_EVENT0("cc", "CCPrioritizedTextureManager::prioritizeTextures");
    ASSERT(CCProxy::isMainThread());

    // Sorting textures in this function could be replaced by a slightly
    // modified O(n) quick-select to partition textures rather than
    // sort them (if performance of the sort becomes an issue).

    TextureVector& sortedTextures = m_tempTextureVector;
    sortedTextures.clear();

    // Copy all textures into a vector and sort them.
    for (TextureSet::iterator it = m_textures.begin(); it != m_textures.end(); ++it)
        sortedTextures.append(*it);
    std::sort(sortedTextures.begin(), sortedTextures.end(), compareTextures);

    m_memoryAvailableBytes = m_maxMemoryLimitBytes;
    m_priorityCutoff = CCPriorityCalculator::lowestPriority();
    size_t memoryBytes = 0;
    for (TextureVector::iterator it = sortedTextures.begin(); it != sortedTextures.end(); ++it) {
        if ((*it)->requestPriority() == CCPriorityCalculator::lowestPriority())
            break;

        if ((*it)->isSelfManaged()) {
            // Account for self-managed memory immediately by reducing the memory
            // available (since it never gets acquired).
            size_t newMemoryBytes = memoryBytes + (*it)->bytes();
            if (newMemoryBytes > m_memoryAvailableBytes) {
                m_priorityCutoff = (*it)->requestPriority();
                m_memoryAvailableBytes = memoryBytes;
                break;
            }
            m_memoryAvailableBytes -= (*it)->bytes();
        } else {
            size_t newMemoryBytes = memoryBytes + (*it)->bytes();
            if (newMemoryBytes > m_memoryAvailableBytes) {
                m_priorityCutoff = (*it)->requestPriority();
                break;
            }
            memoryBytes = newMemoryBytes;
        }
    }

    // Only allow textures if they are higher than the cutoff. All textures
    // of the same priority are accepted or rejected together, rather than
    // being partially allowed randomly.
    m_memoryAboveCutoffBytes = 0;
    for (TextureVector::iterator it = sortedTextures.begin(); it != sortedTextures.end(); ++it) {
        bool isAbovePriorityCutoff = CCPriorityCalculator::priorityIsHigher((*it)->requestPriority(), m_priorityCutoff);
        (*it)->setAbovePriorityCutoff(isAbovePriorityCutoff);
        if (isAbovePriorityCutoff && !(*it)->isSelfManaged())
            m_memoryAboveCutoffBytes += (*it)->bytes();
    }
    sortedTextures.clear();

    ASSERT(m_memoryAboveCutoffBytes <= m_memoryAvailableBytes);
    ASSERT(memoryAboveCutoffBytes() <= maxMemoryLimitBytes());
}

void CCPrioritizedTextureManager::pushTexturePrioritiesToBackings()
{
    TRACE_EVENT0("cc", "CCPrioritizedTextureManager::pushTexturePrioritiesToBackings");
    ASSERT(CCProxy::isImplThread() && CCProxy::isMainThreadBlocked());

    assertInvariants();
    for (BackingList::iterator it = m_backings.begin(); it != m_backings.end(); ++it)
        (*it)->updatePriority();
    sortBackings();
    assertInvariants();
}

void CCPrioritizedTextureManager::updateBackingsInDrawingImplTree()
{
    TRACE_EVENT0("cc", "CCPrioritizedTextureManager::updateBackingsInDrawingImplTree");
    ASSERT(CCProxy::isImplThread() && CCProxy::isMainThreadBlocked());

    assertInvariants();
    for (BackingList::iterator it = m_backings.begin(); it != m_backings.end(); ++it) {
        CCPrioritizedTexture::Backing* backing = (*it);
        backing->updateInDrawingImplTree();
    }
    sortBackings();
    assertInvariants();
}

void CCPrioritizedTextureManager::sortBackings()
{
    TRACE_EVENT0("cc", "CCPrioritizedTextureManager::sortBackings");
    ASSERT(CCProxy::isImplThread());

    // Put backings in eviction/recycling order.
    m_backings.sort(compareBackings);
    m_backingsTailNotSorted = false;
}

void CCPrioritizedTextureManager::clearPriorities()
{
    ASSERT(CCProxy::isMainThread());
    for (TextureSet::iterator it = m_textures.begin(); it != m_textures.end(); ++it) {
        // FIXME: We should remove this and just set all priorities to
        //        CCPriorityCalculator::lowestPriority() once we have priorities
        //        for all textures (we can't currently calculate distances for
        //        off-screen textures).
        (*it)->setRequestPriority(CCPriorityCalculator::lingeringPriority((*it)->requestPriority()));
    }
}

bool CCPrioritizedTextureManager::requestLate(CCPrioritizedTexture* texture)
{
    ASSERT(CCProxy::isMainThread());

    // This is already above cutoff, so don't double count it's memory below.
    if (texture->isAbovePriorityCutoff())
        return true;

    if (CCPriorityCalculator::priorityIsLower(texture->requestPriority(), m_priorityCutoff))
        return false;

    size_t newMemoryBytes = m_memoryAboveCutoffBytes + texture->bytes();
    if (newMemoryBytes > m_memoryAvailableBytes)
        return false;

    m_memoryAboveCutoffBytes = newMemoryBytes;
    texture->setAbovePriorityCutoff(true);
    return true;
}

void CCPrioritizedTextureManager::acquireBackingTextureIfNeeded(CCPrioritizedTexture* texture, CCResourceProvider* resourceProvider)
{
    ASSERT(CCProxy::isImplThread() && CCProxy::isMainThreadBlocked());
    ASSERT(!texture->isSelfManaged());
    ASSERT(texture->isAbovePriorityCutoff());
    if (texture->backing() || !texture->isAbovePriorityCutoff())
        return;

    // Find a backing below, by either recycling or allocating.
    CCPrioritizedTexture::Backing* backing = 0;

    // First try to recycle
    for (BackingList::iterator it = m_backings.begin(); it != m_backings.end(); ++it) {
        if (!(*it)->canBeRecycled())
            break;
        if ((*it)->size() == texture->size() && (*it)->format() == texture->format()) {
            backing = (*it);
            m_backings.erase(it);
            break;
        }
    }

    // Otherwise reduce memory and just allocate a new backing texures.
    if (!backing) {
        evictBackingsToReduceMemory(m_memoryAvailableBytes - texture->bytes(), RespectManagerPriorityCutoff, resourceProvider);
        backing = createBacking(texture->size(), texture->format(), resourceProvider);
    }

    // Move the used backing to the end of the eviction list, and note that
    // the tail is not sorted.
    if (backing->owner())
        backing->owner()->unlink();
    texture->link(backing);
    m_backings.push_back(backing);
    m_backingsTailNotSorted = true;

    // Update the backing's priority from its new owner.
    backing->updatePriority();
}

bool CCPrioritizedTextureManager::evictBackingsToReduceMemory(size_t limitBytes, EvictionPriorityPolicy evictionPolicy, CCResourceProvider* resourceProvider)
{
    ASSERT(CCProxy::isImplThread());
    if (memoryUseBytes() <= limitBytes)
        return false;

    // Destroy backings until we are below the limit,
    // or until all backings remaining are above the cutoff.
    while (memoryUseBytes() > limitBytes && m_backings.size() > 0) {
        CCPrioritizedTexture::Backing* backing = m_backings.front();
        if (evictionPolicy == RespectManagerPriorityCutoff)
            if (backing->wasAbovePriorityCutoffAtLastPriorityUpdate())
                break;
        evictFirstBackingResource(resourceProvider);
    }
    return true;
}

void CCPrioritizedTextureManager::reduceMemory(CCResourceProvider* resourceProvider)
{
    ASSERT(CCProxy::isImplThread() && CCProxy::isMainThreadBlocked());

    evictBackingsToReduceMemory(m_memoryAvailableBytes, RespectManagerPriorityCutoff, resourceProvider);
    ASSERT(memoryUseBytes() <= maxMemoryLimitBytes());

    // We currently collect backings from deleted textures for later recycling.
    // However, if we do that forever we will always use the max limit even if
    // we really need very little memory. This should probably be solved by reducing the
    // limit externally, but until then this just does some "clean up" of unused
    // backing textures (any more than 10%).
    size_t wastedMemory = 0;
    for (BackingList::iterator it = m_backings.begin(); it != m_backings.end(); ++it) {
        if ((*it)->owner())
            break;
        wastedMemory += (*it)->bytes();
    }
    size_t tenPercentOfMemory = m_memoryAvailableBytes / 10;
    if (wastedMemory > tenPercentOfMemory)
        evictBackingsToReduceMemory(memoryUseBytes() - (wastedMemory - tenPercentOfMemory), RespectManagerPriorityCutoff, resourceProvider);

    // Unlink all evicted backings
    for (BackingList::const_iterator it = m_evictedBackings.begin(); it != m_evictedBackings.end(); ++it) {
        if ((*it)->owner())
            (*it)->owner()->unlink();
    }

    // And clear the list of evicted backings
    deleteUnlinkedEvictedBackings();
}

void CCPrioritizedTextureManager::clearAllMemory(CCResourceProvider* resourceProvider)
{
    ASSERT(CCProxy::isImplThread() && CCProxy::isMainThreadBlocked());
    ASSERT(resourceProvider);
    evictBackingsToReduceMemory(0, DoNotRespectManagerPriorityCutoff, resourceProvider);
}

bool CCPrioritizedTextureManager::reduceMemoryOnImplThread(size_t limitBytes, CCResourceProvider* resourceProvider)
{
    ASSERT(CCProxy::isImplThread());
    ASSERT(resourceProvider);
    // If we are in the process of uploading a new frame then the backings at the very end of
    // the list are not sorted by priority. Sort them before doing the eviction.
    if (m_backingsTailNotSorted)
        sortBackings();
    return evictBackingsToReduceMemory(limitBytes, DoNotRespectManagerPriorityCutoff, resourceProvider);
}

void CCPrioritizedTextureManager::getEvictedBackings(BackingList& evictedBackings)
{
    ASSERT(CCProxy::isImplThread());
    evictedBackings.clear();
    evictedBackings.insert(evictedBackings.begin(), m_evictedBackings.begin(), m_evictedBackings.end());
}

void CCPrioritizedTextureManager::unlinkEvictedBackings(const BackingList& evictedBackings)
{
    ASSERT(CCProxy::isMainThread());
    for (BackingList::const_iterator it = evictedBackings.begin(); it != evictedBackings.end(); ++it) {
        CCPrioritizedTexture::Backing* backing = (*it);
        if (backing->owner())
            backing->owner()->unlink();
    }
}

void CCPrioritizedTextureManager::deleteUnlinkedEvictedBackings()
{
    ASSERT(CCProxy::isMainThread() || (CCProxy::isImplThread() && CCProxy::isMainThreadBlocked()));
    BackingList newEvictedBackings;
    for (BackingList::const_iterator it = m_evictedBackings.begin(); it != m_evictedBackings.end(); ++it) {
        CCPrioritizedTexture::Backing* backing = (*it);
        if (backing->owner())
            newEvictedBackings.push_back(backing);
        else
            delete backing;
    }
    m_evictedBackings.swap(newEvictedBackings);
}

bool CCPrioritizedTextureManager::linkedEvictedBackingsExist() const
{
    for (BackingList::const_iterator it = m_evictedBackings.begin(); it != m_evictedBackings.end(); ++it) {
        if ((*it)->owner())
            return true;
    }
    return false;
}

void CCPrioritizedTextureManager::registerTexture(CCPrioritizedTexture* texture)
{
    ASSERT(CCProxy::isMainThread());
    ASSERT(texture);
    ASSERT(!texture->textureManager());
    ASSERT(!texture->backing());
    ASSERT(!ContainsKey(m_textures, texture));

    texture->setManagerInternal(this);
    m_textures.insert(texture);

}

void CCPrioritizedTextureManager::unregisterTexture(CCPrioritizedTexture* texture)
{
    ASSERT(CCProxy::isMainThread() || (CCProxy::isImplThread() && CCProxy::isMainThreadBlocked()));
    ASSERT(texture);
    ASSERT(ContainsKey(m_textures, texture));

    returnBackingTexture(texture);
    texture->setManagerInternal(0);
    m_textures.erase(texture);
    texture->setAbovePriorityCutoff(false);
}

void CCPrioritizedTextureManager::returnBackingTexture(CCPrioritizedTexture* texture)
{
    ASSERT(CCProxy::isMainThread() || (CCProxy::isImplThread() && CCProxy::isMainThreadBlocked()));
    if (texture->backing())
        texture->unlink();
}

CCPrioritizedTexture::Backing* CCPrioritizedTextureManager::createBacking(IntSize size, GC3Denum format, CCResourceProvider* resourceProvider)
{
    ASSERT(CCProxy::isImplThread() && CCProxy::isMainThreadBlocked());
    ASSERT(resourceProvider);
    CCResourceProvider::ResourceId resourceId = resourceProvider->createResource(m_pool, size, format, CCResourceProvider::TextureUsageAny);
    CCPrioritizedTexture::Backing* backing = new CCPrioritizedTexture::Backing(resourceId, resourceProvider, size, format);
    m_memoryUseBytes += backing->bytes();
    return backing;
}

void CCPrioritizedTextureManager::evictFirstBackingResource(CCResourceProvider* resourceProvider)
{
    ASSERT(CCProxy::isImplThread());
    ASSERT(resourceProvider);
    ASSERT(!m_backings.empty());
    CCPrioritizedTexture::Backing* backing = m_backings.front();

    // Note that we create a backing and its resource at the same time, but we
    // delete the backing structure and its resource in two steps. This is because
    // we can delete the resource while the main thread is running, but we cannot
    // unlink backings while the main thread is running.
    backing->deleteResource(resourceProvider);
    m_memoryUseBytes -= backing->bytes();
    m_backings.pop_front();
    m_evictedBackings.push_back(backing);
}

void CCPrioritizedTextureManager::assertInvariants()
{
#if !ASSERT_DISABLED
    ASSERT(CCProxy::isImplThread() && CCProxy::isMainThreadBlocked());

    // If we hit any of these asserts, there is a bug in this class. To see
    // where the bug is, call this function at the beginning and end of
    // every public function.

    // Backings/textures must be doubly-linked and only to other backings/textures in this manager.
    for (BackingList::iterator it = m_backings.begin(); it != m_backings.end(); ++it) {
        if ((*it)->owner()) {
            ASSERT(ContainsKey(m_textures, (*it)->owner()));
            ASSERT((*it)->owner()->backing() == (*it));
        }
    }
    for (TextureSet::iterator it = m_textures.begin(); it != m_textures.end(); ++it) {
        CCPrioritizedTexture* texture = (*it);
        CCPrioritizedTexture::Backing* backing = texture->backing();
        if (backing) {
            if (backing->resourceHasBeenDeleted()) {
                ASSERT(std::find(m_backings.begin(), m_backings.end(), backing) == m_backings.end());
                ASSERT(std::find(m_evictedBackings.begin(), m_evictedBackings.end(), backing) != m_evictedBackings.end());
            } else {
                ASSERT(std::find(m_backings.begin(), m_backings.end(), backing) != m_backings.end());
                ASSERT(std::find(m_evictedBackings.begin(), m_evictedBackings.end(), backing) == m_evictedBackings.end());
            }
            ASSERT(backing->owner() == texture);
        }
    }

    // At all times, backings that can be evicted must always come before
    // backings that can't be evicted in the backing texture list (otherwise
    // reduceMemory will not find all textures available for eviction/recycling).
    bool reachedUnrecyclable = false;
    CCPrioritizedTexture::Backing* previous_backing = NULL;
    for (BackingList::iterator it = m_backings.begin(); it != m_backings.end(); ++it) {
        CCPrioritizedTexture::Backing* backing = *it;
        if (previous_backing && (!m_backingsTailNotSorted || !backing->wasAbovePriorityCutoffAtLastPriorityUpdate()))
            ASSERT(compareBackings(previous_backing, backing));
        if (!backing->canBeRecycled())
            reachedUnrecyclable = true;
        if (reachedUnrecyclable)
            ASSERT(!backing->canBeRecycled());
        else
            ASSERT(backing->canBeRecycled());
        previous_backing = backing;
    }
#endif
}

} // namespace cc
