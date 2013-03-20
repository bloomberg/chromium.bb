// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/resources/prioritized_resource_manager.h"

#include <algorithm>

#include "base/debug/trace_event.h"
#include "base/stl_util.h"
#include "cc/resources/prioritized_resource.h"
#include "cc/resources/priority_calculator.h"
#include "cc/trees/proxy.h"

namespace cc {

PrioritizedResourceManager::PrioritizedResourceManager(const Proxy* proxy)
    : m_proxy(proxy)
    , m_maxMemoryLimitBytes(defaultMemoryAllocationLimit())
    , m_externalPriorityCutoff(PriorityCalculator::AllowEverythingCutoff())
    , m_memoryUseBytes(0)
    , m_memoryAboveCutoffBytes(0)
    , m_memoryAvailableBytes(0)
    , m_backingsTailNotSorted(false)
    , m_memoryVisibleBytes(0)
    , m_memoryVisibleAndNearbyBytes(0)
    , m_memoryVisibleLastPushedBytes(0)
    , m_memoryVisibleAndNearbyLastPushedBytes(0)
{
}

PrioritizedResourceManager::~PrioritizedResourceManager()
{
    while (m_textures.size() > 0)
        unregisterTexture(*m_textures.begin());

    unlinkAndClearEvictedBackings();
    DCHECK(m_evictedBackings.empty());

    // Each remaining backing is a leaked opengl texture. There should be none.
    DCHECK(m_backings.empty());
}

size_t PrioritizedResourceManager::memoryVisibleBytes() const
{
    DCHECK(m_proxy->IsImplThread());
    return m_memoryVisibleLastPushedBytes;
}

size_t PrioritizedResourceManager::memoryVisibleAndNearbyBytes() const
{
    DCHECK(m_proxy->IsImplThread());
    return m_memoryVisibleAndNearbyLastPushedBytes;
}

void PrioritizedResourceManager::prioritizeTextures()
{
    TRACE_EVENT0("cc", "PrioritizedResourceManager::prioritizeTextures");
    DCHECK(m_proxy->IsMainThread());

    // Sorting textures in this function could be replaced by a slightly
    // modified O(n) quick-select to partition textures rather than
    // sort them (if performance of the sort becomes an issue).

    TextureVector& sortedTextures = m_tempTextureVector;
    sortedTextures.clear();

    // Copy all textures into a vector, sort them, and collect memory requirements statistics.
    m_memoryVisibleBytes = 0;
    m_memoryVisibleAndNearbyBytes = 0;
    for (TextureSet::iterator it = m_textures.begin(); it != m_textures.end(); ++it) {
        PrioritizedResource* texture = (*it);
        sortedTextures.push_back(texture);
        if (PriorityCalculator::priority_is_higher(texture->request_priority(), PriorityCalculator::AllowVisibleOnlyCutoff()))
            m_memoryVisibleBytes += texture->bytes();
        if (PriorityCalculator::priority_is_higher(texture->request_priority(), PriorityCalculator::AllowVisibleAndNearbyCutoff()))
            m_memoryVisibleAndNearbyBytes += texture->bytes();
    }
    std::sort(sortedTextures.begin(), sortedTextures.end(), compareTextures);

    // Compute a priority cutoff based on memory pressure
    m_memoryAvailableBytes = m_maxMemoryLimitBytes;
    m_priorityCutoff = m_externalPriorityCutoff;
    size_t memoryBytes = 0;
    for (TextureVector::iterator it = sortedTextures.begin(); it != sortedTextures.end(); ++it) {
        if ((*it)->is_self_managed()) {
            // Account for self-managed memory immediately by reducing the memory
            // available (since it never gets acquired).
            size_t newMemoryBytes = memoryBytes + (*it)->bytes();
            if (newMemoryBytes > m_memoryAvailableBytes) {
                m_priorityCutoff = (*it)->request_priority();
                m_memoryAvailableBytes = memoryBytes;
                break;
            }
            m_memoryAvailableBytes -= (*it)->bytes();
        } else {
            size_t newMemoryBytes = memoryBytes + (*it)->bytes();
            if (newMemoryBytes > m_memoryAvailableBytes) {
                m_priorityCutoff = (*it)->request_priority();
                break;
            }
            memoryBytes = newMemoryBytes;
        }
    }

    // Disallow any textures with priority below the external cutoff to have backings.
    for (TextureVector::iterator it = sortedTextures.begin(); it != sortedTextures.end(); ++it) {
        PrioritizedResource* texture = (*it);
        if (!PriorityCalculator::priority_is_higher(texture->request_priority(), m_externalPriorityCutoff) &&
            texture->have_backing_texture())
            texture->Unlink();
    }

    // Only allow textures if they are higher than the cutoff. All textures
    // of the same priority are accepted or rejected together, rather than
    // being partially allowed randomly.
    m_memoryAboveCutoffBytes = 0;
    for (TextureVector::iterator it = sortedTextures.begin(); it != sortedTextures.end(); ++it) {
        bool is_above_priority_cutoff = PriorityCalculator::priority_is_higher((*it)->request_priority(), m_priorityCutoff);
        (*it)->set_above_priority_cutoff(is_above_priority_cutoff);
        if (is_above_priority_cutoff && !(*it)->is_self_managed())
            m_memoryAboveCutoffBytes += (*it)->bytes();
    }
    sortedTextures.clear();

    DCHECK(m_memoryAboveCutoffBytes <= m_memoryAvailableBytes);
    DCHECK(memoryAboveCutoffBytes() <= maxMemoryLimitBytes());
}

void PrioritizedResourceManager::pushTexturePrioritiesToBackings()
{
    TRACE_EVENT0("cc", "PrioritizedResourceManager::pushTexturePrioritiesToBackings");
    DCHECK(m_proxy->IsImplThread() && m_proxy->IsMainThreadBlocked());

    assertInvariants();
    for (BackingList::iterator it = m_backings.begin(); it != m_backings.end(); ++it)
        (*it)->UpdatePriority();
    sortBackings();
    assertInvariants();

    // Push memory requirements to the impl thread structure.
    m_memoryVisibleLastPushedBytes = m_memoryVisibleBytes;
    m_memoryVisibleAndNearbyLastPushedBytes = m_memoryVisibleAndNearbyBytes;
}

void PrioritizedResourceManager::updateBackingsInDrawingImplTree()
{
    TRACE_EVENT0("cc", "PrioritizedResourceManager::updateBackingsInDrawingImplTree");
    DCHECK(m_proxy->IsImplThread() && m_proxy->IsMainThreadBlocked());

    assertInvariants();
    for (BackingList::iterator it = m_backings.begin(); it != m_backings.end(); ++it) {
        PrioritizedResource::Backing* backing = (*it);
        backing->UpdateInDrawingImplTree();
    }
    sortBackings();
    assertInvariants();
}

void PrioritizedResourceManager::sortBackings()
{
    TRACE_EVENT0("cc", "PrioritizedResourceManager::sortBackings");
    DCHECK(m_proxy->IsImplThread());

    // Put backings in eviction/recycling order.
    m_backings.sort(compareBackings);
    m_backingsTailNotSorted = false;
}

void PrioritizedResourceManager::clearPriorities()
{
    DCHECK(m_proxy->IsMainThread());
    for (TextureSet::iterator it = m_textures.begin(); it != m_textures.end(); ++it) {
        // FIXME: We should remove this and just set all priorities to
        //        PriorityCalculator::lowestPriority() once we have priorities
        //        for all textures (we can't currently calculate distances for
        //        off-screen textures).
        (*it)->set_request_priority(PriorityCalculator::LingeringPriority((*it)->request_priority()));
    }
}

bool PrioritizedResourceManager::requestLate(PrioritizedResource* texture)
{
    DCHECK(m_proxy->IsMainThread());

    // This is already above cutoff, so don't double count it's memory below.
    if (texture->is_above_priority_cutoff())
        return true;

    // Allow textures that have priority equal to the cutoff, but not strictly lower.
    if (PriorityCalculator::priority_is_lower(texture->request_priority(), m_priorityCutoff))
        return false;

    // Disallow textures that do not have a priority strictly higher than the external cutoff.
    if (!PriorityCalculator::priority_is_higher(texture->request_priority(), m_externalPriorityCutoff))
        return false;

    size_t newMemoryBytes = m_memoryAboveCutoffBytes + texture->bytes();
    if (newMemoryBytes > m_memoryAvailableBytes)
        return false;

    m_memoryAboveCutoffBytes = newMemoryBytes;
    texture->set_above_priority_cutoff(true);
    return true;
}

void PrioritizedResourceManager::acquireBackingTextureIfNeeded(PrioritizedResource* texture, ResourceProvider* resourceProvider)
{
    DCHECK(m_proxy->IsImplThread() && m_proxy->IsMainThreadBlocked());
    DCHECK(!texture->is_self_managed());
    DCHECK(texture->is_above_priority_cutoff());
    if (texture->backing() || !texture->is_above_priority_cutoff())
        return;

    // Find a backing below, by either recycling or allocating.
    PrioritizedResource::Backing* backing = 0;

    // First try to recycle
    for (BackingList::iterator it = m_backings.begin(); it != m_backings.end(); ++it) {
        if (!(*it)->CanBeRecycled())
            break;
        if (resourceProvider->InUseByConsumer((*it)->id()))
            continue;
        if ((*it)->size() == texture->size() && (*it)->format() == texture->format()) {
            backing = (*it);
            m_backings.erase(it);
            break;
        }
    }

    // Otherwise reduce memory and just allocate a new backing texures.
    if (!backing) {
        evictBackingsToReduceMemory(m_memoryAvailableBytes - texture->bytes(),
                                    PriorityCalculator::AllowEverythingCutoff(),
                                    EvictOnlyRecyclable,
                                    DoNotUnlinkBackings,
                                    resourceProvider);
        backing = createBacking(texture->size(), texture->format(), resourceProvider);
    }

    // Move the used backing to the end of the eviction list, and note that
    // the tail is not sorted.
    if (backing->owner())
        backing->owner()->Unlink();
    texture->Link(backing);
    m_backings.push_back(backing);
    m_backingsTailNotSorted = true;

    // Update the backing's priority from its new owner.
    backing->UpdatePriority();
}

bool PrioritizedResourceManager::evictBackingsToReduceMemory(size_t limitBytes,
                                                             int priorityCutoff,
                                                             EvictionPolicy evictionPolicy,
                                                             UnlinkPolicy unlinkPolicy,
                                                             ResourceProvider* resourceProvider)
{
    DCHECK(m_proxy->IsImplThread());
    if (unlinkPolicy == UnlinkBackings)
        DCHECK(m_proxy->IsMainThreadBlocked());
    if (memoryUseBytes() <= limitBytes && PriorityCalculator::AllowEverythingCutoff() == priorityCutoff)
        return false;

    // Destroy backings until we are below the limit,
    // or until all backings remaining are above the cutoff.
    while (m_backings.size() > 0) {
        PrioritizedResource::Backing* backing = m_backings.front();
        if (memoryUseBytes() <= limitBytes &&
            PriorityCalculator::priority_is_higher(backing->request_priority_at_last_priority_update(), priorityCutoff))
            break;
        if (evictionPolicy == EvictOnlyRecyclable && !backing->CanBeRecycled())
            break;
        if (unlinkPolicy == UnlinkBackings && backing->owner())
            backing->owner()->Unlink();
        evictFirstBackingResource(resourceProvider);
    }
    return true;
}

void PrioritizedResourceManager::reduceWastedMemory(ResourceProvider* resourceProvider)
{
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
        evictBackingsToReduceMemory(memoryUseBytes() - (wastedMemory - tenPercentOfMemory),
                                    PriorityCalculator::AllowEverythingCutoff(),
                                    EvictOnlyRecyclable,
                                    DoNotUnlinkBackings,
                                    resourceProvider);
}

void PrioritizedResourceManager::reduceMemory(ResourceProvider* resourceProvider)
{
    DCHECK(m_proxy->IsImplThread() && m_proxy->IsMainThreadBlocked());
    evictBackingsToReduceMemory(m_memoryAvailableBytes,
                                PriorityCalculator::AllowEverythingCutoff(),
                                EvictAnything,
                                UnlinkBackings,
                                resourceProvider);
    DCHECK(memoryUseBytes() <= m_memoryAvailableBytes);

    reduceWastedMemory(resourceProvider);
}

void PrioritizedResourceManager::clearAllMemory(ResourceProvider* resourceProvider)
{
    DCHECK(m_proxy->IsImplThread() && m_proxy->IsMainThreadBlocked());
    if (!resourceProvider) {
        DCHECK(m_backings.empty());
        return;
    }
    evictBackingsToReduceMemory(0,
                                PriorityCalculator::AllowEverythingCutoff(),
                                EvictAnything,
                                DoNotUnlinkBackings,
                                resourceProvider);
}

bool PrioritizedResourceManager::reduceMemoryOnImplThread(size_t limitBytes, int priorityCutoff, ResourceProvider* resourceProvider)
{
    DCHECK(m_proxy->IsImplThread());
    DCHECK(resourceProvider);
    // If we are in the process of uploading a new frame then the backings at the very end of
    // the list are not sorted by priority. Sort them before doing the eviction.
    if (m_backingsTailNotSorted)
        sortBackings();
    return evictBackingsToReduceMemory(limitBytes,
                                       priorityCutoff,
                                       EvictAnything,
                                       DoNotUnlinkBackings,
                                       resourceProvider);
}

void PrioritizedResourceManager::reduceWastedMemoryOnImplThread(ResourceProvider* resourceProvider)
{
    DCHECK(m_proxy->IsImplThread());
    DCHECK(resourceProvider);
    // If we are in the process of uploading a new frame then the backings at the very end of
    // the list are not sorted by priority. Sort them before doing the eviction.
    if (m_backingsTailNotSorted)
        sortBackings();
    reduceWastedMemory(resourceProvider);
}

void PrioritizedResourceManager::unlinkAndClearEvictedBackings()
{
    DCHECK(m_proxy->IsMainThread());
    base::AutoLock scoped_lock(m_evictedBackingsLock);
    for (BackingList::const_iterator it = m_evictedBackings.begin(); it != m_evictedBackings.end(); ++it) {
        PrioritizedResource::Backing* backing = (*it);
        if (backing->owner())
            backing->owner()->Unlink();
        delete backing;
    }
    m_evictedBackings.clear();
}

bool PrioritizedResourceManager::linkedEvictedBackingsExist() const
{
    DCHECK(m_proxy->IsImplThread() && m_proxy->IsMainThreadBlocked());
    base::AutoLock scoped_lock(m_evictedBackingsLock);
    for (BackingList::const_iterator it = m_evictedBackings.begin(); it != m_evictedBackings.end(); ++it) {
        if ((*it)->owner())
            return true;
    }
    return false;
}

void PrioritizedResourceManager::registerTexture(PrioritizedResource* texture)
{
    DCHECK(m_proxy->IsMainThread());
    DCHECK(texture);
    DCHECK(!texture->resource_manager());
    DCHECK(!texture->backing());
    DCHECK(!ContainsKey(m_textures, texture));

    texture->set_manager_internal(this);
    m_textures.insert(texture);

}

void PrioritizedResourceManager::unregisterTexture(PrioritizedResource* texture)
{
    DCHECK(m_proxy->IsMainThread() || (m_proxy->IsImplThread() && m_proxy->IsMainThreadBlocked()));
    DCHECK(texture);
    DCHECK(ContainsKey(m_textures, texture));

    returnBackingTexture(texture);
    texture->set_manager_internal(0);
    m_textures.erase(texture);
    texture->set_above_priority_cutoff(false);
}

void PrioritizedResourceManager::returnBackingTexture(PrioritizedResource* texture)
{
    DCHECK(m_proxy->IsMainThread() || (m_proxy->IsImplThread() && m_proxy->IsMainThreadBlocked()));
    if (texture->backing())
        texture->Unlink();
}

PrioritizedResource::Backing* PrioritizedResourceManager::createBacking(gfx::Size size, GLenum format, ResourceProvider* resourceProvider)
{
    DCHECK(m_proxy->IsImplThread() && m_proxy->IsMainThreadBlocked());
    DCHECK(resourceProvider);
    ResourceProvider::ResourceId resourceId = resourceProvider->CreateManagedResource(size, format, ResourceProvider::TextureUsageAny);
    PrioritizedResource::Backing* backing = new PrioritizedResource::Backing(resourceId, resourceProvider, size, format);
    m_memoryUseBytes += backing->bytes();
    return backing;
}

void PrioritizedResourceManager::evictFirstBackingResource(ResourceProvider* resourceProvider)
{
    DCHECK(m_proxy->IsImplThread());
    DCHECK(resourceProvider);
    DCHECK(!m_backings.empty());
    PrioritizedResource::Backing* backing = m_backings.front();

    // Note that we create a backing and its resource at the same time, but we
    // delete the backing structure and its resource in two steps. This is because
    // we can delete the resource while the main thread is running, but we cannot
    // unlink backings while the main thread is running.
    backing->DeleteResource(resourceProvider);
    m_memoryUseBytes -= backing->bytes();
    m_backings.pop_front();
    base::AutoLock scoped_lock(m_evictedBackingsLock);
    m_evictedBackings.push_back(backing);
}

void PrioritizedResourceManager::assertInvariants()
{
#ifndef NDEBUG
    DCHECK(m_proxy->IsImplThread() && m_proxy->IsMainThreadBlocked());

    // If we hit any of these asserts, there is a bug in this class. To see
    // where the bug is, call this function at the beginning and end of
    // every public function.

    // Backings/textures must be doubly-linked and only to other backings/textures in this manager.
    for (BackingList::iterator it = m_backings.begin(); it != m_backings.end(); ++it) {
        if ((*it)->owner()) {
            DCHECK(ContainsKey(m_textures, (*it)->owner()));
            DCHECK((*it)->owner()->backing() == (*it));
        }
    }
    for (TextureSet::iterator it = m_textures.begin(); it != m_textures.end(); ++it) {
        PrioritizedResource* texture = (*it);
        PrioritizedResource::Backing* backing = texture->backing();
        base::AutoLock scoped_lock(m_evictedBackingsLock);
        if (backing) {
            if (backing->ResourceHasBeenDeleted()) {
                DCHECK(std::find(m_backings.begin(), m_backings.end(), backing) == m_backings.end());
                DCHECK(std::find(m_evictedBackings.begin(), m_evictedBackings.end(), backing) != m_evictedBackings.end());
            } else {
                DCHECK(std::find(m_backings.begin(), m_backings.end(), backing) != m_backings.end());
                DCHECK(std::find(m_evictedBackings.begin(), m_evictedBackings.end(), backing) == m_evictedBackings.end());
            }
            DCHECK(backing->owner() == texture);
        }
    }

    // At all times, backings that can be evicted must always come before
    // backings that can't be evicted in the backing texture list (otherwise
    // reduceMemory will not find all textures available for eviction/recycling).
    bool reachedUnrecyclable = false;
    PrioritizedResource::Backing* previous_backing = NULL;
    for (BackingList::iterator it = m_backings.begin(); it != m_backings.end(); ++it) {
        PrioritizedResource::Backing* backing = *it;
        if (previous_backing && (!m_backingsTailNotSorted || !backing->was_above_priority_cutoff_at_last_priority_update()))
            DCHECK(compareBackings(previous_backing, backing));
        if (!backing->CanBeRecycled())
            reachedUnrecyclable = true;
        if (reachedUnrecyclable)
            DCHECK(!backing->CanBeRecycled());
        else
            DCHECK(backing->CanBeRecycled());
        previous_backing = backing;
    }
#endif
}

const Proxy* PrioritizedResourceManager::proxyForDebug() const
{
    return m_proxy;
}

}  // namespace cc
