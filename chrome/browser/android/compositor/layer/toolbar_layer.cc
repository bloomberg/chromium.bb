// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/compositor/layer/toolbar_layer.h"

#include "cc/layers/solid_color_layer.h"
#include "cc/layers/ui_resource_layer.h"
#include "cc/resources/scoped_ui_resource.h"
#include "content/public/browser/android/compositor.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/android/resources/resource_manager.h"

namespace chrome {
namespace android {

// static
scoped_refptr<ToolbarLayer> ToolbarLayer::Create() {
  return make_scoped_refptr(new ToolbarLayer());
}

scoped_refptr<cc::Layer> ToolbarLayer::layer() {
  return layer_;
}

void ToolbarLayer::PushResource(
    ui::ResourceManager::Resource* resource,
    int toolbar_background_color,
    bool anonymize,
    int toolbar_textbox_background_color,
    bool show_debug,
    float brightness) {
  DCHECK(resource);

  // This layer effectively draws over the space it takes for shadows.  Set the
  // bounds to the non-shadow size so that other things can properly line up.
  // Padding height does not include the height of the tabstrip, so we add
  // it explicitly by adding y offset.
  gfx::Size size = gfx::Size(
      resource->padding.width(),
      resource->padding.height() + resource->padding.y());
  layer_->SetBounds(size);

  toolbar_background_layer_->SetBounds(resource->padding.size());
  toolbar_background_layer_->SetPosition(resource->padding.origin());
  toolbar_background_layer_->SetBackgroundColor(toolbar_background_color);

  bitmap_layer_->SetUIResourceId(resource->ui_resource->id());
  bitmap_layer_->SetBounds(resource->size);

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

ToolbarLayer::ToolbarLayer()
    : layer_(cc::Layer::Create(content::Compositor::LayerSettings())),
      toolbar_background_layer_(
          cc::SolidColorLayer::Create(content::Compositor::LayerSettings())),
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
