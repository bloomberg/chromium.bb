// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "cc/picture_layer.h"
#include "cc/picture_layer_impl.h"

namespace cc {

scoped_refptr<PictureLayer> PictureLayer::create(ContentLayerClient* client) {
  return make_scoped_refptr(new PictureLayer(client));
}

PictureLayer::PictureLayer(ContentLayerClient* client) :
  client_(client) {
}

PictureLayer::~PictureLayer() {
}

bool PictureLayer::drawsContent() const {
  return Layer::drawsContent() && client_;
}

scoped_ptr<LayerImpl> PictureLayer::createLayerImpl() {
  return PictureLayerImpl::create(id()).PassAs<LayerImpl>();
}

void PictureLayer::pushPropertiesTo(LayerImpl* baseLayerImpl) {
  PictureLayerImpl* layerImpl = static_cast<PictureLayerImpl*>(baseLayerImpl);
  pile_.pushPropertiesTo(layerImpl->pile_);
}

}  // namespace cc
