// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/io_surface_layer.h"

#include "cc/io_surface_layer_impl.h"

namespace cc {

scoped_refptr<IOSurfaceLayer> IOSurfaceLayer::Create()
{
    return make_scoped_refptr(new IOSurfaceLayer());
}

IOSurfaceLayer::IOSurfaceLayer()
    : Layer()
    , m_ioSurfaceId(0)
{
}

IOSurfaceLayer::~IOSurfaceLayer()
{
}

void IOSurfaceLayer::setIOSurfaceProperties(uint32_t ioSurfaceId, const gfx::Size& size)
{
    m_ioSurfaceId = ioSurfaceId;
    m_ioSurfaceSize = size;
    SetNeedsCommit();
}

scoped_ptr<LayerImpl> IOSurfaceLayer::CreateLayerImpl(LayerTreeImpl* treeImpl)
{
    return IOSurfaceLayerImpl::Create(treeImpl, layer_id_).PassAs<LayerImpl>();
}

bool IOSurfaceLayer::DrawsContent() const
{
    return m_ioSurfaceId && Layer::DrawsContent();
}

void IOSurfaceLayer::PushPropertiesTo(LayerImpl* layer)
{
    Layer::PushPropertiesTo(layer);

    IOSurfaceLayerImpl* ioSurfaceLayer = static_cast<IOSurfaceLayerImpl*>(layer);
    ioSurfaceLayer->setIOSurfaceProperties(m_ioSurfaceId, m_ioSurfaceSize);
}

}  // namespace cc
