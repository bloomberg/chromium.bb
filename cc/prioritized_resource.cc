// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/prioritized_resource.h"

#include <algorithm>

#include "cc/platform_color.h"
#include "cc/prioritized_resource_manager.h"
#include "cc/priority_calculator.h"
#include "cc/proxy.h"

namespace cc {

PrioritizedResource::PrioritizedResource(PrioritizedResourceManager* manager, gfx::Size size, GLenum format)
    : m_size(size)
    , m_format(format)
    , m_bytes(0)
    , m_contentsSwizzled(false)
    , m_priority(PriorityCalculator::lowestPriority())
    , m_isAbovePriorityCutoff(false)
    , m_isSelfManaged(false)
    , m_backing(0)
    , m_manager(0)
{
    // m_manager is set in registerTexture() so validity can be checked.
    DCHECK(format || size.IsEmpty());
    if (format)
        m_bytes = Resource::MemorySizeBytes(size, format);
    if (manager)
        manager->registerTexture(this);
}

PrioritizedResource::~PrioritizedResource()
{
    if (m_manager)
        m_manager->unregisterTexture(this);
}

void PrioritizedResource::setTextureManager(PrioritizedResourceManager* manager)
{
    if (m_manager == manager)
        return;
    if (m_manager)
        m_manager->unregisterTexture(this);
    if (manager)
        manager->registerTexture(this);
}

void PrioritizedResource::setDimensions(gfx::Size size, GLenum format)
{
    if (m_format != format || m_size != size) {
        m_isAbovePriorityCutoff = false;
        m_format = format;
        m_size = size;
        m_bytes = Resource::MemorySizeBytes(size, format);
        DCHECK(m_manager || !m_backing);
        if (m_manager)
            m_manager->returnBackingTexture(this);
    }
}

bool PrioritizedResource::requestLate()
{
    if (!m_manager)
        return false;
    return m_manager->requestLate(this);
}

bool PrioritizedResource::backingResourceWasEvicted() const
{
    return m_backing ? m_backing->resourceHasBeenDeleted() : false;
}

void PrioritizedResource::acquireBackingTexture(ResourceProvider* resourceProvider)
{
    DCHECK(m_isAbovePriorityCutoff);
    if (m_isAbovePriorityCutoff)
        m_manager->acquireBackingTextureIfNeeded(this, resourceProvider);
}

ResourceProvider::ResourceId PrioritizedResource::resourceId() const
{
    if (m_backing)
        return m_backing->id();
    return 0;
}

void PrioritizedResource::setPixels(ResourceProvider* resourceProvider,
                                   const uint8_t* image, const gfx::Rect& imageRect,
                                   const gfx::Rect& sourceRect, const gfx::Vector2d& destOffset)
{
    DCHECK(m_isAbovePriorityCutoff);
    if (m_isAbovePriorityCutoff)
        acquireBackingTexture(resourceProvider);
    DCHECK(m_backing);
    resourceProvider->setPixels(resourceId(), image, imageRect, sourceRect, destOffset);

    // The component order may be bgra if we uploaded bgra pixels to rgba
    // texture. Mark contents as swizzled if image component order is
    // different than texture format.
    m_contentsSwizzled = !PlatformColor::sameComponentOrder(m_format);
}

void PrioritizedResource::link(Backing* backing)
{
    DCHECK(backing);
    DCHECK(!backing->m_owner);
    DCHECK(!m_backing);

    m_backing = backing;
    m_backing->m_owner = this;
}

void PrioritizedResource::unlink()
{
    DCHECK(m_backing);
    DCHECK(m_backing->m_owner == this);

    m_backing->m_owner = 0;
    m_backing = 0;
}

void PrioritizedResource::setToSelfManagedMemoryPlaceholder(size_t bytes)
{
    setDimensions(gfx::Size(), GL_RGBA);
    setIsSelfManaged(true);
    m_bytes = bytes;
}

PrioritizedResource::Backing::Backing(unsigned id, ResourceProvider* resourceProvider, gfx::Size size, GLenum format)
    : Resource(id, size, format)
    , m_owner(0)
    , m_priorityAtLastPriorityUpdate(PriorityCalculator::lowestPriority())
    , m_wasAbovePriorityCutoffAtLastPriorityUpdate(false)
    , m_inDrawingImplTree(false)
    , m_resourceHasBeenDeleted(false)
#ifndef NDEBUG
    , m_resourceProvider(resourceProvider)
#endif
{
}

PrioritizedResource::Backing::~Backing()
{
    DCHECK(!m_owner);
    DCHECK(m_resourceHasBeenDeleted);
}

void PrioritizedResource::Backing::deleteResource(ResourceProvider* resourceProvider)
{
    DCHECK(!proxy() || proxy()->isImplThread());
    DCHECK(!m_resourceHasBeenDeleted);
#ifndef NDEBUG
    DCHECK(resourceProvider == m_resourceProvider);
#endif

    resourceProvider->deleteResource(id());
    set_id(0);
    m_resourceHasBeenDeleted = true;
}

bool PrioritizedResource::Backing::resourceHasBeenDeleted() const
{
    DCHECK(!proxy() || proxy()->isImplThread());
    return m_resourceHasBeenDeleted;
}

bool PrioritizedResource::Backing::canBeRecycled() const
{
    DCHECK(!proxy() || proxy()->isImplThread());
    return !m_wasAbovePriorityCutoffAtLastPriorityUpdate && !m_inDrawingImplTree;
}

void PrioritizedResource::Backing::updatePriority()
{
    DCHECK(!proxy() || proxy()->isImplThread() && proxy()->isMainThreadBlocked());
    if (m_owner) {
        m_priorityAtLastPriorityUpdate = m_owner->requestPriority();
        m_wasAbovePriorityCutoffAtLastPriorityUpdate = m_owner->isAbovePriorityCutoff();
    } else {
        m_priorityAtLastPriorityUpdate = PriorityCalculator::lowestPriority();
        m_wasAbovePriorityCutoffAtLastPriorityUpdate = false;
    }
}

void PrioritizedResource::Backing::updateInDrawingImplTree()
{
    DCHECK(!proxy() || proxy()->isImplThread() && proxy()->isMainThreadBlocked());
    m_inDrawingImplTree = !!owner();
    if (!m_inDrawingImplTree)
        DCHECK(m_priorityAtLastPriorityUpdate == PriorityCalculator::lowestPriority());
}

void PrioritizedResource::returnBackingTexture()
{
    DCHECK(m_manager || !m_backing);
    if (m_manager)
        m_manager->returnBackingTexture(this);
}

const Proxy* PrioritizedResource::Backing::proxy() const
{
    if (!m_owner || !m_owner->resourceManager())
        return 0;
    return m_owner->resourceManager()->proxyForDebug();
}

}  // namespace cc
