// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_COMPOSITOR_LAYER_CONTEXTUAL_SEARCH_LAYER_H_
#define CHROME_BROWSER_ANDROID_COMPOSITOR_LAYER_CONTEXTUAL_SEARCH_LAYER_H_

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/android/compositor/layer/overlay_panel_layer.h"

namespace cc {
class Layer;
class NinePatchLayer;
class SolidColorLayer;
class UIResourceLayer;
}

namespace content {
class ContentViewCore;
}

namespace ui {
class ResourceManager;
}

namespace chrome {
namespace android {

class CrushedSpriteLayer;

class ContextualSearchLayer : public OverlayPanelLayer {
 public:
  static scoped_refptr<ContextualSearchLayer> Create(
      ui::ResourceManager* resource_manager);

  void SetProperties(int panel_shadow_resource_id,
                     int search_context_resource_id,
                     int search_term_resource_id,
                     int search_bar_shadow_resource_id,
                     int panel_icon_resource_id,
                     int arrow_up_resource_id,
                     int close_icon_resource_id,
                     int progress_bar_background_resource_id,
                     int progress_bar_resource_id,
                     int search_promo_resource_id,
                     int peek_promo_ripple_resource_id,
                     int peek_promo_text_resource_id,
                     int search_provider_icon_sprite_bitmap_resource_id,
                     int search_provider_icon_sprite_metadata_resource_id,
                     float dp_to_px,
                     content::ContentViewCore* content_view_core,
                     bool search_promo_visible,
                     float search_promo_height,
                     float search_promo_opacity,
                     bool search_peek_promo_visible,
                     float search_peek_promo_height,
                     float search_peek_promo_padding,
                     float search_peek_promo_ripple_width,
                     float search_peek_promo_ripple_opacity,
                     float search_peek_promo_text_opacity,
                     float search_panel_x,
                     float search_panel_y,
                     float search_panel_width,
                     float search_panel_height,
                     float search_bar_margin_side,
                     float search_bar_height,
                     float search_context_opacity,
                     float search_term_opacity,
                     bool search_bar_border_visible,
                     float search_bar_border_height,
                     bool search_bar_shadow_visible,
                     float search_bar_shadow_opacity,
                     bool search_provider_icon_sprite_visible,
                     float search_provider_icon_sprite_completion_percentage,
                     float arrow_icon_opacity,
                     float arrow_icon_rotation,
                     float close_icon_opacity,
                     bool progress_bar_visible,
                     float progress_bar_height,
                     float progress_bar_opacity,
                     int progress_bar_completion);

 protected:
  explicit ContextualSearchLayer(ui::ResourceManager* resource_manager);
  ~ContextualSearchLayer() override;
  scoped_refptr<cc::Layer> GetIconLayer() override;

 private:
  bool search_provider_icon_sprite_visible_;
  int search_provider_icon_sprite_bitmap_resource_id_;
  int search_provider_icon_sprite_metadata_resource_id_;
  float search_provider_icon_sprite_completion_percentage_;

  scoped_refptr<cc::UIResourceLayer> search_context_;
  scoped_refptr<CrushedSpriteLayer> search_provider_icon_sprite_;
  scoped_refptr<cc::UIResourceLayer> arrow_icon_;
  scoped_refptr<cc::UIResourceLayer> search_promo_;
  scoped_refptr<cc::SolidColorLayer> search_promo_container_;
  scoped_refptr<cc::SolidColorLayer> peek_promo_container_;
  scoped_refptr<cc::NinePatchLayer> peek_promo_ripple_;
  scoped_refptr<cc::UIResourceLayer> peek_promo_text_;
  scoped_refptr<cc::NinePatchLayer> progress_bar_;
  scoped_refptr<cc::NinePatchLayer> progress_bar_background_;
};

}  //  namespace android
}  //  namespace chrome

#endif  // CHROME_BROWSER_ANDROID_COMPOSITOR_LAYER_CONTEXTUAL_SEARCH_LAYER_H_
