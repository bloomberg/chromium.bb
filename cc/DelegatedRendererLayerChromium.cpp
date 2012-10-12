// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "DelegatedRendererLayerChromium.h"

#include "CCDelegatedRendererLayerImpl.h"

namespace cc {

scoped_refptr<DelegatedRendererLayerChromium> DelegatedRendererLayerChromium::create()
{
    return scoped_refptr<DelegatedRendererLayerChromium>(new DelegatedRendererLayerChromium());
}

DelegatedRendererLayerChromium::DelegatedRendererLayerChromium()
    : LayerChromium()
{
    setIsDrawable(true);
    setMasksToBounds(true);
}

DelegatedRendererLayerChromium::~DelegatedRendererLayerChromium()
{
}

scoped_ptr<CCLayerImpl> DelegatedRendererLayerChromium::createCCLayerImpl()
{
    return CCDelegatedRendererLayerImpl::create(m_layerId).PassAs<CCLayerImpl>();
}

}
