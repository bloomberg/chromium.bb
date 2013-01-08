// Copyright 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/image_layer.h"

#include "base/compiler_specific.h"
#include "cc/image_layer_updater.h"
#include "cc/layer_updater.h"
#include "cc/layer_tree_host.h"
#include "cc/prioritized_resource.h"
#include "cc/resource_update_queue.h"

namespace cc {

scoped_refptr<ImageLayer> ImageLayer::create()
{
    return make_scoped_refptr(new ImageLayer());
}

ImageLayer::ImageLayer()
    : TiledLayer()
{
}

ImageLayer::~ImageLayer()
{
}

void ImageLayer::setBitmap(const SkBitmap& bitmap)
{
    // setBitmap() currently gets called whenever there is any
    // style change that affects the layer even if that change doesn't
    // affect the actual contents of the image (e.g. a CSS animation).
    // With this check in place we avoid unecessary texture uploads.
    if (bitmap.pixelRef() && bitmap.pixelRef() == m_bitmap.pixelRef())
        return;

    m_bitmap = bitmap;
    setNeedsDisplay();
}

void ImageLayer::setTexturePriorities(const PriorityCalculator& priorityCalc)
{
    // Update the tile data before creating all the layer's tiles.
    updateTileSizeAndTilingOption();

    TiledLayer::setTexturePriorities(priorityCalc);
}

void ImageLayer::update(ResourceUpdateQueue& queue, const OcclusionTracker* occlusion, RenderingStats& stats)
{
    createUpdaterIfNeeded();
    if (m_needsDisplay) {
        m_updater->setBitmap(m_bitmap);
        updateTileSizeAndTilingOption();
        invalidateContentRect(gfx::Rect(gfx::Point(), contentBounds()));
        m_needsDisplay = false;
    }
    TiledLayer::update(queue, occlusion, stats);
}

void ImageLayer::createUpdaterIfNeeded()
{
    if (m_updater)
        return;

    m_updater = ImageLayerUpdater::create();
    GLenum textureFormat = layerTreeHost()->rendererCapabilities().bestTextureFormat;
    setTextureFormat(textureFormat);
}

LayerUpdater* ImageLayer::updater() const
{
    return m_updater.get();
}

void ImageLayer::calculateContentsScale(
    float ideal_contents_scale,
    float* contentsScaleX,
    float* contentsScaleY,
    gfx::Size* contentBounds)
{
    *contentsScaleX = imageContentsScaleX();
    *contentsScaleY = imageContentsScaleY();
    *contentBounds = gfx::Size(m_bitmap.width(), m_bitmap.height());
}

bool ImageLayer::drawsContent() const
{
    return !m_bitmap.isNull() && TiledLayer::drawsContent();
}

float ImageLayer::imageContentsScaleX() const
{
    if (bounds().IsEmpty() || m_bitmap.width() == 0)
        return 1;
    return static_cast<float>(m_bitmap.width()) / bounds().width();
}

float ImageLayer::imageContentsScaleY() const
{
    if (bounds().IsEmpty() || m_bitmap.height() == 0)
        return 1;
    return static_cast<float>(m_bitmap.height()) / bounds().height();
}

}  // namespace cc
