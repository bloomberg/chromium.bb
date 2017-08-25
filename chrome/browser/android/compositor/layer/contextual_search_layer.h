// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_COMPOSITOR_LAYER_CONTEXTUAL_SEARCH_LAYER_H_
#define CHROME_BROWSER_ANDROID_COMPOSITOR_LAYER_CONTEXTUAL_SEARCH_LAYER_H_

#include <memory>

#include "chrome/browser/android/compositor/layer/overlay_panel_layer.h"

namespace cc {
class Layer;
class NinePatchLayer;
class SolidColorLayer;
class UIResourceLayer;
}

namespace cc {
class Layer;
}

namespace ui {
class ResourceManager;
}

namespace android {

class ContextualSearchLayer : public OverlayPanelLayer {
 public:
  static scoped_refptr<ContextualSearchLayer> Create(
      ui::ResourceManager* resource_manager);

  void SetProperties(int panel_shadow_resource_id,
                     int search_context_resource_id,
                     int search_term_resource_id,
                     int search_caption_resource_id,
                     int search_bar_shadow_resource_id,
                     int search_provider_icon_resource_id,
                     int quick_action_icon_resource_id,
                     int arrow_up_resource_id,
                     int close_icon_resource_id,
                     int progress_bar_background_resource_id,
                     int progress_bar_resource_id,
                     int search_promo_resource_id,
                     int peek_promo_ripple_resource_id,
                     int peek_promo_text_resource_id,
                     float dp_to_px,
                     const scoped_refptr<cc::Layer>& content_layer,
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
                     float search_text_layer_min_height,
                     float search_term_opacity,
                     float search_term_caption_spacing,
                     float search_caption_animation_percentage,
                     bool search_caption_visible,
                     bool search_bar_border_visible,
                     float search_bar_border_height,
                     bool search_bar_shadow_visible,
                     float search_bar_shadow_opacity,
                     bool quick_action_icon_visible,
                     bool thumbnail_visible,
                     float custom_image_visibility_percentage,
                     int bar_image_size,
                     float arrow_icon_opacity,
                     float arrow_icon_rotation,
                     float close_icon_opacity,
                     bool progress_bar_visible,
                     float progress_bar_height,
                     float progress_bar_opacity,
                     int progress_bar_completion,
                     float divider_line_visibility_percentage,
                     float divider_line_width,
                     float divider_line_height,
                     int divider_line_color,
                     float divider_line_x_offset,
                     bool touch_highlight_visible,
                     float touch_highlight_x_offset,
                     float touch_highlight_width);

  void SetThumbnail(const SkBitmap* thumbnail);

 protected:
  explicit ContextualSearchLayer(ui::ResourceManager* resource_manager);
  ~ContextualSearchLayer() override;
  scoped_refptr<cc::Layer> GetIconLayer() override;

 private:
  // Sets up |icon_layer_|, which displays an icon or thumbnail at the start
  // of the Bar.
  void SetupIconLayer(int search_provider_icon_resource_id,
                      bool quick_action_icon_visible,
                      int quick_action_icon_resource_id,
                      bool thumbnail_visible,
                      float custom_image_visibility_percentage);

  void SetCustomImageProperties(
      scoped_refptr<cc::UIResourceLayer> custom_image_layer,
      float top_margin,
      float side_margin,
      float visibility_percentage);

  // Sets up |text_layer_|, which contains |bar_text_|, |search_context_| and
  // |search_caption_|.
  void SetupTextLayer(float search_bar_top,
                      float search_bar_height,
                      float search_text_layer_min_height,
                      int search_caption_resource_id,
                      bool search_caption_visible,
                      float search_caption_animation_percentage,
                      float search_term_opacity,
                      int search_context_resource_id,
                      float search_context_opacity,
                      float search_term_caption_spacing);

  int bar_image_size_;
  float thumbnail_side_margin_;
  float thumbnail_top_margin_;

  scoped_refptr<cc::UIResourceLayer> search_context_;
  scoped_refptr<cc::Layer> icon_layer_;
  scoped_refptr<cc::UIResourceLayer> search_provider_icon_layer_;
  scoped_refptr<cc::UIResourceLayer> thumbnail_layer_;
  scoped_refptr<cc::UIResourceLayer> quick_action_icon_layer_;
  scoped_refptr<cc::UIResourceLayer> arrow_icon_;
  scoped_refptr<cc::UIResourceLayer> search_promo_;
  scoped_refptr<cc::SolidColorLayer> search_promo_container_;
  scoped_refptr<cc::SolidColorLayer> peek_promo_container_;
  scoped_refptr<cc::NinePatchLayer> peek_promo_ripple_;
  scoped_refptr<cc::UIResourceLayer> peek_promo_text_;
  scoped_refptr<cc::NinePatchLayer> progress_bar_;
  scoped_refptr<cc::NinePatchLayer> progress_bar_background_;
  scoped_refptr<cc::UIResourceLayer> search_caption_;
  scoped_refptr<cc::UIResourceLayer> text_layer_;
  scoped_refptr<cc::SolidColorLayer> divider_line_;
  scoped_refptr<cc::SolidColorLayer> touch_highlight_layer_;
};

}  //  namespace android

#endif  // CHROME_BROWSER_ANDROID_COMPOSITOR_LAYER_CONTEXTUAL_SEARCH_LAYER_H_
