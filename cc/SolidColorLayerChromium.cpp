// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#if USE(ACCELERATED_COMPOSITING)

#include "SolidColorLayerChromium.h"

#include "CCSolidColorLayerImpl.h"

namespace cc {

PassOwnPtr<CCLayerImpl> SolidColorLayerChromium::createCCLayerImpl()
{
    return CCSolidColorLayerImpl::create(id());
}

PassRefPtr<SolidColorLayerChromium> SolidColorLayerChromium::create()
{
    return adoptRef(new SolidColorLayerChromium());
}

SolidColorLayerChromium::SolidColorLayerChromium()
    : LayerChromium()
{
}

SolidColorLayerChromium::~SolidColorLayerChromium()
{
}

} // namespace cc

#endif // USE(ACCELERATED_COMPOSITING)
