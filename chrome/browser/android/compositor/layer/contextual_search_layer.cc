// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/compositor/layer/contextual_search_layer.h"

#include "cc/layers/layer.h"
#include "cc/layers/nine_patch_layer.h"
#include "cc/layers/solid_color_layer.h"
#include "cc/layers/ui_resource_layer.h"
#include "cc/resources/scoped_ui_resource.h"
#include "chrome/browser/android/compositor/layer/crushed_sprite_layer.h"
#include "content/public/browser/android/compositor.h"
#include "content/public/browser/android/content_view_core.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/android/resources/crushed_sprite_resource.h"
#include "ui/android/resources/resource_manager.h"
#include "ui/base/l10n/l10n_util_android.h"
#include "ui/gfx/color_utils.h"

namespace {

const SkColor kSearchBackgroundColor = SkColorSetRGB(0xee, 0xee, 0xee);
const SkColor kSearchBarBackgroundColor = SkColorSetRGB(0xff, 0xff, 0xff);
const SkColor kSearchBarBorderColor = SkColorSetRGB(0xf1, 0xf1, 0xf1);
const SkColor kPeekPromoRippleBackgroundColor = SkColorSetRGB(0x42, 0x85, 0xF4);

// The alpha blend used in the Peek Promo Background in order to achieve
// a lighter shade of the color of the Peek Promo Ripple.
const SkAlpha kPeekPromoBackgroundMaximumAlphaBlend = 0.25f * 255;

// This is the default width for any icon displayed on an OverlayPanel.
const float kDefaultIconWidthDp = 36.0f;

}  // namespace

