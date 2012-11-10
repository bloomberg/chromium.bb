// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/picture_layer_impl.h"

namespace cc {

PictureLayerImpl::PictureLayerImpl(int id) :
    LayerImpl(id) {
}

PictureLayerImpl::~PictureLayerImpl() {
}

const char* PictureLayerImpl::layerTypeAsString() const {
  return "PictureLayer";
}

void PictureLayerImpl::appendQuads(QuadSink&, AppendQuadsData&) {
  // TODO(enne): implement me
}

void PictureLayerImpl::dumpLayerProperties(std::string*, int indent) const {
  // TODO(enne): implement me
}


}  // namespace cc
