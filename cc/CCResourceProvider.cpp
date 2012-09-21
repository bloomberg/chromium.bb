// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "CCResourceProvider.h"
#ifdef LOG
#undef LOG
#endif

#include <limits.h>

#include "base/string_split.h"
#include "base/string_util.h"
#include "CCProxy.h"
#include "CCRendererGL.h" // For the GLC() macro.
#include "Extensions3DChromium.h"
#include "IntRect.h"
#include "LayerTextureSubImage.h"
#include "ThrottledTextureUploader.h"
#include "UnthrottledTextureUploader.h"
#include <public/WebGraphicsContext3D.h>
#include <wtf/HashSet.h>

using WebKit::WebGraphicsContext3D;

namespace cc {

static GC3Denum textureToStorageFormat(GC3Denum textureFormat)
{
    GC3Denum storageFormat = Extensions3D::RGBA8_OES;
    switch (textureFormat) {
    case GraphicsContext3D::RGBA:
        break;
    case Extensions3D::BGRA_EXT:
        storageFormat = Extensions3DChromium::BGRA8_EXT;
        break;
    default:
        ASSERT_NOT_REACHED();
        break;
    }

    return storageFormat;
}

static bool isTextureFormatSupportedForStorage(GC3Denum format)
{
    return (format == GraphicsContext3D::RGBA || format == Extensions3D::BGRA_EXT);
}

CCResourceProvider::TransferableResourceList::TransferableResourceList()
{
}

CCResourceProvider::TransferableResourceList::~TransferableResourceList()
{
}

CCResourceProvider::Resource::Resource()
    : glId(0)
    , pixels(0)
    , pool(0)
    , lockForReadCount(0)
    , lockedForWrite(false)
    , external(false)
    , exported(false)
    , size()
    , format(0)
    , type(static_cast<ResourceType>(0))
{
}

CCResourceProvider::Resource::Resource(unsigned textureId, int pool, const IntSize& size, GC3Denum format)
    : glId(textureId)
    , pixels(0)
    , pool(pool)
    , lockForReadCount(0)
    , lockedForWrite(false)
    , external(false)
    , exported(false)
    , size(size)
    , format(format)
    , type(GLTexture)
{
}

CCResourceProvider::Resource::Resource(uint8_t* pixels, int pool, const IntSize& size, GC3Denum format)
    : glId(0)
    , pixels(pixels)
    , pool(pool)
    , lockForReadCount(0)
    , lockedForWrite(false)
    , external(false)
    , exported(false)
    , size(size)
    , format(format)
    , type(Bitmap)
{
}

CCResourceProvider::Child::Child()
{
}

CCResourceProvider::Child::~Child()
{
}

PassOwnPtr<CCResourceProvider> CCResourceProvider::create(CCGraphicsContext* context, TextureUploaderOption option)
{
    OwnPtr<CCResourceProvider> resourceProvider(adoptPtr(new CCResourceProvider(context)));
    if (!resourceProvider->initialize(option))
        return nullptr;
    return resourceProvider.release();
}

CCResourceProvider::~CCResourceProvider()
{
    WebGraphicsContext3D* context3d = m_context->context3D();
    if (!context3d || !context3d->makeContextCurrent())
        return;
    m_textureUploader.clear();
    m_textureCopier.clear();
}

WebGraphicsContext3D* CCResourceProvider::graphicsContext3D()
{
    ASSERT(CCProxy::isImplThread());
    return m_context->context3D();
}

bool CCResourceProvider::inUseByConsumer(ResourceId id)
{
    ASSERT(CCProxy::isImplThread());
    ResourceMap::iterator it = m_resources.find(id);
    CHECK(it != m_resources.end());
#if WTF_NEW_HASHMAP_ITERATORS_INTERFACE
    Resource* resource = &it->value;
#else
    Resource* resource = &it->second;
#endif
    return !!resource->lockForReadCount || resource->exported;
}

CCResourceProvider::ResourceId CCResourceProvider::createResource(int pool, const IntSize& size, GC3Denum format, TextureUsageHint hint)
{
    switch (m_defaultResourceType) {
    case GLTexture:
        return createGLTexture(pool, size, format, hint);
    case Bitmap:
        ASSERT(format == GraphicsContext3D::RGBA);
        return createBitmap(pool, size);
    }

    CRASH();
    return 0;
}

CCResourceProvider::ResourceId CCResourceProvider::createGLTexture(int pool, const IntSize& size, GC3Denum format, TextureUsageHint hint)
{
    ASSERT(CCProxy::isImplThread());
    unsigned textureId = 0;
    WebGraphicsContext3D* context3d = m_context->context3D();
    ASSERT(context3d);
    GLC(context3d, textureId = context3d->createTexture());
    GLC(context3d, context3d->bindTexture(GraphicsContext3D::TEXTURE_2D, textureId));
    GLC(context3d, context3d->texParameteri(GraphicsContext3D::TEXTURE_2D, GraphicsContext3D::TEXTURE_MIN_FILTER, GraphicsContext3D::LINEAR));
    GLC(context3d, context3d->texParameteri(GraphicsContext3D::TEXTURE_2D, GraphicsContext3D::TEXTURE_MAG_FILTER, GraphicsContext3D::LINEAR));
    GLC(context3d, context3d->texParameteri(GraphicsContext3D::TEXTURE_2D, GraphicsContext3D::TEXTURE_WRAP_S, GraphicsContext3D::CLAMP_TO_EDGE));
    GLC(context3d, context3d->texParameteri(GraphicsContext3D::TEXTURE_2D, GraphicsContext3D::TEXTURE_WRAP_T, GraphicsContext3D::CLAMP_TO_EDGE));

    if (m_useTextureUsageHint && hint == TextureUsageFramebuffer)
        GLC(context3d, context3d->texParameteri(GraphicsContext3D::TEXTURE_2D, Extensions3DChromium::GL_TEXTURE_USAGE_ANGLE, Extensions3DChromium::GL_FRAMEBUFFER_ATTACHMENT_ANGLE));
    if (m_useTextureStorageExt && isTextureFormatSupportedForStorage(format)) {
        GC3Denum storageFormat = textureToStorageFormat(format);
        GLC(context3d, context3d->texStorage2DEXT(GraphicsContext3D::TEXTURE_2D, 1, storageFormat, size.width(), size.height()));
    } else
        GLC(context3d, context3d->texImage2D(GraphicsContext3D::TEXTURE_2D, 0, format, size.width(), size.height(), 0, format, GraphicsContext3D::UNSIGNED_BYTE, 0));
    ResourceId id = m_nextId++;
    Resource resource(textureId, pool, size, format);
    m_resources.add(id, resource);
    return id;
}

CCResourceProvider::ResourceId CCResourceProvider::createBitmap(int pool, const IntSize& size)
{
    ASSERT(CCProxy::isImplThread());

    uint8_t* pixels = new uint8_t[size.width() * size.height() * 4];

    ResourceId id = m_nextId++;
    Resource resource(pixels, pool, size, GraphicsContext3D::RGBA);
    m_resources.add(id, resource);
    return id;
}

CCResourceProvider::ResourceId CCResourceProvider::createResourceFromExternalTexture(unsigned textureId)
{
    ASSERT(CCProxy::isImplThread());
    ASSERT(m_context->context3D());
    ResourceId id = m_nextId++;
    Resource resource(textureId, 0, IntSize(), 0);
    resource.external = true;
    m_resources.add(id, resource);
    return id;
}

void CCResourceProvider::deleteResource(ResourceId id)
{
    ASSERT(CCProxy::isImplThread());
    ResourceMap::iterator it = m_resources.find(id);
    CHECK(it != m_resources.end());
#if WTF_NEW_HASHMAP_ITERATORS_INTERFACE
    Resource* resource = &it->value;
#else
    Resource* resource = &it->second;
#endif
    ASSERT(!resource->lockedForWrite);
    ASSERT(!resource->lockForReadCount);

    if (resource->glId && !resource->external) {
        WebGraphicsContext3D* context3d = m_context->context3D();
        ASSERT(context3d);
        GLC(context3d, context3d->deleteTexture(resource->glId));
    }
    if (resource->pixels)
        delete resource->pixels;

    m_resources.remove(it);
}

void CCResourceProvider::deleteOwnedResources(int pool)
{
    ASSERT(CCProxy::isImplThread());
    ResourceIdArray toDelete;
    for (ResourceMap::iterator it = m_resources.begin(); it != m_resources.end(); ++it) {
#if WTF_NEW_HASHMAP_ITERATORS_INTERFACE
        if (it->value.pool == pool && !it->value.external)
            toDelete.append(it->key);
#else
        if (it->second.pool == pool && !it->second.external)
            toDelete.append(it->first);
#endif
    }
    for (ResourceIdArray::iterator it = toDelete.begin(); it != toDelete.end(); ++it)
        deleteResource(*it);
}

CCResourceProvider::ResourceType CCResourceProvider::resourceType(ResourceId id)
{
    ResourceMap::iterator it = m_resources.find(id);
    CHECK(it != m_resources.end());
#if WTF_NEW_HASHMAP_ITERATORS_INTERFACE
    Resource* resource = &it->value;
#else
    Resource* resource = &it->second;
#endif
    return resource->type;
}

void CCResourceProvider::upload(ResourceId id, const uint8_t* image, const IntRect& imageRect, const IntRect& sourceRect, const IntSize& destOffset)
{
    ASSERT(CCProxy::isImplThread());
    ResourceMap::iterator it = m_resources.find(id);
    CHECK(it != m_resources.end());
#if WTF_NEW_HASHMAP_ITERATORS_INTERFACE
    Resource* resource = &it->value;
#else
    Resource* resource = &it->second;
#endif
    ASSERT(!resource->lockedForWrite);
    ASSERT(!resource->lockForReadCount);
    ASSERT(!resource->external);

    if (resource->glId) {
        WebGraphicsContext3D* context3d = m_context->context3D();
        ASSERT(context3d);
        ASSERT(m_texSubImage.get());
        context3d->bindTexture(GraphicsContext3D::TEXTURE_2D, resource->glId);
        m_texSubImage->upload(image, imageRect, sourceRect, destOffset, resource->format, context3d);
    }

    if (resource->pixels) {
        SkBitmap srcFull;
        srcFull.setConfig(SkBitmap::kARGB_8888_Config, imageRect.width(), imageRect.height());
        srcFull.setPixels(const_cast<uint8_t*>(image));
        SkBitmap srcSubset;
        SkIRect skSourceRect = SkIRect::MakeXYWH(sourceRect.x(), sourceRect.y(), sourceRect.width(), sourceRect.height());
        skSourceRect.offset(-imageRect.x(), -imageRect.y());
        srcFull.extractSubset(&srcSubset, skSourceRect);

        ScopedWriteLockSoftware lock(this, id);
        SkCanvas* dest = lock.skCanvas();
        dest->writePixels(srcSubset, destOffset.width(), destOffset.height());
    }
}

void CCResourceProvider::flush()
{
    ASSERT(CCProxy::isImplThread());
    WebGraphicsContext3D* context3d = m_context->context3D();
    if (context3d)
        context3d->flush();
}

bool CCResourceProvider::shallowFlushIfSupported()
{
    ASSERT(CCProxy::isImplThread());
    WebGraphicsContext3D* context3d = m_context->context3D();
    if (!context3d || !m_useShallowFlush)
        return false;

    context3d->shallowFlushCHROMIUM();
    return true;
}

const CCResourceProvider::Resource* CCResourceProvider::lockForRead(ResourceId id)
{
    ASSERT(CCProxy::isImplThread());
    ResourceMap::iterator it = m_resources.find(id);
    CHECK(it != m_resources.end());
#if WTF_NEW_HASHMAP_ITERATORS_INTERFACE
    Resource* resource = &it->value;
#else
    Resource* resource = &it->second;
#endif
    ASSERT(!resource->lockedForWrite);
    resource->lockForReadCount++;
    return resource;
}

void CCResourceProvider::unlockForRead(ResourceId id)
{
    ASSERT(CCProxy::isImplThread());
    ResourceMap::iterator it = m_resources.find(id);
    CHECK(it != m_resources.end());
#if WTF_NEW_HASHMAP_ITERATORS_INTERFACE
    Resource* resource = &it->value;
#else
    Resource* resource = &it->second;
#endif
    ASSERT(resource->lockForReadCount > 0);
    resource->lockForReadCount--;
}

const CCResourceProvider::Resource* CCResourceProvider::lockForWrite(ResourceId id)
{
    ASSERT(CCProxy::isImplThread());
    ResourceMap::iterator it = m_resources.find(id);
    CHECK(it != m_resources.end());
#if WTF_NEW_HASHMAP_ITERATORS_INTERFACE
    Resource* resource = &it->value;
#else
    Resource* resource = &it->second;
#endif
    ASSERT(!resource->lockedForWrite);
    ASSERT(!resource->lockForReadCount);
    ASSERT(!resource->external);
    resource->lockedForWrite = true;
    return resource;
}

void CCResourceProvider::unlockForWrite(ResourceId id)
{
    ASSERT(CCProxy::isImplThread());
    ResourceMap::iterator it = m_resources.find(id);
    CHECK(it != m_resources.end());
#if WTF_NEW_HASHMAP_ITERATORS_INTERFACE
    Resource* resource = &it->value;
#else
    Resource* resource = &it->second;
#endif
    ASSERT(resource->lockedForWrite);
    ASSERT(!resource->external);
    resource->lockedForWrite = false;
}

CCResourceProvider::ScopedReadLockGL::ScopedReadLockGL(CCResourceProvider* resourceProvider, CCResourceProvider::ResourceId resourceId)
    : m_resourceProvider(resourceProvider)
    , m_resourceId(resourceId)
    , m_textureId(resourceProvider->lockForRead(resourceId)->glId)
{
    ASSERT(m_textureId);
}

CCResourceProvider::ScopedReadLockGL::~ScopedReadLockGL()
{
    m_resourceProvider->unlockForRead(m_resourceId);
}

CCResourceProvider::ScopedWriteLockGL::ScopedWriteLockGL(CCResourceProvider* resourceProvider, CCResourceProvider::ResourceId resourceId)
    : m_resourceProvider(resourceProvider)
    , m_resourceId(resourceId)
    , m_textureId(resourceProvider->lockForWrite(resourceId)->glId)
{
    ASSERT(m_textureId);
}

CCResourceProvider::ScopedWriteLockGL::~ScopedWriteLockGL()
{
    m_resourceProvider->unlockForWrite(m_resourceId);
}

void CCResourceProvider::populateSkBitmapWithResource(SkBitmap* skBitmap, const Resource* resource)
{
    ASSERT(resource->pixels);
    ASSERT(resource->format == GraphicsContext3D::RGBA);
    skBitmap->setConfig(SkBitmap::kARGB_8888_Config, resource->size.width(), resource->size.height());
    skBitmap->setPixels(resource->pixels);
}

CCResourceProvider::ScopedReadLockSoftware::ScopedReadLockSoftware(CCResourceProvider* resourceProvider, CCResourceProvider::ResourceId resourceId)
    : m_resourceProvider(resourceProvider)
    , m_resourceId(resourceId)
{
    CCResourceProvider::populateSkBitmapWithResource(&m_skBitmap, resourceProvider->lockForRead(resourceId));
}

CCResourceProvider::ScopedReadLockSoftware::~ScopedReadLockSoftware()
{
    m_resourceProvider->unlockForRead(m_resourceId);
}

CCResourceProvider::ScopedWriteLockSoftware::ScopedWriteLockSoftware(CCResourceProvider* resourceProvider, CCResourceProvider::ResourceId resourceId)
    : m_resourceProvider(resourceProvider)
    , m_resourceId(resourceId)
{
    CCResourceProvider::populateSkBitmapWithResource(&m_skBitmap, resourceProvider->lockForWrite(resourceId));
    m_skCanvas.setBitmapDevice(m_skBitmap);
}

CCResourceProvider::ScopedWriteLockSoftware::~ScopedWriteLockSoftware()
{
    m_resourceProvider->unlockForWrite(m_resourceId);
}

CCResourceProvider::CCResourceProvider(CCGraphicsContext* context)
    : m_context(context)
    , m_nextId(1)
    , m_nextChild(1)
    , m_defaultResourceType(GLTexture)
    , m_useTextureStorageExt(false)
    , m_useTextureUsageHint(false)
    , m_useShallowFlush(false)
    , m_maxTextureSize(0)
{
}

bool CCResourceProvider::initialize(TextureUploaderOption textureUploaderOption)
{
    ASSERT(CCProxy::isImplThread());
    WebGraphicsContext3D* context3d = m_context->context3D();
    if (!context3d) {
        m_maxTextureSize = INT_MAX;
        m_textureUploader = UnthrottledTextureUploader::create();

        // FIXME: Implement this path for software compositing.
        return false;
    }
    if (!context3d->makeContextCurrent())
        return false;

    std::string extensionsString = UTF16ToASCII(context3d->getString(GraphicsContext3D::EXTENSIONS));
    std::vector<std::string> extensions;
    base::SplitString(extensionsString, ' ', &extensions);
    bool useMapSub = false;
    bool useBindUniform = false;
    for (size_t i = 0; i < extensions.size(); ++i) {
        if (extensions[i] == "GL_EXT_texture_storage")
            m_useTextureStorageExt = true;
        else if (extensions[i] == "GL_ANGLE_texture_usage")
            m_useTextureUsageHint = true;
        else if (extensions[i] == "GL_CHROMIUM_map_sub")
            useMapSub = true;
        else if (extensions[i] == "GL_CHROMIUM_shallow_flush")
            m_useShallowFlush = true;
        else if (extensions[i] == "GL_CHROMIUM_bind_uniform_location")
            useBindUniform = true;
    }

    m_texSubImage = adoptPtr(new LayerTextureSubImage(useMapSub));
    m_textureCopier = AcceleratedTextureCopier::create(context3d, useBindUniform);

    if (textureUploaderOption == ThrottledUploader)
        m_textureUploader = ThrottledTextureUploader::create(context3d);
    else
        m_textureUploader = UnthrottledTextureUploader::create();
    GLC(context3d, context3d->getIntegerv(GraphicsContext3D::MAX_TEXTURE_SIZE, &m_maxTextureSize));
    return true;
}

int CCResourceProvider::createChild(int pool)
{
    ASSERT(CCProxy::isImplThread());
    Child childInfo;
    childInfo.pool = pool;
    int child = m_nextChild++;
    m_children.add(child, childInfo);
    return child;
}

void CCResourceProvider::destroyChild(int child)
{
    ASSERT(CCProxy::isImplThread());
    ChildMap::iterator it = m_children.find(child);
    ASSERT(it != m_children.end());
#if WTF_NEW_HASHMAP_ITERATORS_INTERFACE
    deleteOwnedResources(it->value.pool);
#else
    deleteOwnedResources(it->second.pool);
#endif
    m_children.remove(it);
    trimMailboxDeque();
}

const CCResourceProvider::ResourceIdMap& CCResourceProvider::getChildToParentMap(int child) const
{
    ASSERT(CCProxy::isImplThread());
    ChildMap::const_iterator it = m_children.find(child);
    ASSERT(it != m_children.end());
#if WTF_NEW_HASHMAP_ITERATORS_INTERFACE
    return it->value.childToParentMap;
#else
    return it->second.childToParentMap;
#endif
}

CCResourceProvider::TransferableResourceList CCResourceProvider::prepareSendToParent(const ResourceIdArray& resources)
{
    ASSERT(CCProxy::isImplThread());
    TransferableResourceList list;
    list.syncPoint = 0;
    WebGraphicsContext3D* context3d = m_context->context3D();
    if (!context3d || !context3d->makeContextCurrent()) {
        // FIXME: Implement this path for software compositing.
        return list;
    }
    for (ResourceIdArray::const_iterator it = resources.begin(); it != resources.end(); ++it) {
        TransferableResource resource;
        if (transferResource(context3d, *it, &resource)) {
#if WTF_NEW_HASHMAP_ITERATORS_INTERFACE
            m_resources.find(*it)->value.exported = true;
#else
            m_resources.find(*it)->second.exported = true;
#endif
            list.resources.append(resource);
        }
    }
    if (list.resources.size())
        list.syncPoint = context3d->insertSyncPoint();
    return list;
}

CCResourceProvider::TransferableResourceList CCResourceProvider::prepareSendToChild(int child, const ResourceIdArray& resources)
{
    ASSERT(CCProxy::isImplThread());
    TransferableResourceList list;
    list.syncPoint = 0;
    WebGraphicsContext3D* context3d = m_context->context3D();
    if (!context3d || !context3d->makeContextCurrent()) {
        // FIXME: Implement this path for software compositing.
        return list;
    }
#if WTF_NEW_HASHMAP_ITERATORS_INTERFACE
    Child& childInfo = m_children.find(child)->value;
#else
    Child& childInfo = m_children.find(child)->second;
#endif
    for (ResourceIdArray::const_iterator it = resources.begin(); it != resources.end(); ++it) {
        TransferableResource resource;
        if (!transferResource(context3d, *it, &resource))
            ASSERT_NOT_REACHED();
        resource.id = childInfo.parentToChildMap.get(*it);
        childInfo.parentToChildMap.remove(*it);
        childInfo.childToParentMap.remove(resource.id);
        list.resources.append(resource);
        deleteResource(*it);
    }
    if (list.resources.size())
        list.syncPoint = context3d->insertSyncPoint();
    return list;
}

void CCResourceProvider::receiveFromChild(int child, const TransferableResourceList& resources)
{
    ASSERT(CCProxy::isImplThread());
    WebGraphicsContext3D* context3d = m_context->context3D();
    if (!context3d || !context3d->makeContextCurrent()) {
        // FIXME: Implement this path for software compositing.
        return;
    }
    if (resources.syncPoint) {
        // NOTE: If the parent is a browser and the child a renderer, the parent
        // is not supposed to have its context wait, because that could induce
        // deadlocks and/or security issues. The caller is responsible for
        // waiting asynchronously, and resetting syncPoint before calling this.
        // However if the parent is a renderer (e.g. browser tag), it may be ok
        // (and is simpler) to wait.
        GLC(context3d, context3d->waitSyncPoint(resources.syncPoint));
    }
#if WTF_NEW_HASHMAP_ITERATORS_INTERFACE
    Child& childInfo = m_children.find(child)->value;
#else
    Child& childInfo = m_children.find(child)->second;
#endif
    for (Vector<TransferableResource>::const_iterator it = resources.resources.begin(); it != resources.resources.end(); ++it) {
        unsigned textureId;
        GLC(context3d, textureId = context3d->createTexture());
        GLC(context3d, context3d->bindTexture(GraphicsContext3D::TEXTURE_2D, textureId));
        GLC(context3d, context3d->consumeTextureCHROMIUM(GraphicsContext3D::TEXTURE_2D, it->mailbox.name));
        ResourceId id = m_nextId++;
        Resource resource(textureId, childInfo.pool, it->size, it->format);
        m_resources.add(id, resource);
        m_mailboxes.append(it->mailbox);
        childInfo.parentToChildMap.add(id, it->id);
        childInfo.childToParentMap.add(it->id, id);
    }
}

void CCResourceProvider::receiveFromParent(const TransferableResourceList& resources)
{
    ASSERT(CCProxy::isImplThread());
    WebGraphicsContext3D* context3d = m_context->context3D();
    if (!context3d || !context3d->makeContextCurrent()) {
        // FIXME: Implement this path for software compositing.
        return;
    }
    if (resources.syncPoint)
        GLC(context3d, context3d->waitSyncPoint(resources.syncPoint));
    for (Vector<TransferableResource>::const_iterator it = resources.resources.begin(); it != resources.resources.end(); ++it) {
#if WTF_NEW_HASHMAP_ITERATORS_INTERFACE
        Resource* resource = &m_resources.find(it->id)->value;
#else
        Resource* resource = &m_resources.find(it->id)->second;
#endif
        ASSERT(resource->exported);
        resource->exported = false;
        GLC(context3d, context3d->bindTexture(GraphicsContext3D::TEXTURE_2D, resource->glId));
        GLC(context3d, context3d->consumeTextureCHROMIUM(GraphicsContext3D::TEXTURE_2D, it->mailbox.name));
        m_mailboxes.append(it->mailbox);
    }
}

bool CCResourceProvider::transferResource(WebGraphicsContext3D* context, ResourceId id, TransferableResource* resource)
{
    ASSERT(CCProxy::isImplThread());
    ResourceMap::const_iterator it = m_resources.find(id);
    CHECK(it != m_resources.end());
#if WTF_NEW_HASHMAP_ITERATORS_INTERFACE
    const Resource* source = &it->value;
#else
    const Resource* source = &it->second;
#endif
    ASSERT(!source->lockedForWrite);
    ASSERT(!source->lockForReadCount);
    ASSERT(!source->external);
    if (source->exported)
        return false;
    resource->id = id;
    resource->format = source->format;
    resource->size = source->size;
    if (!m_mailboxes.isEmpty())
        resource->mailbox = m_mailboxes.takeFirst();
    else
        GLC(context, context->genMailboxCHROMIUM(resource->mailbox.name));
    GLC(context, context->bindTexture(GraphicsContext3D::TEXTURE_2D, source->glId));
    GLC(context, context->produceTextureCHROMIUM(GraphicsContext3D::TEXTURE_2D, resource->mailbox.name));
    return true;
}

void CCResourceProvider::trimMailboxDeque()
{
    // Trim the mailbox deque to the maximum number of resources we may need to
    // send.
    // If we have a parent, any non-external resource not already transfered is
    // eligible to be sent to the parent. Otherwise, all resources belonging to
    // a child might need to be sent back to the child.
    size_t maxMailboxCount = 0;
    if (m_context->capabilities().hasParentCompositor) {
        for (ResourceMap::iterator it = m_resources.begin(); it != m_resources.end(); ++it) {
#if WTF_NEW_HASHMAP_ITERATORS_INTERFACE
            if (!it->value.exported && !it->value.external)
#else
            if (!it->second.exported && !it->second.external)
#endif
                ++maxMailboxCount;
        }
    } else {
        HashSet<int> childPoolSet;
        for (ChildMap::iterator it = m_children.begin(); it != m_children.end(); ++it)
#if WTF_NEW_HASHMAP_ITERATORS_INTERFACE
            childPoolSet.add(it->value.pool);
#else
            childPoolSet.add(it->second.pool);
#endif
        for (ResourceMap::iterator it = m_resources.begin(); it != m_resources.end(); ++it) {
#if WTF_NEW_HASHMAP_ITERATORS_INTERFACE
            if (childPoolSet.contains(it->value.pool))
#else
            if (childPoolSet.contains(it->second.pool))
#endif
                ++maxMailboxCount;
        }
    }
    while (m_mailboxes.size() > maxMailboxCount)
        m_mailboxes.removeFirst();
}

}
