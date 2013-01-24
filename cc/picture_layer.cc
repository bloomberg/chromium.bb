// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/picture_layer.h"

#include "cc/layer_tree_impl.h"
#include "cc/picture_layer_impl.h"
#include "ui/gfx/rect_conversions.h"

namespace cc {

scoped_refptr<PictureLayer> PictureLayer::create(ContentLayerClient* client) {
  return make_scoped_refptr(new PictureLayer(client));
}

PictureLayer::PictureLayer(ContentLayerClient* client) :
  client_(client),
  pile_(make_scoped_refptr(new PicturePile())),
  is_mask_(false) {
}

PictureLayer::~PictureLayer() {
}

bool PictureLayer::drawsContent() const {
  return Layer::drawsContent() && client_;
}

scoped_ptr<LayerImpl> PictureLayer::createLayerImpl(LayerTreeImpl* treeImpl) {
  return PictureLayerImpl::create(treeImpl, id()).PassAs<LayerImpl>();
}

void PictureLayer::pushPropertiesTo(LayerImpl* base_layer) {
  Layer::pushPropertiesTo(base_layer);

  PictureLayerImpl* layer_impl = static_cast<PictureLayerImpl*>(base_layer);
  layer_impl->SetIsMask(is_mask_);
  layer_impl->CreateTilingSet();
  layer_impl->invalidation_.Clear();
  layer_impl->invalidation_.Swap(pile_invalidation_);
  pile_->PushPropertiesTo(layer_impl->pile_);

  layer_impl->SyncFromActiveLayer();
}

void PictureLayer::setLayerTreeHost(LayerTreeHost* host) {
  Layer::setLayerTreeHost(host);
  if (host)
      pile_->SetMinContentsScale(host->settings().minimumContentsScale);
}

void PictureLayer::setNeedsDisplayRect(const gfx::RectF& layer_rect) {
  gfx::Rect rect = gfx::ToEnclosedRect(layer_rect);
  if (!rect.IsEmpty()) {
    // Clamp invalidation to the layer bounds.
    rect.Intersect(gfx::Rect(bounds()));
    pending_invalidation_.Union(rect);
  }
  Layer::setNeedsDisplayRect(layer_rect);
}

void PictureLayer::update(ResourceUpdateQueue&, const OcclusionTracker*,
                    RenderingStats& stats) {
  // Do not early-out of this function so that PicturePile::Update has a chance
  // to record pictures due to changing visibility of this layer.

  pile_->Resize(bounds());

  // Calling paint in WebKit can sometimes cause invalidations, so save
  // off the invalidation prior to calling update.
  pile_invalidation_.Swap(pending_invalidation_);
  pending_invalidation_.Clear();

  gfx::Rect visible_layer_rect = gfx::ToEnclosingRect(
      gfx::ScaleRect(visibleContentRect(), 1.f / contentsScaleX()));
  pile_->Update(client_, pile_invalidation_, visible_layer_rect, stats);
}

void PictureLayer::setIsMask(bool is_mask) {
  is_mask_ = is_mask;
}

}  // namespace cc
