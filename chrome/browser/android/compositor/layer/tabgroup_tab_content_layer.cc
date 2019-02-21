// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/compositor/layer/tabgroup_tab_content_layer.h"

#include <vector>

#include "base/lazy_instance.h"
#include "cc/layers/layer.h"
#include "cc/layers/nine_patch_layer.h"
#include "cc/paint/filter_operations.h"
#include "chrome/browser/android/compositor/layer/thumbnail_layer.h"
#include "chrome/browser/android/compositor/tab_content_manager.h"
#include "content/public/browser/android/compositor.h"
#include "ui/android/resources/nine_patch_resource.h"
#include "ui/gfx/geometry/size.h"

namespace android {

// static
scoped_refptr<TabGroupTabContentLayer> TabGroupTabContentLayer::Create(
    TabContentManager* tab_content_manager) {
  return base::WrapRefCounted(new TabGroupTabContentLayer(tab_content_manager));
}

void TabGroupTabContentLayer::SetProperties(
    int id,
    bool can_use_live_layer,
    float static_to_view_blend,
    bool should_override_content_alpha,
    float content_alpha_override,
    float saturation,
    bool should_clip,
    const gfx::Rect& clip,
    ui::NinePatchResource* border_inner_shadow_resource,
    float width,
    float height,
    const std::vector<int>& tab_ids,
    float border_inner_shadow_alpha,
    int inset_diff) {
  // TODO(meiliang) : should call content_->SetProperties() and create
  // set border_inner_shadow to front_border_inner_shadow_.
}

scoped_refptr<cc::Layer> TabGroupTabContentLayer::layer() {
  return layer_;
}

TabGroupTabContentLayer::TabGroupTabContentLayer(
    TabContentManager* tab_content_manager)
    : layer_(cc::Layer::Create()),
      content_(ContentLayer::Create(tab_content_manager)),
      front_border_inner_shadow_(cc::NinePatchLayer::Create()) {
  layer_->AddChild(content_->layer());
  layer_->AddChild(front_border_inner_shadow_);

  front_border_inner_shadow_->SetIsDrawable(true);
}

TabGroupTabContentLayer::~TabGroupTabContentLayer() {}

}  //  namespace android
