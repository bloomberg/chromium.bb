// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/compositor/layer/toolbar_layer.h"

#include "cc/layers/nine_patch_layer.h"
#include "cc/layers/solid_color_layer.h"
#include "cc/layers/ui_resource_layer.h"
#include "cc/resources/scoped_ui_resource.h"
#include "content/public/browser/android/compositor.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/android/resources/resource_manager.h"

namespace chrome {
namespace android {

// static
scoped_refptr<ToolbarLayer> ToolbarLayer::Create(
    ui::ResourceManager* resource_manager) {
  return make_scoped_refptr(new ToolbarLayer(resource_manager));
}

scoped_refptr<cc::Layer> ToolbarLayer::layer() {
  return layer_;
}

void ToolbarLayer::PushResource(
    int toolbar_resource_id,
    int toolbar_background_color,
    bool anonymize,
    int toolbar_textbox_background_color,
    int url_bar_background_resource_id,
    float url_bar_alpha,
    bool show_debug,
    float brightness,
    bool clip_shadow) {
  ui::ResourceManager::Resource* resource =
      resource_manager_->GetResource(ui::ANDROID_RESOURCE_TYPE_DYNAMIC,
                                     toolbar_resource_id);

  // Ensure the toolbar resource is available before making the layer visible.
  layer_->SetHideLayerAndSubtree(!resource);
  if (!resource)
    return;

  // This layer effectively draws over the space it takes for shadows.  Set the
  // bounds to the non-shadow size so that other things can properly line up.
  // Padding height does not include the height of the tabstrip, so we add
  // it explicitly by adding y offset.
  gfx::Size size = gfx::Size(
      resource->padding.width(),
      resource->padding.height() + resource->padding.y());
  layer_->SetBounds(size);

  toolbar_background_layer_->SetBounds(resource->padding.size());
  toolbar_background_layer_->SetPosition(
      gfx::PointF(resource->padding.origin()));
  toolbar_background_layer_->SetBackgroundColor(toolbar_background_color);

  bool url_bar_visible = (resource->aperture.width() != 0);
  url_bar_background_layer_->SetHideLayerAndSubtree(!url_bar_visible);
  if (url_bar_visible) {
    ui::ResourceManager::Resource* url_bar_background_resource =
        resource_manager_->GetResource(ui::ANDROID_RESOURCE_TYPE_STATIC,
                                       url_bar_background_resource_id);
    gfx::Size url_bar_size(
        resource->aperture.width() + url_bar_background_resource->size.width()
        - url_bar_background_resource->padding.width(),
        resource->aperture.height() + url_bar_background_resource->size.height()
        - url_bar_background_resource->padding.height());
    gfx::Rect url_bar_border(
        url_bar_background_resource->Border(url_bar_size));
    gfx::PointF url_bar_position = gfx::PointF(
        resource->aperture.x() - url_bar_background_resource->padding.x(),
        resource->aperture.y() - url_bar_background_resource->padding.y());

    url_bar_background_layer_->SetUIResourceId(
        url_bar_background_resource->ui_resource->id());
    url_bar_background_layer_->SetBorder(url_bar_border);
    url_bar_background_layer_->SetAperture(
        url_bar_background_resource->aperture);
    url_bar_background_layer_->SetBounds(url_bar_size);
    url_bar_background_layer_->SetPosition(url_bar_position);
    url_bar_background_layer_->SetOpacity(url_bar_alpha);
  }

  bitmap_layer_->SetUIResourceId(resource->ui_resource->id());
  bitmap_layer_->SetBounds(resource->size);

  layer_->SetMasksToBounds(clip_shadow);

  anonymize_layer_->SetHideLayerAndSubtree(!anonymize);
  if (anonymize) {
    anonymize_layer_->SetPosition(gfx::PointF(resource->aperture.origin()));
    anonymize_layer_->SetBounds(resource->aperture.size());
    anonymize_layer_->SetBackgroundColor(toolbar_textbox_background_color);
  }

  debug_layer_->SetBounds(resource->size);
  if (show_debug && !debug_layer_->parent())
    layer_->AddChild(debug_layer_);
  else if (!show_debug && debug_layer_->parent())
    debug_layer_->RemoveFromParent();

  if (brightness != brightness_) {
    brightness_ = brightness;
    cc::FilterOperations filters;
    if (brightness_ < 1.f)
      filters.Append(cc::FilterOperation::CreateBrightnessFilter(brightness_));
    layer_->SetFilters(filters);
  }
}

void ToolbarLayer::UpdateProgressBar(int progress_bar_x,
                                     int progress_bar_y,
                                     int progress_bar_width,
                                     int progress_bar_height,
                                     int progress_bar_color,
                                     int progress_bar_background_x,
                                     int progress_bar_background_y,
                                     int progress_bar_background_width,
                                     int progress_bar_background_height,
                                     int progress_bar_background_color) {
  bool is_progress_bar_background_visible = SkColorGetA(
      progress_bar_background_color);
  progress_bar_background_layer_->SetHideLayerAndSubtree(
      !is_progress_bar_background_visible);
  if (is_progress_bar_background_visible) {
    progress_bar_background_layer_->SetPosition(
        gfx::PointF(progress_bar_background_x, progress_bar_background_y));
    progress_bar_background_layer_->SetBounds(
        gfx::Size(progress_bar_background_width,
                  progress_bar_background_height));
    progress_bar_background_layer_->SetBackgroundColor(
        progress_bar_background_color);
  }

  bool is_progress_bar_visible = SkColorGetA(progress_bar_background_color);
  progress_bar_layer_->SetHideLayerAndSubtree(!is_progress_bar_visible);
  if (is_progress_bar_visible) {
    progress_bar_layer_->SetPosition(
        gfx::PointF(progress_bar_x, progress_bar_y));
    progress_bar_layer_->SetBounds(
        gfx::Size(progress_bar_width, progress_bar_height));
    progress_bar_layer_->SetBackgroundColor(progress_bar_color);
  }
}

ToolbarLayer::ToolbarLayer(ui::ResourceManager* resource_manager)
    : resource_manager_(resource_manager),
      layer_(cc::Layer::Create(content::Compositor::LayerSettings())),
      toolbar_background_layer_(
          cc::SolidColorLayer::Create(content::Compositor::LayerSettings())),
      url_bar_background_layer_(
          cc::NinePatchLayer::Create(content::Compositor::LayerSettings())),
      bitmap_layer_(
          cc::UIResourceLayer::Create(content::Compositor::LayerSettings())),
      progress_bar_layer_(
          cc::SolidColorLayer::Create(content::Compositor::LayerSettings())),
      progress_bar_background_layer_(
          cc::SolidColorLayer::Create(content::Compositor::LayerSettings())),
      anonymize_layer_(
          cc::SolidColorLayer::Create(content::Compositor::LayerSettings())),
      debug_layer_(
          cc::SolidColorLayer::Create(content::Compositor::LayerSettings())),
      brightness_(1.f) {
  toolbar_background_layer_->SetIsDrawable(true);
  layer_->AddChild(toolbar_background_layer_);

  url_bar_background_layer_->SetIsDrawable(true);
  url_bar_background_layer_->SetFillCenter(true);
  layer_->AddChild(url_bar_background_layer_);

  bitmap_layer_->SetIsDrawable(true);
  layer_->AddChild(bitmap_layer_);

  progress_bar_background_layer_->SetIsDrawable(true);
  progress_bar_background_layer_->SetHideLayerAndSubtree(true);
  layer_->AddChild(progress_bar_background_layer_);

  progress_bar_layer_->SetIsDrawable(true);
  progress_bar_layer_->SetHideLayerAndSubtree(true);
  layer_->AddChild(progress_bar_layer_);

  anonymize_layer_->SetIsDrawable(true);
  anonymize_layer_->SetBackgroundColor(SK_ColorWHITE);
  layer_->AddChild(anonymize_layer_);

  debug_layer_->SetIsDrawable(true);
  debug_layer_->SetBackgroundColor(SK_ColorGREEN);
  debug_layer_->SetOpacity(0.5f);
}

ToolbarLayer::~ToolbarLayer() {
}

}  //  namespace android
}  //  namespace chrome
