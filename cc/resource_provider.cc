// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "cc/resource_provider.h"

#include <limits.h>

#include "CCProxy.h"
#include "IntRect.h"
#include "base/debug/alias.h"
#include "base/hash_tables.h"
#include "base/stl_util.h"
#include "base/string_split.h"
#include "base/string_util.h"
#include "cc/gl_renderer.h" // For the GLC() macro.
#include "cc/texture_uploader.h"
#include "third_party/khronos/GLES2/gl2.h"
#include "third_party/khronos/GLES2/gl2ext.h"

#include <public/WebGraphicsContext3D.h>

using WebKit::WebGraphicsContext3D;

namespace cc {

static GLenum textureToStorageFormat(GLenum textureFormat)
{
    GLenum storageFormat = GL_RGBA8_OES;
    switch (textureFormat) {
    case GL_RGBA:
        break;
    case GL_BGRA_EXT:
        storageFormat = GL_BGRA8_EXT;
        break;
    default:
        NOTREACHED();
        break;
    }

    return storageFormat;
}

static bool isTextureFormatSupportedForStorage(GLenum format)
{
    return (format == GL_RGBA || format == GL_BGRA_EXT);
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
    , markedForDeletion(false)
    , size()
    , format(0)
    , type(static_cast<ResourceType>(0))
{
}

CCResourceProvider::Resource::Resource(unsigned textureId, int pool, const IntSize& size, GLenum format)
    : glId(textureId)
    , pixels(0)
    , pool(pool)
    , lockForReadCount(0)
    , lockedForWrite(false)
    , external(false)
    , exported(false)
    , markedForDeletion(false)
    , size(size)
    , format(format)
    , type(GLTexture)
{
}

CCResourceProvider::Resource::Resource(uint8_t* pixels, int pool, const IntSize& size, GLenum format)
    : glId(0)
    , pixels(pixels)
    , pool(pool)
    , lockForReadCount(0)
    , lockedForWrite(false)
    , external(false)
    , exported(false)
    , markedForDeletion(false)
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

scoped_ptr<CCResourceProvider> CCResourceProvider::create(CCGraphicsContext* context)
{
    scoped_ptr<CCResourceProvider> resourceProvider(new CCResourceProvider(context));
    if (!resourceProvider->initialize())
        return scoped_ptr<CCResourceProvider>();
    return resourceProvider.Pass();
}

CCResourceProvider::~CCResourceProvider()
{
    WebGraphicsContext3D* context3d = m_context->context3D();
    if (!context3d || !context3d->makeContextCurrent())
        return;
    m_textureUploader.reset();
    m_textureCopier.reset();
}

WebGraphicsContext3D* CCResourceProvider::graphicsContext3D()
{
    DCHECK(CCProxy::isImplThread());
    return m_context->context3D();
}

bool CCResourceProvider::inUseByConsumer(ResourceId id)
{
    DCHECK(CCProxy::isImplThread());
    ResourceMap::iterator it = m_resources.find(id);
    CHECK(it != m_resources.end());
    Resource* resource = &it->second;
    return !!resource->lockForReadCount || resource->exported;
}

CCResourceProvider::ResourceId CCResourceProvider::createResource(int pool, const IntSize& size, GLenum format, TextureUsageHint hint)
{
    switch (m_defaultResourceType) {
    case GLTexture:
        return createGLTexture(pool, size, format, hint);
    case Bitmap:
        DCHECK(format == GL_RGBA);
        return createBitmap(pool, size);
    }

    CRASH();
    return 0;
}

CCResourceProvider::ResourceId CCResourceProvider::createGLTexture(int pool, const IntSize& size, GLenum format, TextureUsageHint hint)
{
    DCHECK(CCProxy::isImplThread());
    unsigned textureId = 0;
    WebGraphicsContext3D* context3d = m_context->context3D();
    DCHECK(context3d);
    GLC(context3d, textureId = context3d->createTexture());
    GLC(context3d, context3d->bindTexture(GL_TEXTURE_2D, textureId));
    GLC(context3d, context3d->texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
    GLC(context3d, context3d->texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
    GLC(context3d, context3d->texParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
    GLC(context3d, context3d->texParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));

    if (m_useTextureUsageHint && hint == TextureUsageFramebuffer)
        GLC(context3d, context3d->texParameteri(GL_TEXTURE_2D, GL_TEXTURE_USAGE_ANGLE, GL_FRAMEBUFFER_ATTACHMENT_ANGLE));
    if (m_useTextureStorageExt && isTextureFormatSupportedForStorage(format)) {
        GLenum storageFormat = textureToStorageFormat(format);
        GLC(context3d, context3d->texStorage2DEXT(GL_TEXTURE_2D, 1, storageFormat, size.width(), size.height()));
    } else
        GLC(context3d, context3d->texImage2D(GL_TEXTURE_2D, 0, format, size.width(), size.height(), 0, format, GL_UNSIGNED_BYTE, 0));
    ResourceId id = m_nextId++;
    Resource resource(textureId, pool, size, format);
    m_resources[id] = resource;
    return id;
}

CCResourceProvider::ResourceId CCResourceProvider::createBitmap(int pool, const IntSize& size)
{
    DCHECK(CCProxy::isImplThread());

    uint8_t* pixels = new uint8_t[size.width() * size.height() * 4];

    ResourceId id = m_nextId++;
    Resource resource(pixels, pool, size, GL_RGBA);
    m_resources[id] = resource;
    return id;
}

CCResourceProvider::ResourceId CCResourceProvider::createResourceFromExternalTexture(unsigned textureId)
{
    DCHECK(CCProxy::isImplThread());
    DCHECK(m_context->context3D());
    ResourceId id = m_nextId++;
    Resource resource(textureId, 0, IntSize(), 0);
    resource.external = true;
    m_resources[id] = resource;
    return id;
}

void CCResourceProvider::deleteResource(ResourceId id)
{
    DCHECK(CCProxy::isImplThread());
    ResourceMap::iterator it = m_resources.find(id);
    CHECK(it != m_resources.end());
    Resource* resource = &it->second;
    DCHECK(!resource->lockedForWrite);
    DCHECK(!resource->lockForReadCount);
    DCHECK(!resource->markedForDeletion);

    if (resource->exported) {
        resource->markedForDeletion = true;
        return;
    } else
        deleteResourceInternal(it);
}

void CCResourceProvider::deleteResourceInternal(ResourceMap::iterator it)
{
    Resource* resource = &it->second;
    if (resource->glId && !resource->external) {
        WebGraphicsContext3D* context3d = m_context->context3D();
        DCHECK(context3d);
        GLC(context3d, context3d->deleteTexture(resource->glId));
    }
    if (resource->pixels)
        delete resource->pixels;

    m_resources.erase(it);
}

void CCResourceProvider::deleteOwnedResources(int pool)
{
    DCHECK(CCProxy::isImplThread());
    ResourceIdArray toDelete;
    for (ResourceMap::iterator it = m_resources.begin(); it != m_resources.end(); ++it) {
        if (it->second.pool == pool && !it->second.external && !it->second.markedForDeletion)
            toDelete.push_back(it->first);
    }
    for (ResourceIdArray::iterator it = toDelete.begin(); it != toDelete.end(); ++it)
        deleteResource(*it);
}

CCResourceProvider::ResourceType CCResourceProvider::resourceType(ResourceId id)
{
    ResourceMap::iterator it = m_resources.find(id);
    CHECK(it != m_resources.end());
    Resource* resource = &it->second;
    return resource->type;
}

void CCResourceProvider::upload(ResourceId id, const uint8_t* image, const IntRect& imageRect, const IntRect& sourceRect, const IntSize& destOffset)
{
    DCHECK(CCProxy::isImplThread());
    ResourceMap::iterator it = m_resources.find(id);
    CHECK(it != m_resources.end());
    Resource* resource = &it->second;
    DCHECK(!resource->lockedForWrite);
    DCHECK(!resource->lockForReadCount);
    DCHECK(!resource->external);
    DCHECK(!resource->exported);

    if (resource->glId) {
        WebGraphicsContext3D* context3d = m_context->context3D();
        DCHECK(context3d);
        DCHECK(m_textureUploader.get());
        context3d->bindTexture(GL_TEXTURE_2D, resource->glId);
        m_textureUploader->upload(image,
                                  imageRect,
                                  sourceRect,
                                  destOffset,
                                  resource->format,
                                  resource->size);
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

size_t CCResourceProvider::numBlockingUploads()
{
    if (!m_textureUploader)
        return 0;

    return m_textureUploader->numBlockingUploads();
}

void CCResourceProvider::markPendingUploadsAsNonBlocking()
{
    if (!m_textureUploader)
        return;

    m_textureUploader->markPendingUploadsAsNonBlocking();
}

double CCResourceProvider::estimatedUploadsPerSecond()
{
    if (!m_textureUploader)
        return 0.0;

    return m_textureUploader->estimatedTexturesPerSecond();
}

void CCResourceProvider::flush()
{
    DCHECK(CCProxy::isImplThread());
    WebGraphicsContext3D* context3d = m_context->context3D();
    if (context3d)
        context3d->flush();
}

bool CCResourceProvider::shallowFlushIfSupported()
{
    DCHECK(CCProxy::isImplThread());
    WebGraphicsContext3D* context3d = m_context->context3D();
    if (!context3d || !m_useShallowFlush)
        return false;

    context3d->shallowFlushCHROMIUM();
    return true;
}

const CCResourceProvider::Resource* CCResourceProvider::lockForRead(ResourceId id)
{
    DCHECK(CCProxy::isImplThread());
    ResourceMap::iterator it = m_resources.find(id);
    CHECK(it != m_resources.end());

    Resource* resource = &it->second;
    DCHECK(!resource->lockedForWrite);
    DCHECK(!resource->exported);
    resource->lockForReadCount++;
    return resource;
}

void CCResourceProvider::unlockForRead(ResourceId id)
{
    DCHECK(CCProxy::isImplThread());
    ResourceMap::iterator it = m_resources.find(id);
    CHECK(it != m_resources.end());
    Resource* resource = &it->second;
    DCHECK(resource->lockForReadCount > 0);
    DCHECK(!resource->exported);
    resource->lockForReadCount--;
}

const CCResourceProvider::Resource* CCResourceProvider::lockForWrite(ResourceId id)
{
    DCHECK(CCProxy::isImplThread());
    ResourceMap::iterator it = m_resources.find(id);
    CHECK(it != m_resources.end());
    Resource* resource = &it->second;
    DCHECK(!resource->lockedForWrite);
    DCHECK(!resource->lockForReadCount);
    DCHECK(!resource->exported);
    DCHECK(!resource->external);
    resource->lockedForWrite = true;
    return resource;
}

void CCResourceProvider::unlockForWrite(ResourceId id)
{
    DCHECK(CCProxy::isImplThread());
    ResourceMap::iterator it = m_resources.find(id);
    CHECK(it != m_resources.end());
    Resource* resource = &it->second;
    DCHECK(resource->lockedForWrite);
    DCHECK(!resource->exported);
    DCHECK(!resource->external);
    resource->lockedForWrite = false;
}

CCResourceProvider::ScopedReadLockGL::ScopedReadLockGL(CCResourceProvider* resourceProvider, CCResourceProvider::ResourceId resourceId)
    : m_resourceProvider(resourceProvider)
    , m_resourceId(resourceId)
    , m_textureId(resourceProvider->lockForRead(resourceId)->glId)
{
    DCHECK(m_textureId);
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
    DCHECK(m_textureId);
}

CCResourceProvider::ScopedWriteLockGL::~ScopedWriteLockGL()
{
    m_resourceProvider->unlockForWrite(m_resourceId);
}

void CCResourceProvider::populateSkBitmapWithResource(SkBitmap* skBitmap, const Resource* resource)
{
    DCHECK(resource->pixels);
    DCHECK(resource->format == GL_RGBA);
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
    m_skCanvas.reset(new SkCanvas(m_skBitmap));
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

bool CCResourceProvider::initialize()
{
    DCHECK(CCProxy::isImplThread());
    WebGraphicsContext3D* context3d = m_context->context3D();
    if (!context3d) {
        m_maxTextureSize = INT_MAX / 2;
        return true;
    }
    if (!context3d->makeContextCurrent())
        return false;

    std::string extensionsString = UTF16ToASCII(context3d->getString(GL_EXTENSIONS));
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

    m_textureCopier = AcceleratedTextureCopier::create(context3d, useBindUniform);

    m_textureUploader = TextureUploader::create(context3d, useMapSub);
    GLC(context3d, context3d->getIntegerv(GL_MAX_TEXTURE_SIZE, &m_maxTextureSize));
    return true;
}

int CCResourceProvider::createChild(int pool)
{
    DCHECK(CCProxy::isImplThread());
    Child childInfo;
    childInfo.pool = pool;
    int child = m_nextChild++;
    m_children[child] = childInfo;
    return child;
}

void CCResourceProvider::destroyChild(int child)
{
    DCHECK(CCProxy::isImplThread());
    ChildMap::iterator it = m_children.find(child);
    DCHECK(it != m_children.end());
    deleteOwnedResources(it->second.pool);
    m_children.erase(it);
    trimMailboxDeque();
}

const CCResourceProvider::ResourceIdMap& CCResourceProvider::getChildToParentMap(int child) const
{
    DCHECK(CCProxy::isImplThread());
    ChildMap::const_iterator it = m_children.find(child);
    DCHECK(it != m_children.end());
    return it->second.childToParentMap;
}

CCResourceProvider::TransferableResourceList CCResourceProvider::prepareSendToParent(const ResourceIdArray& resources)
{
    DCHECK(CCProxy::isImplThread());
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
            m_resources.find(*it)->second.exported = true;
            list.resources.push_back(resource);
        }
    }
    if (list.resources.size())
        list.syncPoint = context3d->insertSyncPoint();
    return list;
}

CCResourceProvider::TransferableResourceList CCResourceProvider::prepareSendToChild(int child, const ResourceIdArray& resources)
{
    DCHECK(CCProxy::isImplThread());
    TransferableResourceList list;
    list.syncPoint = 0;
    WebGraphicsContext3D* context3d = m_context->context3D();
    if (!context3d || !context3d->makeContextCurrent()) {
        // FIXME: Implement this path for software compositing.
        return list;
    }
    Child& childInfo = m_children.find(child)->second;
    for (ResourceIdArray::const_iterator it = resources.begin(); it != resources.end(); ++it) {
        TransferableResource resource;
        if (!transferResource(context3d, *it, &resource))
            NOTREACHED();
        DCHECK(childInfo.parentToChildMap.find(*it) != childInfo.parentToChildMap.end());
        resource.id = childInfo.parentToChildMap[*it];
        childInfo.parentToChildMap.erase(*it);
        childInfo.childToParentMap.erase(resource.id);
        list.resources.push_back(resource);
        deleteResource(*it);
    }
    if (list.resources.size())
        list.syncPoint = context3d->insertSyncPoint();
    return list;
}

void CCResourceProvider::receiveFromChild(int child, const TransferableResourceList& resources)
{
    DCHECK(CCProxy::isImplThread());
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
    Child& childInfo = m_children.find(child)->second;
    for (TransferableResourceArray::const_iterator it = resources.resources.begin(); it != resources.resources.end(); ++it) {
        unsigned textureId;
        GLC(context3d, textureId = context3d->createTexture());
        GLC(context3d, context3d->bindTexture(GL_TEXTURE_2D, textureId));
        GLC(context3d, context3d->consumeTextureCHROMIUM(GL_TEXTURE_2D, it->mailbox.name));
        ResourceId id = m_nextId++;
        Resource resource(textureId, childInfo.pool, it->size, it->format);
        m_resources[id] = resource;
        m_mailboxes.push_back(it->mailbox);
        childInfo.parentToChildMap[id] = it->id;
        childInfo.childToParentMap[it->id] = id;
    }
}

void CCResourceProvider::receiveFromParent(const TransferableResourceList& resources)
{
    DCHECK(CCProxy::isImplThread());
    WebGraphicsContext3D* context3d = m_context->context3D();
    if (!context3d || !context3d->makeContextCurrent()) {
        // FIXME: Implement this path for software compositing.
        return;
    }
    if (resources.syncPoint)
        GLC(context3d, context3d->waitSyncPoint(resources.syncPoint));
    for (TransferableResourceArray::const_iterator it = resources.resources.begin(); it != resources.resources.end(); ++it) {
        ResourceMap::iterator mapIterator = m_resources.find(it->id);
        DCHECK(mapIterator != m_resources.end());
        Resource* resource = &mapIterator->second;
        DCHECK(resource->exported);
        resource->exported = false;
        GLC(context3d, context3d->bindTexture(GL_TEXTURE_2D, resource->glId));
        GLC(context3d, context3d->consumeTextureCHROMIUM(GL_TEXTURE_2D, it->mailbox.name));
        m_mailboxes.push_back(it->mailbox);
        if (resource->markedForDeletion)
            deleteResourceInternal(mapIterator);
    }
}

bool CCResourceProvider::transferResource(WebGraphicsContext3D* context, ResourceId id, TransferableResource* resource)
{
    DCHECK(CCProxy::isImplThread());
    ResourceMap::const_iterator it = m_resources.find(id);
    CHECK(it != m_resources.end());
    const Resource* source = &it->second;
    DCHECK(!source->lockedForWrite);
    DCHECK(!source->lockForReadCount);
    DCHECK(!source->external);
    if (source->exported)
        return false;
    resource->id = id;
    resource->format = source->format;
    resource->size = source->size;
    if (m_mailboxes.size()) {
        resource->mailbox = m_mailboxes.front();
        m_mailboxes.pop_front();
    }
    else
        GLC(context, context->genMailboxCHROMIUM(resource->mailbox.name));
    GLC(context, context->bindTexture(GL_TEXTURE_2D, source->glId));
    GLC(context, context->produceTextureCHROMIUM(GL_TEXTURE_2D, resource->mailbox.name));
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
            if (!it->second.exported && !it->second.external)
                ++maxMailboxCount;
        }
    } else {
        base::hash_set<int> childPoolSet;
        for (ChildMap::iterator it = m_children.begin(); it != m_children.end(); ++it)
            childPoolSet.insert(it->second.pool);
        for (ResourceMap::iterator it = m_resources.begin(); it != m_resources.end(); ++it) {
            if (ContainsKey(childPoolSet, it->second.pool))
                ++maxMailboxCount;
        }
    }
    while (m_mailboxes.size() > maxMailboxCount)
        m_mailboxes.pop_front();
}

}
