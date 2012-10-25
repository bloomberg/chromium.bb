// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "cc/prioritized_texture.h"

#include "cc/platform_color.h"
#include "cc/prioritized_texture_manager.h"
#include "cc/priority_calculator.h"
#include "cc/proxy.h"
#include <algorithm>

using namespace std;

namespace cc {

PrioritizedTexture::PrioritizedTexture(PrioritizedTextureManager* manager, IntSize size, GLenum format)
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
    DCHECK(format || size.isEmpty());
    if (format)
        m_bytes = Texture::memorySizeBytes(size, format);
    if (manager)
        manager->registerTexture(this);
}

PrioritizedTexture::~PrioritizedTexture()
{
    if (m_manager)
        m_manager->unregisterTexture(this);
}

void PrioritizedTexture::setTextureManager(PrioritizedTextureManager* manager)
{
    if (m_manager == manager)
        return;
    if (m_manager)
        m_manager->unregisterTexture(this);
    if (manager)
        manager->registerTexture(this);
}

void PrioritizedTexture::setDimensions(IntSize size, GLenum format)
{
    if (m_format != format || m_size != size) {
        m_isAbovePriorityCutoff = false;
        m_format = format;
        m_size = size;
        m_bytes = Texture::memorySizeBytes(size, format);
        DCHECK(m_manager || !m_backing);
        if (m_manager)
            m_manager->returnBackingTexture(this);
    }
}

bool PrioritizedTexture::requestLate()
{
    if (!m_manager)
        return false;
    return m_manager->requestLate(this);
}

bool PrioritizedTexture::backingResourceWasEvicted() const
{
    return m_backing ? m_backing->resourceHasBeenDeleted() : false;
}

void PrioritizedTexture::acquireBackingTexture(ResourceProvider* resourceProvider)
{
    DCHECK(m_isAbovePriorityCutoff);
    if (m_isAbovePriorityCutoff)
        m_manager->acquireBackingTextureIfNeeded(this, resourceProvider);
}

ResourceProvider::ResourceId PrioritizedTexture::resourceId() const
{
    if (m_backing)
        return m_backing->id();
    return 0;
}

void PrioritizedTexture::upload(ResourceProvider* resourceProvider,
                                  const uint8_t* image, const IntRect& imageRect,
                                  const IntRect& sourceRect, const IntSize& destOffset)
{
    DCHECK(m_isAbovePriorityCutoff);
    if (m_isAbovePriorityCutoff)
        acquireBackingTexture(resourceProvider);
    DCHECK(m_backing);
    resourceProvider->upload(resourceId(), image, imageRect, sourceRect, destOffset);

    // The component order may be bgra if we uploaded bgra pixels to rgba
    // texture. Mark contents as swizzled if image component order is
    // different than texture format.
    m_contentsSwizzled = !PlatformColor::sameComponentOrder(m_format);
}

void PrioritizedTexture::link(Backing* backing)
{
    DCHECK(backing);
    DCHECK(!backing->m_owner);
    DCHECK(!m_backing);

    m_backing = backing;
    m_backing->m_owner = this;
}

void PrioritizedTexture::unlink()
{
    DCHECK(m_backing);
    DCHECK(m_backing->m_owner == this);

    m_backing->m_owner = 0;
    m_backing = 0;
}

void PrioritizedTexture::setToSelfManagedMemoryPlaceholder(size_t bytes)
{
    setDimensions(IntSize(), GL_RGBA);
    setIsSelfManaged(true);
    m_bytes = bytes;
}

PrioritizedTexture::Backing::Backing(unsigned id, ResourceProvider* resourceProvider, IntSize size, GLenum format)
    : Texture(id, size, format)
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

PrioritizedTexture::Backing::~Backing()
{
    DCHECK(!m_owner);
    DCHECK(m_resourceHasBeenDeleted);
}

void PrioritizedTexture::Backing::deleteResource(ResourceProvider* resourceProvider)
{
    DCHECK(Proxy::isImplThread());
    DCHECK(!m_resourceHasBeenDeleted);
#ifndef NDEBUG
    DCHECK(resourceProvider == m_resourceProvider);
#endif

    resourceProvider->deleteResource(id());
    setId(0);
    m_resourceHasBeenDeleted = true;
}

bool PrioritizedTexture::Backing::resourceHasBeenDeleted() const
{
    DCHECK(Proxy::isImplThread());
    return m_resourceHasBeenDeleted;
}

bool PrioritizedTexture::Backing::canBeRecycled() const
{
    DCHECK(Proxy::isImplThread());
    return !m_wasAbovePriorityCutoffAtLastPriorityUpdate && !m_inDrawingImplTree;
}

void PrioritizedTexture::Backing::updatePriority()
{
    DCHECK(Proxy::isImplThread() && Proxy::isMainThreadBlocked());
    if (m_owner) {
        m_priorityAtLastPriorityUpdate = m_owner->requestPriority();
        m_wasAbovePriorityCutoffAtLastPriorityUpdate = m_owner->isAbovePriorityCutoff();
    } else {
        m_priorityAtLastPriorityUpdate = PriorityCalculator::lowestPriority();
        m_wasAbovePriorityCutoffAtLastPriorityUpdate = false;
    }
}

void PrioritizedTexture::Backing::updateInDrawingImplTree()
{
    DCHECK(Proxy::isImplThread() && Proxy::isMainThreadBlocked());
    m_inDrawingImplTree = !!owner();
    if (!m_inDrawingImplTree)
        DCHECK(m_priorityAtLastPriorityUpdate == PriorityCalculator::lowestPriority());
}

void PrioritizedTexture::returnBackingTexture()
{
    DCHECK(m_manager || !m_backing);
    if (m_manager)
        m_manager->returnBackingTexture(this);
}

} // namespace cc
