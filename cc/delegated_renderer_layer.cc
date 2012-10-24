// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "cc/delegated_renderer_layer.h"

#include "cc/delegated_renderer_layer_impl.h"

namespace cc {

scoped_refptr<DelegatedRendererLayer> DelegatedRendererLayer::create()
{
    return scoped_refptr<DelegatedRendererLayer>(new DelegatedRendererLayer());
}

DelegatedRendererLayer::DelegatedRendererLayer()
    : Layer()
{
    setIsDrawable(true);
    setMasksToBounds(true);
}

DelegatedRendererLayer::~DelegatedRendererLayer()
{
}

scoped_ptr<LayerImpl> DelegatedRendererLayer::createLayerImpl()
{
    return DelegatedRendererLayerImpl::create(m_layerId).PassAs<LayerImpl>();
}

}  // namespace cc
