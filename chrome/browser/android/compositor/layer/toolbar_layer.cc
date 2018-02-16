// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/compositor/layer/toolbar_layer.h"

#include "cc/layers/nine_patch_layer.h"
#include "cc/layers/solid_color_layer.h"
#include "cc/layers/ui_resource_layer.h"
#include "cc/resources/scoped_ui_resource.h"
#include "chrome/browser/android/compositor/resources/toolbar_resource.h"
#include "content/public/browser/android/compositor.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/android/resources/nine_patch_resource.h"
#include "ui/android/resources/resource_manager.h"

namespace android {

// static
scoped_refptr<ToolbarLayer> ToolbarLayer::Create(
    ui::ResourceManager* resource_manager) {
  return base::WrapRefCounted(new ToolbarLayer(resource_manager));
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
    float window_height,
    float y_offset,
    bool show_debug,
    bool clip_shadow,
    bool modern_design_enabled) {
  ToolbarResource* resource =
      ToolbarResource::From(resource_manager_->GetResource(
          ui::ANDROID_RESOURCE_TYPE_DYNAMIC, toolbar_resource_id));

  // Ensure the toolbar resource is available before making the layer visible.
  layer_->SetHideLayerAndSubtree(!resource);
  if (!resource)
    return;

  // This layer effectively draws over the space the resource takes for shadows.
  // Set the bounds to the non-shadow size so that other things can properly
  // line up.
  gfx::Size toolbar_bounds =
      gfx::Size(resource->size().width(),
                resource->size().height() - resource->shadow_height());
  layer_->SetBounds(toolbar_bounds);

  toolbar_background_layer_->SetBounds(resource->toolbar_rect().size());
  toolbar_background_layer_->SetPosition(
      gfx::PointF(resource->toolbar_rect().origin()));
  toolbar_background_layer_->SetBackgroundColor(toolbar_background_color);

  bool url_bar_visible =
      (resource->location_bar_content_rect().width() != 0) && url_bar_alpha > 0;
  url_bar_background_layer_->SetHideLayerAndSubtree(!url_bar_visible);
  if (url_bar_visible) {
    ui::NinePatchResource* url_bar_background_resource;
    if (!modern_design_enabled) {
      url_bar_background_resource = ui::NinePatchResource::From(
          resource_manager_->GetResource(ui::ANDROID_RESOURCE_TYPE_STATIC,
                                         url_bar_background_resource_id));
    } else {
      url_bar_background_resource = ui::NinePatchResource::From(
          resource_manager_->GetStaticResourceWithTint(
              url_bar_background_resource_id,
              toolbar_textbox_background_color));
    }

    gfx::Size draw_size(url_bar_background_resource->DrawSize(
        resource->location_bar_content_rect().size()));
    gfx::Rect border(url_bar_background_resource->Border(draw_size));
    gfx::PointF position(url_bar_background_resource->DrawPosition(
        resource->location_bar_content_rect().origin()));

    url_bar_background_layer_->SetBounds(draw_size);
    url_bar_background_layer_->SetPosition(position);
    url_bar_background_layer_->SetBorder(border);
    url_bar_background_layer_->SetAperture(
        url_bar_background_resource->aperture());
    url_bar_background_layer_->SetUIResourceId(
        url_bar_background_resource->ui_resource()->id());
    url_bar_background_layer_->SetOpacity(url_bar_alpha);
  }

  bitmap_layer_->SetUIResourceId(resource->ui_resource()->id());
  bitmap_layer_->SetBounds(resource->size());

  layer_->SetMasksToBounds(clip_shadow);

  // TODO(mdjones): Remove the anonymize layer once modern launches.
  anonymize_layer_->SetHideLayerAndSubtree(!anonymize || modern_design_enabled);
  if (!modern_design_enabled && anonymize) {
    anonymize_layer_->SetPosition(
        gfx::PointF(resource->location_bar_content_rect().origin()));
    anonymize_layer_->SetBounds(resource->location_bar_content_rect().size());
    anonymize_layer_->SetBackgroundColor(toolbar_textbox_background_color);
  }

  // If modern is enabled, the location bar background doubles as the
  // anonymize layer -- it just needs to be drawn on top of the toolbar
  // bitmap.
  if (modern_design_enabled) {
    int background_layer_index = GetIndexOfLayer(toolbar_background_layer_);

    bool needs_move_to_front =
        anonymize && layer_->children().back() != url_bar_background_layer_;
    bool needs_move_to_back =
        !anonymize &&
        layer_->child_at(background_layer_index) != url_bar_background_layer_;

    // If the layer needs to move, remove and re-add it.
    if (needs_move_to_front) {
      layer_->AddChild(url_bar_background_layer_);
    } else if (needs_move_to_back) {
      layer_->InsertChild(url_bar_background_layer_,
                          background_layer_index + 1);
    }
  }

  debug_layer_->SetBounds(resource->size());
  if (show_debug && !debug_layer_->parent())
    layer_->AddChild(debug_layer_);
  else if (!show_debug && debug_layer_->parent())
    debug_layer_->RemoveFromParent();

  layer_->SetPosition(gfx::PointF(0, y_offset));
}

int ToolbarLayer::GetIndexOfLayer(scoped_refptr<cc::Layer> layer) {
  for (unsigned int i = 0; i < layer_->children().size(); ++i) {
    if (layer_->child_at(i) == layer)
      return i;
  }
  return -1;
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
      layer_(cc::Layer::Create()),
      toolbar_background_layer_(cc::SolidColorLayer::Create()),
      url_bar_background_layer_(cc::NinePatchLayer::Create()),
      bitmap_layer_(cc::UIResourceLayer::Create()),
      progress_bar_layer_(cc::SolidColorLayer::Create()),
      progress_bar_background_layer_(cc::SolidColorLayer::Create()),
      anonymize_layer_(cc::SolidColorLayer::Create()),
      debug_layer_(cc::SolidColorLayer::Create()) {
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
