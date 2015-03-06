// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/compositor/layer/toolbar_layer.h"

#include "cc/layers/solid_color_layer.h"
#include "cc/layers/ui_resource_layer.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/android/resources/resource_manager.h"
#include "ui/android/resources/ui_resource_android.h"

const SkColor kNormalAnonymizeContentColor = SK_ColorWHITE;
const SkColor kIncognitoAnonymizeContentColor = 0xFF737373;

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
    ui::ResourceManager::Resource* progress_resource,
    bool anonymize,
    bool anonymize_component_is_incognito,
    bool show_debug) {
  DCHECK(resource);

  // This layer effectively draws over the space it takes for shadows.  Set the
  // bounds to the non-shadow size so that other things can properly line up.
  layer_->SetBounds(resource->padding.size());

  bitmap_layer_->SetUIResourceId(resource->ui_resource->id());
  bitmap_layer_->SetBounds(resource->size);

  anonymize_layer_->SetHideLayerAndSubtree(!anonymize);
  if (anonymize) {
    anonymize_layer_->SetPosition(resource->aperture.origin());
    anonymize_layer_->SetBounds(resource->aperture.size());
    anonymize_layer_->SetBackgroundColor(anonymize_component_is_incognito
                                             ? kIncognitoAnonymizeContentColor
                                             : kNormalAnonymizeContentColor);
  }

  progress_layer_->SetHideLayerAndSubtree(!progress_resource);
  if (progress_resource) {
    progress_layer_->SetUIResourceId(progress_resource->ui_resource->id());
    progress_layer_->SetBounds(progress_resource->size);
    progress_layer_->SetPosition(progress_resource->padding.origin());
  }

  debug_layer_->SetBounds(resource->size);
  if (show_debug && !debug_layer_->parent())
    layer_->AddChild(debug_layer_);
  else if (!show_debug && debug_layer_->parent())
    debug_layer_->RemoveFromParent();
}

void ToolbarLayer::PushResource(ui::ResourceManager::Resource* resource,
                                bool anonymize,
                                bool anonymize_component_is_incognito,
                                bool show_debug) {
  PushResource(resource,
               nullptr,
               anonymize,
               anonymize_component_is_incognito,
               show_debug);
}

ToolbarLayer::ToolbarLayer()
    : layer_(cc::Layer::Create()),
      bitmap_layer_(cc::UIResourceLayer::Create()),
      progress_layer_(cc::UIResourceLayer::Create()),
      anonymize_layer_(cc::SolidColorLayer::Create()),
      debug_layer_(cc::SolidColorLayer::Create()) {
  bitmap_layer_->SetIsDrawable(true);
  layer_->AddChild(bitmap_layer_);

  progress_layer_->SetIsDrawable(true);
  progress_layer_->SetHideLayerAndSubtree(true);
  layer_->AddChild(progress_layer_);

  anonymize_layer_->SetHideLayerAndSubtree(true);
  anonymize_layer_->SetIsDrawable(true);
  anonymize_layer_->SetBackgroundColor(kNormalAnonymizeContentColor);
  layer_->AddChild(anonymize_layer_);

  debug_layer_->SetIsDrawable(true);
  debug_layer_->SetBackgroundColor(SK_ColorGREEN);
  debug_layer_->SetOpacity(0.5f);
}

ToolbarLayer::~ToolbarLayer() {
}

}  //  namespace android
}  //  namespace chrome
