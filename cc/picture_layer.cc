// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/picture_layer.h"
#include "cc/picture_layer_impl.h"
#include "ui/gfx/rect_conversions.h"

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

void PictureLayer::pushPropertiesTo(LayerImpl* base_layer) {
  PictureLayerImpl* layer_impl = static_cast<PictureLayerImpl*>(base_layer);
  pile_.PushPropertiesTo(layer_impl->pile_);

  // TODO(enne): Need to sync tiling from active tree prior to this?
  // TODO(nduca): Need to invalidate tiles here from pile's invalidation info.
}

void PictureLayer::setNeedsDisplayRect(const gfx::RectF& layer_rect) {
  gfx::Rect rect = gfx::ToEnclosedRect(layer_rect);
  pile_.Invalidate(rect);
}

void PictureLayer::update(ResourceUpdateQueue&, const OcclusionTracker*,
                    RenderingStats& stats) {
  pile_.Resize(bounds());
  pile_.Update(client_, stats);
}

}  // namespace cc
