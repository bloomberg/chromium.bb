// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "DelegatedRendererLayerChromium.h"

#include "CCDelegatedRendererLayerImpl.h"

namespace cc {

PassRefPtr<DelegatedRendererLayerChromium> DelegatedRendererLayerChromium::create()
{
    return adoptRef(new DelegatedRendererLayerChromium());
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

PassOwnPtr<CCLayerImpl> DelegatedRendererLayerChromium::createCCLayerImpl()
{
    return CCDelegatedRendererLayerImpl::create(m_layerId);
}

}
