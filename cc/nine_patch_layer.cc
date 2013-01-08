// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/nine_patch_layer.h"

#include "cc/layer_tree_host.h"
#include "cc/nine_patch_layer_impl.h"
#include "cc/prioritized_resource.h"
#include "cc/resource_update.h"
#include "cc/resource_update_queue.h"

namespace cc {

scoped_refptr<NinePatchLayer> NinePatchLayer::create()
{
    return make_scoped_refptr(new NinePatchLayer());
}

NinePatchLayer::NinePatchLayer()
    : m_bitmapDirty(false)
{
}

NinePatchLayer::~NinePatchLayer()
{
}

scoped_ptr<LayerImpl> NinePatchLayer::createLayerImpl(LayerTreeImpl* treeImpl)
{
    return NinePatchLayerImpl::create(treeImpl, id()).PassAs<LayerImpl>();
}

void NinePatchLayer::setTexturePriorities(const PriorityCalculator& priorityCalc)
{
    if (m_resource && !m_resource->texture()->resourceManager()) {
        // Release the resource here, as it is no longer tied to a resource manager.
        m_resource.reset();
        if (!m_bitmap.isNull())
            createResource();
    } else if (m_needsDisplay && m_bitmapDirty && drawsContent()) {
        createResource();
    }

    if (m_resource) {
        m_resource->texture()->setRequestPriority(PriorityCalculator::uiPriority(true));
        // FIXME: Need to support swizzle in the shader for !PlatformColor::sameComponentOrder(textureFormat)
        GLenum textureFormat = layerTreeHost()->rendererCapabilities().bestTextureFormat;
        m_resource->texture()->setDimensions(gfx::Size(m_bitmap.width(), m_bitmap.height()), textureFormat);
    }
}

void NinePatchLayer::setBitmap(const SkBitmap& bitmap, const gfx::Rect& aperture) {
    m_bitmap = bitmap;
    m_imageAperture = aperture;
    m_bitmapDirty = true;
    setNeedsDisplay();
}

void NinePatchLayer::update(ResourceUpdateQueue& queue, const OcclusionTracker* occlusion, RenderingStats& stats)
{
    createUpdaterIfNeeded();

    if (m_resource && (m_bitmapDirty || m_resource->texture()->resourceId() == 0)) {
        gfx::Rect contentRect(gfx::Point(), gfx::Size(m_bitmap.width(), m_bitmap.height()));
        ResourceUpdate upload = ResourceUpdate::Create(m_resource->texture(), &m_bitmap, contentRect, contentRect, gfx::Vector2d());
        queue.appendFullUpload(upload);
        m_bitmapDirty = false;
    }
}

void NinePatchLayer::createUpdaterIfNeeded()
{
    if (m_updater)
        return;

    m_updater = ImageLayerUpdater::create();
}

void NinePatchLayer::createResource()
{
    DCHECK(!m_bitmap.isNull());
    createUpdaterIfNeeded();
    m_updater->setBitmap(m_bitmap);
    m_needsDisplay = false;

    if (!m_resource)
        m_resource = m_updater->createResource(layerTreeHost()->contentsTextureManager());
}

bool NinePatchLayer::drawsContent() const
{
    bool draws = !m_bitmap.isNull() && Layer::drawsContent() && m_bitmap.width() && m_bitmap.height();
    return draws;
}

void NinePatchLayer::pushPropertiesTo(LayerImpl* layer)
{
    Layer::pushPropertiesTo(layer);
    NinePatchLayerImpl* layerImpl = static_cast<NinePatchLayerImpl*>(layer);

    if (m_resource) {
        DCHECK(!m_bitmap.isNull());
        layerImpl->setResourceId(m_resource->texture()->resourceId());
        layerImpl->setLayout(gfx::Size(m_bitmap.width(), m_bitmap.height()), m_imageAperture);
    }
}

}
