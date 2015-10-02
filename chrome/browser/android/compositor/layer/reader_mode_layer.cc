// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/compositor/layer/reader_mode_layer.h"

#include "cc/layers/layer.h"
#include "cc/layers/nine_patch_layer.h"
#include "cc/layers/solid_color_layer.h"
#include "cc/layers/ui_resource_layer.h"
#include "cc/output/filter_operation.h"
#include "cc/output/filter_operations.h"
#include "cc/resources/scoped_ui_resource.h"
#include "content/public/browser/android/compositor.h"
#include "content/public/browser/android/content_view_core.h"
#include "content/public/browser/web_contents.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/android/resources/resource_manager.h"

namespace chrome {
namespace android {

// static
scoped_refptr<ReaderModeLayer> ReaderModeLayer::Create(
    ui::ResourceManager* resource_manager) {
  return make_scoped_refptr(new ReaderModeLayer(resource_manager));
}

void ReaderModeLayer::SetProperties(
    int panel_background_resource_id,
    int panel_text_resource_id,
    content::ContentViewCore* reader_mode_content_view_core,
    float panel_y,
    float panel_width,
    float panel_margin_top,
    float panel_height,
    float distilled_y,
    float distilled_height,
    float x,
    float panel_text_opacity,
    int header_background_color) {
  // Grab the dynamic Reader Mode Text resource.
  ui::ResourceManager::Resource* panel_text_resource =
      resource_manager_->GetResource(ui::ANDROID_RESOURCE_TYPE_DYNAMIC,
                                     panel_text_resource_id);

  // Grab required static resources.
  ui::ResourceManager::Resource* panel_background_resource =
      resource_manager_->GetResource(ui::ANDROID_RESOURCE_TYPE_STATIC,
                                     panel_background_resource_id);

  DCHECK(panel_background_resource);

  cc::FilterOperations blur_filter;

  // ---------------------------------------------------------------------------
  // Reader Mode Bar Background
  // ---------------------------------------------------------------------------
  gfx::Size background_size(panel_width, panel_height);
  panel_background_->SetUIResourceId(
      panel_background_resource->ui_resource->id());
  panel_background_->SetBorder(
      panel_background_resource->Border(background_size));
  panel_background_->SetAperture(panel_background_resource->aperture);
  panel_background_->SetBounds(background_size);
  panel_background_->SetPosition(gfx::PointF(0.0f, panel_y));
  panel_background_->SetOpacity(panel_text_opacity);

  // ---------------------------------------------------------------------------
  // Reader Mode Bar Text
  // ---------------------------------------------------------------------------
  if (panel_text_resource) {
    // Centralizes the text vertically in the Reader Mode panel.
    panel_text_->SetUIResourceId(
        panel_text_resource->ui_resource->id());
    panel_text_->SetBounds(panel_text_resource->size);
    panel_text_->SetPosition(gfx::PointF(0.f, panel_margin_top + panel_y));
    panel_text_->SetFilters(blur_filter);
    panel_text_->SetOpacity(panel_text_opacity);
  }

  // ---------------------------------------------------------------------------
  // Reader Mode Content View
  // ---------------------------------------------------------------------------
  content_shadow_->SetUIResourceId(
      panel_background_resource->ui_resource->id());
  content_shadow_->SetBorder(
      panel_background_resource->Border(background_size));
  content_shadow_->SetAperture(panel_background_resource->aperture);
  content_shadow_->SetBounds(background_size);
  content_shadow_->SetPosition(gfx::PointF(0.f, distilled_y));

  content_solid_->SetBounds(background_size);
  content_solid_->SetPosition(gfx::PointF(0.f, panel_margin_top + distilled_y));
  content_solid_->SetBackgroundColor(header_background_color);

  content_view_container_->SetPosition(
      gfx::PointF(0.f, panel_margin_top + distilled_y));
  content_view_container_->SetOpacity(1.0f - panel_text_opacity);

  if (reader_mode_content_view_core &&
      reader_mode_content_view_core->GetLayer().get()) {
    scoped_refptr<cc::Layer> content_view_layer =
        reader_mode_content_view_core->GetLayer();
    if (content_view_layer->parent() != content_view_container_) {
      content_view_container_->AddChild(content_view_layer);
      content_view_container_->SetFilters(blur_filter);
    }
  } else {
    content_view_container_->RemoveAllChildren();
  }

  // ---------------------------------------------------------------------------
  // Reader Mode Panel.
  // ---------------------------------------------------------------------------
  layer_->SetPosition(gfx::PointF(x, 0.f));
}

ReaderModeLayer::ReaderModeLayer(ui::ResourceManager* resource_manager)
    : resource_manager_(resource_manager),
      layer_(cc::Layer::Create(content::Compositor::LayerSettings())),
      panel_background_(
          cc::NinePatchLayer::Create(content::Compositor::LayerSettings())),
      panel_text_(
          cc::UIResourceLayer::Create(content::Compositor::LayerSettings())),
      content_shadow_(
          cc::NinePatchLayer::Create(content::Compositor::LayerSettings())),
      content_solid_(
          cc::SolidColorLayer::Create(content::Compositor::LayerSettings())),
      content_view_container_(
          cc::Layer::Create(content::Compositor::LayerSettings())) {
  layer_->SetMasksToBounds(false);

  // Reader Mode Background
  panel_background_->SetIsDrawable(true);
  panel_background_->SetFillCenter(true);
  layer_->AddChild(panel_background_);

  // Reader Mode Text
  panel_text_->SetIsDrawable(true);
  layer_->AddChild(panel_text_);

  // Reader Mode Content shadow
  content_shadow_->SetIsDrawable(true);
  content_shadow_->SetFillCenter(true);
  layer_->AddChild(content_shadow_);

  // Reader Mode Content solid background
  content_solid_->SetIsDrawable(true);
  layer_->AddChild(content_solid_);

  // Reader Mode Content View
  layer_->AddChild(content_view_container_);
}

ReaderModeLayer::~ReaderModeLayer() {
}

scoped_refptr<cc::Layer> ReaderModeLayer::layer() {
  return layer_;
}

}  //  namespace android
}  //  namespace chrome
