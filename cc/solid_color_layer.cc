// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "cc/solid_color_layer.h"

#include "cc/solid_color_layer_impl.h"

namespace cc {

scoped_ptr<LayerImpl> SolidColorLayer::createLayerImpl()
{
    return SolidColorLayerImpl::create(id()).PassAs<LayerImpl>();
}

scoped_refptr<SolidColorLayer> SolidColorLayer::create()
{
    return make_scoped_refptr(new SolidColorLayer());
}

SolidColorLayer::SolidColorLayer()
    : Layer()
{
}

SolidColorLayer::~SolidColorLayer()
{
}

} // namespace cc
