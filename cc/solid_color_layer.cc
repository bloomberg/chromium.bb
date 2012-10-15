// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "SolidColorLayerChromium.h"

#include "CCSolidColorLayerImpl.h"

namespace cc {

scoped_ptr<CCLayerImpl> SolidColorLayerChromium::createCCLayerImpl()
{
    return CCSolidColorLayerImpl::create(id()).PassAs<CCLayerImpl>();
}

scoped_refptr<SolidColorLayerChromium> SolidColorLayerChromium::create()
{
    return make_scoped_refptr(new SolidColorLayerChromium());
}

SolidColorLayerChromium::SolidColorLayerChromium()
    : LayerChromium()
{
}

SolidColorLayerChromium::~SolidColorLayerChromium()
{
}

} // namespace cc
