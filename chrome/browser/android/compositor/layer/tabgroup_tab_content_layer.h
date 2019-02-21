// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_COMPOSITOR_LAYER_TABGROUP_TAB_CONTENT_LAYER_H_
#define CHROME_BROWSER_ANDROID_COMPOSITOR_LAYER_TABGROUP_TAB_CONTENT_LAYER_H_

#include <memory>
#include <vector>

#include "base/macros.h"
#include "chrome/browser/android/compositor/layer/content_layer.h"
#include "chrome/browser/android/compositor/layer/layer.h"
#include "ui/android/resources/nine_patch_resource.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace android {

class TabContentManager;

// Sub layer tree representation of the contents of a
// tabgroup tab with a border shadow around.
class TabGroupTabContentLayer : public Layer {
 public:
  static scoped_refptr<TabGroupTabContentLayer> Create(
      TabContentManager* tab_content_manager);

  void SetProperties(int id,
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
                     int inset_diff);

  scoped_refptr<cc::Layer> layer() override;

 protected:
  explicit TabGroupTabContentLayer(TabContentManager* tab_content_manager);
  ~TabGroupTabContentLayer() override;

 private:
  scoped_refptr<cc::Layer> layer_;
  scoped_refptr<ContentLayer> content_;
  scoped_refptr<cc::NinePatchLayer> front_border_inner_shadow_;
  DISALLOW_COPY_AND_ASSIGN(TabGroupTabContentLayer);
};

}  //  namespace android

#endif  // CHROME_BROWSER_ANDROID_COMPOSITOR_LAYER_TABGROUP_TAB_CONTENT_LAYER_H_
