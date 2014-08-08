// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/tiled_layer_test_common.h"

namespace cc {

FakeLayerUpdater::Resource::Resource(FakeLayerUpdater* layer,
                                     scoped_ptr<PrioritizedResource> texture)
    : LayerUpdater::Resource(texture.Pass()), layer_(layer) {
  bitmap_.allocN32Pixels(10, 10);
}

FakeLayerUpdater::Resource::~Resource() {}

void FakeLayerUpdater::Resource::Update(ResourceUpdateQueue* queue,
                                        const gfx::Rect& source_rect,
                                        const gfx::Vector2d& dest_offset,
                                        bool partial_update) {
  const gfx::Rect kRect(0, 0, 10, 10);
  ResourceUpdate upload = ResourceUpdate::Create(
      texture(), &bitmap_, kRect, kRect, gfx::Vector2d());
  if (partial_update)
    queue->AppendPartialUpload(upload);
  else
    queue->AppendFullUpload(upload);

  layer_->Update();
}

FakeLayerUpdater::FakeLayerUpdater() : prepare_count_(0), update_count_(0) {}

FakeLayerUpdater::~FakeLayerUpdater() {}

void FakeLayerUpdater::PrepareToUpdate(const gfx::Rect& content_rect,
                                       const gfx::Size& tile_size,
                                       float contents_width_scale,
                                       float contents_height_scale,
                                       gfx::Rect* resulting_opaque_rect) {
  prepare_count_++;
  last_update_rect_ = content_rect;
  if (!rect_to_invalidate_.IsEmpty()) {
    layer_->InvalidateContentRect(rect_to_invalidate_);
    rect_to_invalidate_ = gfx::Rect();
    layer_ = NULL;
  }
  *resulting_opaque_rect = opaque_paint_rect_;
}

void FakeLayerUpdater::SetRectToInvalidate(const gfx::Rect& rect,
                                           FakeTiledLayer* layer) {
  rect_to_invalidate_ = rect;
  layer_ = layer;
}

scoped_ptr<LayerUpdater::Resource> FakeLayerUpdater::CreateResource(
    PrioritizedResourceManager* manager) {
  return scoped_ptr<LayerUpdater::Resource>(
      new Resource(this, PrioritizedResource::Create(manager)));
}

FakeTiledLayerImpl::FakeTiledLayerImpl(LayerTreeImpl* tree_impl, int id)
    : TiledLayerImpl(tree_impl, id) {}

FakeTiledLayerImpl::~FakeTiledLayerImpl() {}

FakeTiledLayer::FakeTiledLayer(PrioritizedResourceManager* resource_manager)
    : TiledLayer(),
      fake_updater_(make_scoped_refptr(new FakeLayerUpdater)),
      resource_manager_(resource_manager) {
  SetTileSize(tile_size());
  SetTextureFormat(RGBA_8888);
  SetBorderTexelOption(LayerTilingData::NO_BORDER_TEXELS);
  // So that we don't get false positives if any of these
  // tests expect to return false from DrawsContent() for other reasons.
  SetIsDrawable(true);
}

FakeTiledLayerWithScaledBounds::FakeTiledLayerWithScaledBounds(
    PrioritizedResourceManager* resource_manager)
    : FakeTiledLayer(resource_manager) {}

FakeTiledLayerWithScaledBounds::~FakeTiledLayerWithScaledBounds() {}

FakeTiledLayer::~FakeTiledLayer() {}

void FakeTiledLayer::SetNeedsDisplayRect(const gfx::RectF& rect) {
  last_needs_display_rect_ = rect;
  TiledLayer::SetNeedsDisplayRect(rect);
}

void FakeTiledLayer::SetTexturePriorities(
    const PriorityCalculator& calculator) {
  // Ensure there is always a target render surface available. If none has been
  // set (the layer is an orphan for the test), then just set a surface on
  // itself.
  bool missing_target_render_surface = !render_target();

  if (missing_target_render_surface)
    CreateRenderSurface();

  TiledLayer::SetTexturePriorities(calculator);

  if (missing_target_render_surface) {
    ClearRenderSurface();
    draw_properties().render_target = 0;
  }
}

PrioritizedResourceManager* FakeTiledLayer::ResourceManager() {
  return resource_manager_;
}

void FakeTiledLayer::UpdateContentsScale(float ideal_contents_scale) {
  CalculateContentsScale(ideal_contents_scale,
                         &draw_properties().contents_scale_x,
                         &draw_properties().contents_scale_y,
                         &draw_properties().content_bounds);
}

void FakeTiledLayer::ResetNumDependentsNeedPushProperties() {
  size_t num = 0;
  if (mask_layer()) {
    if (mask_layer()->needs_push_properties() ||
        mask_layer()->descendant_needs_push_properties())
      ++num;
  }
  if (replica_layer()) {
    if (replica_layer()->needs_push_properties() ||
        replica_layer()->descendant_needs_push_properties())
      ++num;
  }
  for (size_t i = 0; i < children().size(); ++i) {
    if (children()[i]->needs_push_properties() ||
        children()[i]->descendant_needs_push_properties())
      ++num;
  }
  num_dependents_need_push_properties_ = num;
}

LayerUpdater* FakeTiledLayer::Updater() const {
  return fake_updater_.get();
}

void FakeTiledLayerWithScaledBounds::SetContentBounds(
    const gfx::Size& content_bounds) {
  forced_content_bounds_ = content_bounds;
  draw_properties().content_bounds = forced_content_bounds_;
}

void FakeTiledLayerWithScaledBounds::CalculateContentsScale(
    float ideal_contents_scale,
    float* contents_scale_x,
    float* contents_scale_y,
    gfx::Size* content_bounds) {
  *contents_scale_x =
      static_cast<float>(forced_content_bounds_.width()) / bounds().width();
  *contents_scale_y =
      static_cast<float>(forced_content_bounds_.height()) / bounds().height();
  *content_bounds = forced_content_bounds_;
}

}  // namespace cc
