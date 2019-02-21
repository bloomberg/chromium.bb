// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/compositor/layer/tabgroup_content_layer.h"

#include <vector>

#include "base/lazy_instance.h"
#include "cc/layers/layer.h"
#include "cc/layers/layer_collections.h"
#include "cc/layers/nine_patch_layer.h"
#include "cc/paint/filter_operations.h"
#include "chrome/browser/android/compositor/layer/thumbnail_layer.h"
#include "chrome/browser/android/compositor/tab_content_manager.h"
#include "content/public/browser/android/compositor.h"
#include "ui/android/resources/nine_patch_resource.h"
#include "ui/gfx/geometry/size.h"

namespace android {

// static
scoped_refptr<TabGroupContentLayer> TabGroupContentLayer::Create(
    TabContentManager* tab_content_manager) {
  return base::WrapRefCounted(new TabGroupContentLayer(tab_content_manager));
}

void TabGroupContentLayer::SetProperties(
    int id,
    const std::vector<int>& tab_ids,
    bool can_use_live_layer,
    float static_to_view_blend,
    bool should_override_content_alpha,
    float content_alpha_override,
    float saturation,
    bool should_clip,
    const gfx::Rect& clip,
    float width,
    float height,
    int inset_diff,
    ui::NinePatchResource* inner_shadow_resource,
    float inner_shadow_alpha) {
  // TODO(meiliang): Override here;
}

TabGroupContentLayer::TabGroupContentLayer(
    TabContentManager* tab_content_manager)
    : ContentLayer(tab_content_manager) {}

TabGroupContentLayer::~TabGroupContentLayer() {}

}  //  namespace android
