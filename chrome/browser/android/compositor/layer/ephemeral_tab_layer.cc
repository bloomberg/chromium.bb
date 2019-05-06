// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/compositor/layer/ephemeral_tab_layer.h"

#include "cc/layers/layer.h"
#include "cc/layers/nine_patch_layer.h"
#include "cc/resources/scoped_ui_resource.h"
#include "content/public/browser/android/compositor.h"
#include "ui/android/resources/nine_patch_resource.h"
#include "ui/android/resources/resource_manager.h"

namespace android {
// static
scoped_refptr<EphemeralTabLayer> EphemeralTabLayer::Create(
    ui::ResourceManager* resource_manager) {
  return base::WrapRefCounted(new EphemeralTabLayer(resource_manager));
}

void EphemeralTabLayer::SetProperties(
    int title_view_resource_id,
    jfloat text_layer_min_height,
    int progress_bar_background_resource_id,
    int progress_bar_resource_id,
    float dp_to_px,
    const scoped_refptr<cc::Layer>& content_layer,
    float panel_x,
    float panel_y,
    float panel_width,
    float panel_height,
    int bar_background_color,
    float bar_margin_side,
    float bar_margin_top,
    float bar_height,
    bool bar_border_visible,
    float bar_border_height,
    bool bar_shadow_visible,
    float bar_shadow_opacity,
    int icon_color,
    int drag_handlebar_color,
    bool progress_bar_visible,
    float progress_bar_height,
    float progress_bar_opacity,
    int progress_bar_completion) {
  // Round values to avoid pixel gap between layers.
  bar_height = floor(bar_height);
  float bar_top = 0.f;
  float bar_bottom = bar_top + bar_height;

  float title_opacity = 0.f;
  OverlayPanelLayer::SetProperties(
      dp_to_px, content_layer, bar_height, panel_x, panel_y, panel_width,
      panel_height, bar_background_color, bar_margin_side, bar_margin_top,
      bar_height, 0.0f, title_opacity, bar_border_visible, bar_border_height,
      bar_shadow_visible, bar_shadow_opacity, icon_color, drag_handlebar_color,
      1.0f /* icon opacity */);

  SetupTextLayer(bar_top, bar_height, text_layer_min_height,
                 title_view_resource_id);

  OverlayPanelLayer::SetProgressBar(
      progress_bar_background_resource_id, progress_bar_resource_id,
      progress_bar_visible, bar_bottom, progress_bar_height,
      progress_bar_opacity, progress_bar_completion, panel_width);
}

void EphemeralTabLayer::SetupTextLayer(float bar_top,
                                       float bar_height,
                                       float text_layer_min_height,
                                       int title_resource_id) {
  // ---------------------------------------------------------------------------
  // Setup the Drawing Hierarchy
  // ---------------------------------------------------------------------------

  DCHECK(text_layer_.get());
  DCHECK(title_.get());

  // Title
  ui::Resource* title_resource = resource_manager_->GetResource(
      ui::ANDROID_RESOURCE_TYPE_DYNAMIC, title_resource_id);
  if (title_resource) {
    title_->SetUIResourceId(title_resource->ui_resource()->id());
    title_->SetBounds(title_resource->size());
  }

  // ---------------------------------------------------------------------------
  // Calculate Text Layer Size
  // ---------------------------------------------------------------------------
  float title_height = title_->bounds().height();

  float layer_height = std::max(text_layer_min_height, title_height);
  float layer_width = title_->bounds().width();

  float layer_top = bar_top + (bar_height - layer_height) / 2;
  text_layer_->SetBounds(gfx::Size(layer_width, layer_height));
  text_layer_->SetPosition(gfx::PointF(0.f, layer_top));
  text_layer_->SetMasksToBounds(true);

  // ---------------------------------------------------------------------------
  // Layout Text Layer
  // ---------------------------------------------------------------------------
  // ---Top of Panel Bar---  <- bar_top
  //
  // ---Top of Text Layer--- <- layer_top
  //                         } remaining_height / 2
  //      Title              } title_height
  // --Bottom of Text Layer-
  //
  // --Bottom of Panel Bar-

  float title_top = (layer_height - title_height) / 2;
  title_->SetPosition(gfx::PointF(0.f, title_top));
}

EphemeralTabLayer::EphemeralTabLayer(ui::ResourceManager* resource_manager)
    : OverlayPanelLayer(resource_manager),
      title_(cc::UIResourceLayer::Create()),
      text_layer_(cc::UIResourceLayer::Create()) {
  // Content layer
  text_layer_->SetIsDrawable(true);

  title_->SetIsDrawable(true);

  AddBarTextLayer(text_layer_);
  text_layer_->AddChild(title_);
}

EphemeralTabLayer::~EphemeralTabLayer() {}

}  //  namespace android
