// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/compositor/layer/contextual_search_layer.h"

#include "cc/layers/layer.h"
#include "cc/layers/nine_patch_layer.h"
#include "cc/layers/solid_color_layer.h"
#include "cc/layers/ui_resource_layer.h"
#include "content/public/browser/android/content_view_core.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/android/resources/resource_manager.h"
#include "ui/android/resources/ui_resource_android.h"

namespace {

const SkColor kContextualSearchBarBorderColor = SkColorSetRGB(0xf1, 0xf1, 0xf1);

}  // namespace

namespace chrome {
namespace android {

// static
scoped_refptr<ContextualSearchLayer> ContextualSearchLayer::Create(
    ui::ResourceManager* resource_manager) {
  return make_scoped_refptr(new ContextualSearchLayer(resource_manager));
}

void ContextualSearchLayer::SetProperties(
    int search_bar_background_resource_id,
    int search_bar_text_resource_id,
    int search_provider_icon_resource_id,
    int search_icon_resource_id,
    int progress_bar_background_resource_id,
    int progress_bar_resource_id,
    content::ContentViewCore* content_view_core,
    float search_panel_y,
    float search_panel_width,
    float search_bar_margin_top,
    float search_bar_height,
    float search_bar_text_opacity,
    bool search_bar_border_visible,
    float search_bar_border_y,
    float search_bar_border_height,
    float search_provider_icon_opacity,
    float search_icon_padding_left,
    float search_icon_opacity,
    bool progress_bar_visible,
    float progress_bar_y,
    float progress_bar_height,
    float progress_bar_opacity,
    int progress_bar_completion) {
  // Grab the dynamic Search Bar Text resource.
  ui::ResourceManager::Resource* search_bar_text_resource =
      resource_manager_->GetResource(ui::ANDROID_RESOURCE_TYPE_DYNAMIC,
                                     search_bar_text_resource_id);

  // Grab required static resources.
  ui::ResourceManager::Resource* search_bar_background_resource =
      resource_manager_->GetResource(ui::ANDROID_RESOURCE_TYPE_STATIC,
                                     search_bar_background_resource_id);
  ui::ResourceManager::Resource* search_provider_icon_resource =
      resource_manager_->GetResource(ui::ANDROID_RESOURCE_TYPE_STATIC,
                                     search_provider_icon_resource_id);
  ui::ResourceManager::Resource* search_icon_resource =
      resource_manager_->GetResource(ui::ANDROID_RESOURCE_TYPE_STATIC,
                                     search_icon_resource_id);

  DCHECK(search_bar_background_resource);
  DCHECK(search_provider_icon_resource);
  DCHECK(search_icon_resource);

  // Round values to avoid pixel gap between layers.
  search_bar_height = floor(search_bar_height);
  search_bar_margin_top = floor(search_bar_margin_top);

  // ---------------------------------------------------------------------------
  // Search Bar Background
  // ---------------------------------------------------------------------------
  gfx::Size background_size(search_panel_width, search_bar_height);
  search_bar_background_->SetUIResourceId(
      search_bar_background_resource->ui_resource->id());
  search_bar_background_->SetBorder(
      search_bar_background_resource->Border(background_size));
  search_bar_background_->SetAperture(search_bar_background_resource->aperture);
  search_bar_background_->SetBounds(background_size);

  // ---------------------------------------------------------------------------
  // Search Bar Text
  // ---------------------------------------------------------------------------
  if (search_bar_text_resource) {
    // Centralizes the text vertically in the Search Bar.
    float search_bar_padding_top =
        search_bar_margin_top +
        (search_bar_height - search_bar_margin_top) / 2 -
        search_bar_text_resource->size.height() / 2;
    search_bar_text_->SetUIResourceId(
        search_bar_text_resource->ui_resource->id());
    search_bar_text_->SetBounds(search_bar_text_resource->size);
    search_bar_text_->SetPosition(gfx::PointF(0.f, search_bar_padding_top));
    search_bar_text_->SetOpacity(search_bar_text_opacity);
  }

  // ---------------------------------------------------------------------------
  // Search Provider Icon
  // ---------------------------------------------------------------------------
  // Centralizes the Search Provider Icon horizontally in the Search Bar.
  float search_provider_icon_left =
      search_panel_width / 2.f -
      search_provider_icon_resource->size.width() / 2.f;
  search_provider_icon_->SetUIResourceId(
      search_provider_icon_resource->ui_resource->id());
  search_provider_icon_->SetBounds(search_provider_icon_resource->size);
  search_provider_icon_->SetPosition(
      gfx::PointF(search_provider_icon_left, 0.f));
  search_provider_icon_->SetOpacity(search_provider_icon_opacity);

  // ---------------------------------------------------------------------------
  // Search Icon
  // ---------------------------------------------------------------------------
  // Centralizes the Search Icon vertically in the Search Bar.
  float search_icon_padding_top =
      search_bar_margin_top + (search_bar_height - search_bar_margin_top) / 2 -
      search_icon_resource->size.height() / 2;
  search_icon_->SetUIResourceId(search_icon_resource->ui_resource->id());
  search_icon_->SetBounds(search_icon_resource->size);
  search_icon_->SetPosition(
      gfx::PointF(search_icon_padding_left, search_icon_padding_top));
  search_icon_->SetOpacity(search_icon_opacity);

  // ---------------------------------------------------------------------------
  // Search Content View
  // ---------------------------------------------------------------------------
  content_view_container_->SetPosition(gfx::PointF(0.f, search_bar_height));
  if (content_view_core && content_view_core->GetLayer().get()) {
    scoped_refptr<cc::Layer> content_view_layer = content_view_core->GetLayer();
    if (content_view_layer->parent() != content_view_container_)
      content_view_container_->AddChild(content_view_layer);
  } else {
    content_view_container_->RemoveAllChildren();
  }

  // ---------------------------------------------------------------------------
  // Search Panel.
  // ---------------------------------------------------------------------------
  layer_->SetPosition(gfx::PointF(0.f, search_panel_y));

  // ---------------------------------------------------------------------------
  // Progress Bar
  // ---------------------------------------------------------------------------
  bool should_render_progress_bar =
      progress_bar_visible && progress_bar_opacity > 0.f;
  if (should_render_progress_bar) {
    // Load Progress Bar resources.
    ui::ResourceManager::Resource* progress_bar_background_resource =
        resource_manager_->GetResource(ui::ANDROID_RESOURCE_TYPE_STATIC,
                                       progress_bar_background_resource_id);
    ui::ResourceManager::Resource* progress_bar_resource =
        resource_manager_->GetResource(ui::ANDROID_RESOURCE_TYPE_STATIC,
                                       progress_bar_resource_id);

    DCHECK(progress_bar_background_resource);
    DCHECK(progress_bar_resource);

    // Progress Bar Background
    if (progress_bar_background_->parent() != layer_)
      layer_->AddChild(progress_bar_background_);

    gfx::Size progress_bar_background_size(search_panel_width,
                                           progress_bar_height);
    progress_bar_background_->SetUIResourceId(
        progress_bar_background_resource->ui_resource->id());
    progress_bar_background_->SetBorder(
        progress_bar_background_resource->Border(progress_bar_background_size));
    progress_bar_background_->SetAperture(
        progress_bar_background_resource->aperture);
    progress_bar_background_->SetBounds(progress_bar_background_size);
    progress_bar_background_->SetPosition(gfx::PointF(0.f, progress_bar_y));
    progress_bar_background_->SetOpacity(progress_bar_opacity);

    // Progress Bar
    if (progress_bar_->parent() != layer_)
      layer_->AddChild(progress_bar_);

    float progress_bar_width =
        floor(search_panel_width * progress_bar_completion / 100.f);
    gfx::Size progress_bar_size(progress_bar_width, progress_bar_height);
    progress_bar_->SetUIResourceId(progress_bar_resource->ui_resource->id());
    progress_bar_->SetBorder(progress_bar_resource->Border(progress_bar_size));
    progress_bar_->SetAperture(progress_bar_resource->aperture);
    progress_bar_->SetBounds(progress_bar_size);
    progress_bar_->SetPosition(gfx::PointF(0.f, progress_bar_y));
    progress_bar_->SetOpacity(progress_bar_opacity);
  } else {
    // Removes Progress Bar and its Background from the Layer Tree.
    if (progress_bar_background_.get() && progress_bar_background_->parent())
      progress_bar_background_->RemoveFromParent();

    if (progress_bar_.get() && progress_bar_->parent())
      progress_bar_->RemoveFromParent();
  }

  // ---------------------------------------------------------------------------
  // Search Bar border.
  // ---------------------------------------------------------------------------
  if (!should_render_progress_bar && search_bar_border_visible) {
    gfx::Size search_bar_border_size(search_panel_width,
                                     search_bar_border_height);
    search_bar_border_->SetBounds(search_bar_border_size);
    search_bar_border_->SetPosition(gfx::PointF(0.f, search_bar_border_y));
    layer_->AddChild(search_bar_border_);
  } else if (search_bar_border_.get() && search_bar_border_->parent()) {
    search_bar_border_->RemoveFromParent();
  }
}

ContextualSearchLayer::ContextualSearchLayer(
    ui::ResourceManager* resource_manager)
    : resource_manager_(resource_manager),
      layer_(cc::Layer::Create()),
      search_bar_background_(cc::NinePatchLayer::Create()),
      search_bar_text_(cc::UIResourceLayer::Create()),
      search_provider_icon_(cc::UIResourceLayer::Create()),
      search_icon_(cc::UIResourceLayer::Create()),
      content_view_container_(cc::Layer::Create()),
      search_bar_border_(cc::SolidColorLayer::Create()),
      progress_bar_(cc::NinePatchLayer::Create()),
      progress_bar_background_(cc::NinePatchLayer::Create()) {
  layer_->SetMasksToBounds(false);
  layer_->SetIsDrawable(true);

  // Search Bar Background
  search_bar_background_->SetIsDrawable(true);
  search_bar_background_->SetFillCenter(true);
  layer_->AddChild(search_bar_background_);

  // Search Bar Text
  search_bar_text_->SetIsDrawable(true);
  layer_->AddChild(search_bar_text_);

  // Search Provider Icon
  search_provider_icon_->SetIsDrawable(true);
  layer_->AddChild(search_provider_icon_);

  // Search Icon
  search_icon_->SetIsDrawable(true);
  layer_->AddChild(search_icon_);

  // Search Bar Border
  search_bar_border_->SetIsDrawable(true);
  search_bar_border_->SetBackgroundColor(kContextualSearchBarBorderColor);

  // Progress Bar Background
  progress_bar_background_->SetIsDrawable(true);
  progress_bar_background_->SetFillCenter(true);

  // Progress Bar
  progress_bar_->SetIsDrawable(true);
  progress_bar_->SetFillCenter(true);

  // Search Content View
  layer_->AddChild(content_view_container_);
}

ContextualSearchLayer::~ContextualSearchLayer() {
}

scoped_refptr<cc::Layer> ContextualSearchLayer::layer() {
  return layer_;
}

}  //  namespace android
}  //  namespace chrome
