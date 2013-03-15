// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/nine_patch_layer.h"

#include "cc/layer_tree_host.h"
#include "cc/nine_patch_layer_impl.h"
#include "cc/prioritized_resource.h"
#include "cc/resource_update.h"
#include "cc/resource_update_queue.h"

namespace cc {

scoped_refptr<NinePatchLayer> NinePatchLayer::Create() {
  return make_scoped_refptr(new NinePatchLayer());
}

NinePatchLayer::NinePatchLayer()
    : bitmap_dirty_(false) {}

NinePatchLayer::~NinePatchLayer() {}

scoped_ptr<LayerImpl> NinePatchLayer::CreateLayerImpl(
    LayerTreeImpl* tree_impl) {
  return NinePatchLayerImpl::Create(tree_impl, id()).PassAs<LayerImpl>();
}

void NinePatchLayer::SetTexturePriorities(
    const PriorityCalculator& priority_calc) {
  if (resource_ && !resource_->texture()->resourceManager()) {
    // Release the resource here, as it is no longer tied to a resource manager.
    resource_.reset();
    if (!bitmap_.isNull())
      CreateResource();
  } else if (needs_display_ && bitmap_dirty_ && DrawsContent()) {
    CreateResource();
  }

  if (resource_) {
    resource_->texture()->setRequestPriority(
        PriorityCalculator::uiPriority(true));
    // FIXME: Need to support swizzle in the shader for
    // !PlatformColor::sameComponentOrder(texture_format)
    GLenum texture_format =
        layer_tree_host()->GetRendererCapabilities().best_texture_format;
    resource_->texture()->setDimensions(
        gfx::Size(bitmap_.width(), bitmap_.height()), texture_format);
  }
}

void NinePatchLayer::SetBitmap(const SkBitmap& bitmap, gfx::Rect aperture) {
  bitmap_ = bitmap;
  image_aperture_ = aperture;
  bitmap_dirty_ = true;
  SetNeedsDisplay();
}

void NinePatchLayer::Update(ResourceUpdateQueue* queue,
                            const OcclusionTracker* occlusion,
                            RenderingStats* stats) {
  CreateUpdaterIfNeeded();

  if (resource_ && (bitmap_dirty_ || resource_->texture()->resourceId() == 0)) {
    gfx::Rect contentRect(gfx::Point(),
                          gfx::Size(bitmap_.width(), bitmap_.height()));
    ResourceUpdate upload = ResourceUpdate::Create(resource_->texture(),
                                                   &bitmap_,
                                                   contentRect,
                                                   contentRect,
                                                   gfx::Vector2d());
    queue->appendFullUpload(upload);
    bitmap_dirty_ = false;
  }
}

void NinePatchLayer::CreateUpdaterIfNeeded() {
  if (updater_)
    return;

  updater_ = ImageLayerUpdater::create();
}

void NinePatchLayer::CreateResource() {
  DCHECK(!bitmap_.isNull());
  CreateUpdaterIfNeeded();
  updater_->setBitmap(bitmap_);
  needs_display_ = false;

  if (!resource_) {
    resource_ = updater_->CreateResource(
        layer_tree_host()->contents_texture_manager());
  }
}

bool NinePatchLayer::DrawsContent() const {
  bool draws = !bitmap_.isNull() &&
               Layer::DrawsContent() &&
               bitmap_.width() &&
               bitmap_.height();
  return draws;
}

void NinePatchLayer::PushPropertiesTo(LayerImpl* layer) {
  Layer::PushPropertiesTo(layer);
  NinePatchLayerImpl* layer_impl = static_cast<NinePatchLayerImpl*>(layer);

  if (resource_) {
    DCHECK(!bitmap_.isNull());
    layer_impl->SetResourceId(resource_->texture()->resourceId());
    layer_impl->SetLayout(
        gfx::Size(bitmap_.width(), bitmap_.height()), image_aperture_);
  }
}

}