namespace chrome {
namespace android {

// static
scoped_refptr<ContextualSearchLayer> ContextualSearchLayer::Create(
    ui::ResourceManager* resource_manager) {
  return make_scoped_refptr(new ContextualSearchLayer(resource_manager));
}

void ContextualSearchLayer::SetProperties(
    int panel_shadow_resource_id,
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
    int progress_bar_completion) {
  // Grabs the dynamic Search Bar Text resource.
  ui::ResourceManager::Resource* search_context_resource =
      resource_manager_->GetResource(ui::ANDROID_RESOURCE_TYPE_DYNAMIC,
                                     search_context_resource_id);
  ui::ResourceManager::Resource* search_term_resource =
      resource_manager_->GetResource(ui::ANDROID_RESOURCE_TYPE_DYNAMIC,
                                     search_term_resource_id);

  // Grabs required static resources.
  ui::ResourceManager::Resource* panel_shadow_resource =
      resource_manager_->GetResource(ui::ANDROID_RESOURCE_TYPE_STATIC,
                                     panel_shadow_resource_id);

  DCHECK(panel_shadow_resource);

  // Round values to avoid pixel gap between layers.
  search_bar_height = floor(search_bar_height);

  float search_bar_top = search_peek_promo_height;
  float search_bar_bottom = search_bar_top + search_bar_height;

  bool is_rtl = l10n_util::IsLayoutRtl();

  // If |panel_icon_resource_id| != 0, the static resource associated with that
  // id will be displayed. If |panel_icon_resource_id| == 0, then the
  // search provider icon sprite will be shown instead.
  bool should_use_static_icon = panel_icon_resource_id != 0;

  // ---------------------------------------------------------------------------
  // Panel Shadow
  // ---------------------------------------------------------------------------
  gfx::Size shadow_res_size = panel_shadow_resource->size;
  gfx::Rect shadow_res_padding = panel_shadow_resource->padding;
  gfx::Size shadow_bounds(
      search_panel_width + shadow_res_size.width()
          - shadow_res_padding.size().width(),
      search_panel_height + shadow_res_size.height()
          - shadow_res_padding.size().height());
  panel_shadow_->SetUIResourceId(panel_shadow_resource->ui_resource->id());
  panel_shadow_->SetBorder(panel_shadow_resource->Border(shadow_bounds));
  panel_shadow_->SetAperture(panel_shadow_resource->aperture);
  panel_shadow_->SetBounds(shadow_bounds);
  gfx::PointF shadow_position(-shadow_res_padding.origin().x(),
                              -shadow_res_padding.origin().y());
  panel_shadow_->SetPosition(shadow_position);

  // ---------------------------------------------------------------------------
  // Peek Promo
  // ---------------------------------------------------------------------------
  if (search_peek_promo_visible) {
    // Grabs the Search Opt Out Promo resource.
    ui::ResourceManager::Resource* peek_promo_text_resource =
        resource_manager_->GetResource(ui::ANDROID_RESOURCE_TYPE_DYNAMIC,
                                       peek_promo_text_resource_id);

    ui::ResourceManager::Resource* peek_promo_ripple_resource =
        resource_manager_->GetResource(ui::ANDROID_RESOURCE_TYPE_STATIC,
                                       peek_promo_ripple_resource_id);

    // -----------------------------------------------------------------
    // Peek Promo Container
    // -----------------------------------------------------------------
    if (peek_promo_container_->parent() != layer_) {
      layer_->AddChild(peek_promo_container_);
    }

    gfx::Size peek_promo_size(search_panel_width, search_peek_promo_height);
    peek_promo_container_->SetBounds(peek_promo_size);
    peek_promo_container_->SetPosition(gfx::PointF(0.f, 0.f));
    peek_promo_container_->SetMasksToBounds(true);

    // Apply a blend based on the ripple opacity. The resulting color will
    // be an interpolation between the background color of the Search Bar and
    // a lighter shade of the background color of the Ripple. The range of
    // the alpha value used in the blend will be:
    // [0.f, kPeekPromoBackgroundMaximumAlphaBlend]
    peek_promo_container_->SetBackgroundColor(
        color_utils::AlphaBlend(kPeekPromoRippleBackgroundColor,
                                kSearchBarBackgroundColor,
                                kPeekPromoBackgroundMaximumAlphaBlend *
                                    search_peek_promo_ripple_opacity));

    // -----------------------------------------------------------------
    // Peek Promo Ripple
    // -----------------------------------------------------------------
    gfx::Size peek_promo_ripple_size(
        search_peek_promo_ripple_width, search_peek_promo_height);
    gfx::Rect peek_promo_ripple_border(
        peek_promo_ripple_resource->Border(peek_promo_ripple_size));

    // Add padding so the ripple will occupy the whole width at 100%.
    peek_promo_ripple_size.set_width(
        peek_promo_ripple_size.width() + peek_promo_ripple_border.width());

    float ripple_rotation = 0.f;
    float ripple_left = 0.f;
    if (is_rtl) {
      // Rotate the ripple 180 degrees to make it point to the left side.
      ripple_rotation = 180.f;
      ripple_left = search_panel_width - peek_promo_ripple_size.width();
    }

    peek_promo_ripple_->SetUIResourceId(
        peek_promo_ripple_resource->ui_resource->id());
    peek_promo_ripple_->SetBorder(peek_promo_ripple_border);
    peek_promo_ripple_->SetAperture(peek_promo_ripple_resource->aperture);
    peek_promo_ripple_->SetBounds(peek_promo_ripple_size);
    peek_promo_ripple_->SetPosition(gfx::PointF(ripple_left, 0.f));
    peek_promo_ripple_->SetOpacity(search_peek_promo_ripple_opacity);

    if (ripple_rotation != 0.f) {
      // Apply rotation about the center of the resource.
      float pivot_x = floor(peek_promo_ripple_size.width() / 2);
      float pivot_y = floor(peek_promo_ripple_size.height() / 2);
      gfx::PointF pivot_origin(pivot_x, pivot_y);
      gfx::Transform transform;
      transform.Translate(pivot_origin.x(), pivot_origin.y());
      transform.RotateAboutZAxis(ripple_rotation);
      transform.Translate(-pivot_origin.x(), -pivot_origin.y());
      peek_promo_ripple_->SetTransform(transform);
    }

    // -----------------------------------------------------------------
    // Peek Promo Text
    // -----------------------------------------------------------------
    if (peek_promo_text_resource) {
      peek_promo_text_->SetUIResourceId(
          peek_promo_text_resource->ui_resource->id());
      peek_promo_text_->SetBounds(peek_promo_text_resource->size);
      peek_promo_text_->SetPosition(
          gfx::PointF(0.f, search_peek_promo_padding));
      peek_promo_text_->SetOpacity(search_peek_promo_text_opacity);
    }
  } else {
    // Peek Promo Container
    if (peek_promo_container_.get() && peek_promo_container_->parent())
      peek_promo_container_->RemoveFromParent();
  }

  // ---------------------------------------------------------------------------
  // Search Bar Background
  // ---------------------------------------------------------------------------
  gfx::Size background_size(search_panel_width, search_bar_height);
  search_bar_background_->SetBounds(background_size);
  search_bar_background_->SetPosition(gfx::PointF(0.f, search_bar_top));

  // ---------------------------------------------------------------------------
  // Search Bar Text
  // ---------------------------------------------------------------------------
  if (search_context_resource) {
    // Centers the text vertically in the Search Bar.
    float search_bar_padding_top =
        search_bar_top +
        search_bar_height / 2 -
        search_context_resource->size.height() / 2;
    search_context_->SetUIResourceId(
        search_context_resource->ui_resource->id());
    search_context_->SetBounds(search_context_resource->size);
    search_context_->SetPosition(gfx::PointF(0.f, search_bar_padding_top));
    search_context_->SetOpacity(search_context_opacity);
  }

  if (search_term_resource) {
    // Centers the text vertically in the Search Bar.
    float search_bar_padding_top =
        search_bar_top +
        search_bar_height / 2 -
        search_term_resource->size.height() / 2;
    search_term_->SetUIResourceId(search_term_resource->ui_resource->id());
    search_term_->SetBounds(search_term_resource->size);
    search_term_->SetPosition(gfx::PointF(0.f, search_bar_padding_top));
    search_term_->SetOpacity(search_term_opacity);
  }

  // ---------------------------------------------------------------------------
  // Panel Icon (Static)
  // ---------------------------------------------------------------------------
  if (should_use_static_icon) {
    if (panel_icon_->parent() != layer_) {
      layer_->AddChild(panel_icon_);
    }
    ui::ResourceManager::Resource* panel_icon_resource =
        resource_manager_->GetResource(ui::ANDROID_RESOURCE_TYPE_STATIC,
                                       panel_icon_resource_id);
    DCHECK(panel_icon_resource);

    // If the icon is not the default width, add or remove padding so it appears
    // centered.
    float icon_padding = (kDefaultIconWidthDp * dp_to_px -
        panel_icon_resource->size.width()) / 2.0f;

    // Positions the Icon at the start of the Search Bar.
    float search_provider_icon_left;
    if (is_rtl) {
      search_provider_icon_left = search_panel_width -
          panel_icon_resource->size.width() -
          (search_bar_margin_side + icon_padding);
    } else {
      search_provider_icon_left = search_bar_margin_side + icon_padding;
    }

    // Centers the Icon vertically in the Search Bar.
    float search_provider_icon_top = search_bar_top +
        search_bar_height / 2 -
        panel_icon_resource->size.height() / 2;

    panel_icon_->SetUIResourceId(
        panel_icon_resource->ui_resource->id());
    panel_icon_->SetBounds(panel_icon_resource->size);
    panel_icon_->SetPosition(
        gfx::PointF(search_provider_icon_left, search_provider_icon_top));
  }

  // ---------------------------------------------------------------------------
  // Search Provider Icon Sprite (Animated)
  // ---------------------------------------------------------------------------
  if (!should_use_static_icon) {
    if (search_provider_icon_sprite_visible) {
      if (search_provider_icon_sprite_->layer()->parent() != layer_) {
        layer_->AddChild(search_provider_icon_sprite_->layer());
      }

      search_provider_icon_sprite_->DrawSpriteFrame(
          resource_manager_,
          search_provider_icon_sprite_bitmap_resource_id,
          search_provider_icon_sprite_metadata_resource_id,
          search_provider_icon_sprite_completion_percentage);

      // Positions the Search Provider Icon at the start of the Search Bar.
      float icon_x;
      if (is_rtl) {
        icon_x = search_panel_width -
            search_provider_icon_sprite_->layer()->bounds().width() -
            search_bar_margin_side;
      } else {
        icon_x = search_bar_margin_side;
      }

      // Centers the Search Provider Icon vertically in the Search Bar.
      float icon_y = search_bar_top + search_bar_height / 2
          - search_provider_icon_sprite_->layer()->bounds().height() / 2;
      search_provider_icon_sprite_->layer()->SetPosition(
          gfx::PointF(icon_x, icon_y));
    } else {
      if (search_provider_icon_sprite_->layer().get() &&
          search_provider_icon_sprite_->layer()->parent()) {
        search_provider_icon_sprite_->layer()->RemoveFromParent();
      }
    }
  }

  // ---------------------------------------------------------------------------
  // Arrow Icon
  // ---------------------------------------------------------------------------
  // Grabs the Search Arrow Icon resource.
  ui::ResourceManager::Resource* arrow_icon_resource =
      resource_manager_->GetResource(ui::ANDROID_RESOURCE_TYPE_STATIC,
                                     arrow_up_resource_id);

  // Positions the icon at the end of the Search Bar.
  float arrow_icon_left;
  if (is_rtl) {
    arrow_icon_left = search_bar_margin_side;
  } else {
    arrow_icon_left = search_panel_width -
        arrow_icon_resource->size.width() - search_bar_margin_side;
  }

  // Centers the Arrow Icon vertically in the Search Bar.
  float arrow_icon_top = search_bar_top +
      search_bar_height / 2 -
      arrow_icon_resource->size.height() / 2;

  arrow_icon_->SetUIResourceId(arrow_icon_resource->ui_resource->id());
  arrow_icon_->SetBounds(arrow_icon_resource->size);
  arrow_icon_->SetPosition(
      gfx::PointF(arrow_icon_left, arrow_icon_top));
  arrow_icon_->SetOpacity(arrow_icon_opacity);

  gfx::Transform transform;
  if (arrow_icon_rotation != 0.f) {
    // Apply rotation about the center of the icon.
    float pivot_x = floor(arrow_icon_resource->size.width() / 2);
    float pivot_y = floor(arrow_icon_resource->size.height() / 2);
    gfx::PointF pivot_origin(pivot_x, pivot_y);
    transform.Translate(pivot_origin.x(), pivot_origin.y());
    transform.RotateAboutZAxis(arrow_icon_rotation);
    transform.Translate(-pivot_origin.x(), -pivot_origin.y());
  }
  arrow_icon_->SetTransform(transform);

  // ---------------------------------------------------------------------------
  // Close Icon
  // ---------------------------------------------------------------------------
  // Grab the Close Icon resource.
  ui::ResourceManager::Resource* close_icon_resource =
      resource_manager_->GetResource(ui::ANDROID_RESOURCE_TYPE_STATIC,
                                     close_icon_resource_id);

  // Positions the icon at the end of the Search Bar.
  float close_icon_left;
  if (is_rtl) {
    close_icon_left = search_bar_margin_side;
  } else {
    close_icon_left = search_panel_width -
        close_icon_resource->size.width() - search_bar_margin_side;
  }

  // Centers the Close Icon vertically in the Search Bar.
  float close_icon_top =
      search_bar_top +
      search_bar_height / 2 -
      close_icon_resource->size.height() / 2;

  close_icon_->SetUIResourceId(close_icon_resource->ui_resource->id());
  close_icon_->SetBounds(close_icon_resource->size);
  close_icon_->SetPosition(
      gfx::PointF(close_icon_left, close_icon_top));
  close_icon_->SetOpacity(close_icon_opacity);

  // ---------------------------------------------------------------------------
  // Search Promo
  // ---------------------------------------------------------------------------
  if (search_promo_visible) {
    // Grabs the Search Opt Out Promo resource.
    ui::ResourceManager::Resource* search_promo_resource =
        resource_manager_->GetResource(ui::ANDROID_RESOURCE_TYPE_DYNAMIC,
                                       search_promo_resource_id);
    // Search Promo Container
    if (search_promo_container_->parent() != layer_) {
      // NOTE(pedrosimonetti): The Promo layer should be always placed before
      // Search Bar Shadow to make sure it won't occlude the shadow.
      layer_->InsertChild(search_promo_container_, 0);
    }

    if (search_promo_resource) {
      int search_promo_content_height = search_promo_resource->size.height();
      gfx::Size search_promo_size(search_panel_width, search_promo_height);
      search_promo_container_->SetBounds(search_promo_size);
      search_promo_container_->SetPosition(gfx::PointF(0.f, search_bar_bottom));
      search_promo_container_->SetMasksToBounds(true);

      // Search Promo
      if (search_promo_->parent() != search_promo_container_)
        search_promo_container_->AddChild(search_promo_);

      search_promo_->SetUIResourceId(search_promo_resource->ui_resource->id());
      search_promo_->SetBounds(search_promo_resource->size);
      // Align promo at the bottom of the container so the confirmation button
      // is is not clipped when resizing the promo.
      search_promo_->SetPosition(
          gfx::PointF(0.f, search_promo_height - search_promo_content_height));
      search_promo_->SetOpacity(search_promo_opacity);
    }
  } else {
    // Search Promo Container
    if (search_promo_container_.get() && search_promo_container_->parent())
      search_promo_container_->RemoveFromParent();
  }

  // ---------------------------------------------------------------------------
  // Search Content View
  // ---------------------------------------------------------------------------
  content_view_container_->SetPosition(
      gfx::PointF(0.f, search_bar_bottom + search_promo_height));
  if (content_view_core && content_view_core->GetLayer().get()) {
    scoped_refptr<cc::Layer> content_view_layer = content_view_core->GetLayer();
    if (content_view_layer->parent() != content_view_container_)
      content_view_container_->AddChild(content_view_layer);
  } else {
    content_view_container_->RemoveAllChildren();
  }

  // ---------------------------------------------------------------------------
  // Search Bar Shadow
  // ---------------------------------------------------------------------------
  if (search_bar_shadow_visible) {
    ui::ResourceManager::Resource* search_bar_shadow_resource =
        resource_manager_->GetResource(ui::ANDROID_RESOURCE_TYPE_STATIC,
                                       search_bar_shadow_resource_id);

    if (search_bar_shadow_resource) {
      if (search_bar_shadow_->parent() != layer_)
        layer_->AddChild(search_bar_shadow_);

      int shadow_height = search_bar_shadow_resource->size.height();
      gfx::Size shadow_size(search_panel_width, shadow_height);

      search_bar_shadow_->SetUIResourceId(
          search_bar_shadow_resource->ui_resource->id());
      search_bar_shadow_->SetBounds(shadow_size);
      search_bar_shadow_->SetPosition(gfx::PointF(0.f, search_bar_bottom));
      search_bar_shadow_->SetOpacity(search_bar_shadow_opacity);
    }
  } else {
    if (search_bar_shadow_.get() && search_bar_shadow_->parent())
      search_bar_shadow_->RemoveFromParent();
  }

  // ---------------------------------------------------------------------------
  // Search Panel.
  // ---------------------------------------------------------------------------
  layer_->SetPosition(
      gfx::PointF(search_panel_x, search_panel_y));

  // ---------------------------------------------------------------------------
  // Progress Bar
  // ---------------------------------------------------------------------------
  bool should_render_progress_bar =
      progress_bar_visible && progress_bar_opacity > 0.f;
  if (should_render_progress_bar) {
    // Grabs Progress Bar resources.
    ui::ResourceManager::Resource* progress_bar_background_resource =
        resource_manager_->GetResource(ui::ANDROID_RESOURCE_TYPE_STATIC,
                                       progress_bar_background_resource_id);
    ui::ResourceManager::Resource* progress_bar_resource =
        resource_manager_->GetResource(ui::ANDROID_RESOURCE_TYPE_STATIC,
                                       progress_bar_resource_id);

    DCHECK(progress_bar_background_resource);
    DCHECK(progress_bar_resource);

    // Progress Bar Background
    if (progress_bar_background_->parent() != layer_)
      layer_->AddChild(progress_bar_background_);

    float progress_bar_y = search_bar_bottom - progress_bar_height;
    gfx::Size progress_bar_background_size(search_panel_width,
                                           progress_bar_height);

    progress_bar_background_->SetUIResourceId(
        progress_bar_background_resource->ui_resource->id());
    progress_bar_background_->SetBorder(
        progress_bar_background_resource->Border(progress_bar_background_size));
    progress_bar_background_->SetAperture(
        progress_bar_background_resource->aperture);
    progress_bar_background_->SetBounds(progress_bar_background_size);
    progress_bar_background_->SetPosition(gfx::PointF(0.f, progress_bar_y));
    progress_bar_background_->SetOpacity(progress_bar_opacity);

    // Progress Bar
    if (progress_bar_->parent() != layer_)
      layer_->AddChild(progress_bar_);

    float progress_bar_width =
        floor(search_panel_width * progress_bar_completion / 100.f);
    gfx::Size progress_bar_size(progress_bar_width, progress_bar_height);
    progress_bar_->SetUIResourceId(progress_bar_resource->ui_resource->id());
    progress_bar_->SetBorder(progress_bar_resource->Border(progress_bar_size));
    progress_bar_->SetAperture(progress_bar_resource->aperture);
    progress_bar_->SetBounds(progress_bar_size);
    progress_bar_->SetPosition(gfx::PointF(0.f, progress_bar_y));
    progress_bar_->SetOpacity(progress_bar_opacity);
  } else {
    // Removes Progress Bar and its Background from the Layer Tree.
    if (progress_bar_background_.get() && progress_bar_background_->parent())
      progress_bar_background_->RemoveFromParent();

    if (progress_bar_.get() && progress_bar_->parent())
      progress_bar_->RemoveFromParent();
  }

  // ---------------------------------------------------------------------------
  // Search Bar border.
  // ---------------------------------------------------------------------------
  if (!should_render_progress_bar && search_bar_border_visible) {
    gfx::Size search_bar_border_size(search_panel_width,
                                     search_bar_border_height);
    float border_y = search_bar_bottom - search_bar_border_height;
    search_bar_border_->SetBounds(search_bar_border_size);
    search_bar_border_->SetPosition(
        gfx::PointF(0.f, border_y));
    layer_->AddChild(search_bar_border_);
  } else if (search_bar_border_.get() && search_bar_border_->parent()) {
    search_bar_border_->RemoveFromParent();
  }
}

ContextualSearchLayer::ContextualSearchLayer(
    ui::ResourceManager* resource_manager)
    : resource_manager_(resource_manager),
      layer_(cc::Layer::Create(content::Compositor::LayerSettings())),
      panel_shadow_(
          cc::NinePatchLayer::Create(content::Compositor::LayerSettings())),
      search_bar_background_(
          cc::SolidColorLayer::Create(content::Compositor::LayerSettings())),
      search_context_(
          cc::UIResourceLayer::Create(content::Compositor::LayerSettings())),
      search_term_(
          cc::UIResourceLayer::Create(content::Compositor::LayerSettings())),
      search_bar_shadow_(
          cc::UIResourceLayer::Create(content::Compositor::LayerSettings())),
      panel_icon_(
          cc::UIResourceLayer::Create(content::Compositor::LayerSettings())),
      search_provider_icon_sprite_(CrushedSpriteLayer::Create()),
      arrow_icon_(
          cc::UIResourceLayer::Create(content::Compositor::LayerSettings())),
      close_icon_(
          cc::UIResourceLayer::Create(content::Compositor::LayerSettings())),
      content_view_container_(
          cc::Layer::Create(content::Compositor::LayerSettings())),
      search_bar_border_(
          cc::SolidColorLayer::Create(content::Compositor::LayerSettings())),
      progress_bar_(
          cc::NinePatchLayer::Create(content::Compositor::LayerSettings())),
      progress_bar_background_(
          cc::NinePatchLayer::Create(content::Compositor::LayerSettings())),
      search_promo_(
          cc::UIResourceLayer::Create(content::Compositor::LayerSettings())),
      search_promo_container_(
          cc::SolidColorLayer::Create(content::Compositor::LayerSettings())),
      peek_promo_container_(
          cc::SolidColorLayer::Create(content::Compositor::LayerSettings())),
      peek_promo_ripple_(
          cc::NinePatchLayer::Create(content::Compositor::LayerSettings())),
      peek_promo_text_(
          cc::UIResourceLayer::Create(content::Compositor::LayerSettings())) {
  layer_->SetMasksToBounds(false);
  layer_->SetIsDrawable(true);

  // Panel Shadow
  panel_shadow_->SetIsDrawable(true);
  panel_shadow_->SetFillCenter(false);
  layer_->AddChild(panel_shadow_);

  // Search Peek Promo
  peek_promo_container_->SetIsDrawable(true);
  peek_promo_container_->SetBackgroundColor(kSearchBarBackgroundColor);
  peek_promo_ripple_->SetIsDrawable(true);
  peek_promo_ripple_->SetFillCenter(true);
  peek_promo_text_->SetIsDrawable(true);
  peek_promo_container_->AddChild(peek_promo_ripple_);
  peek_promo_container_->AddChild(peek_promo_text_);

  // Search Bar Background
  search_bar_background_->SetIsDrawable(true);
  search_bar_background_->SetBackgroundColor(kSearchBarBackgroundColor);
  layer_->AddChild(search_bar_background_);

  // Search Bar Text
  search_context_->SetIsDrawable(true);
  layer_->AddChild(search_context_);
  search_term_->SetIsDrawable(true);
  layer_->AddChild(search_term_);

  // Panel Icon
  panel_icon_->SetIsDrawable(true);

  // Arrow Icon
  arrow_icon_->SetIsDrawable(true);
  layer_->AddChild(arrow_icon_);

  // Close Icon
  close_icon_->SetIsDrawable(true);
  layer_->AddChild(close_icon_);

  // Search Opt Out Promo
  search_promo_container_->SetIsDrawable(true);
  search_promo_container_->SetBackgroundColor(kSearchBackgroundColor);
  search_promo_->SetIsDrawable(true);

  // Search Bar Border
  search_bar_border_->SetIsDrawable(true);
  search_bar_border_->SetBackgroundColor(kSearchBarBorderColor);

  // Progress Bar Background
  progress_bar_background_->SetIsDrawable(true);
  progress_bar_background_->SetFillCenter(true);

  // Progress Bar
  progress_bar_->SetIsDrawable(true);
  progress_bar_->SetFillCenter(true);

  // Search Content View Container
  layer_->AddChild(content_view_container_);

  // Search Bar Shadow
  search_bar_shadow_->SetIsDrawable(true);
}

ContextualSearchLayer::~ContextualSearchLayer() {
}

scoped_refptr<cc::Layer> ContextualSearchLayer::layer() {
  return layer_;
}

}  //  namespace android
}  //  namespace chrome
