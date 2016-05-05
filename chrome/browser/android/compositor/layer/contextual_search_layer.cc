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
const SkColor kPeekPromoRippleBackgroundColor = SkColorSetRGB(0x42, 0x85, 0xF4);

// The alpha blend used in the Peek Promo Background in order to achieve
// a lighter shade of the color of the Peek Promo Ripple.
const SkAlpha kPeekPromoBackgroundMaximumAlphaBlend = 0.25f * 255;

}  // namespace

namespace chrome {
namespace android {

// static
scoped_refptr<ContextualSearchLayer> ContextualSearchLayer::Create(
    ui::ResourceManager* resource_manager) {
  return make_scoped_refptr(new ContextualSearchLayer(resource_manager));
}

scoped_refptr<cc::Layer> ContextualSearchLayer::GetIconLayer() {
  // Search Provider Icon Sprite (Animated)
  if (search_provider_icon_sprite_visible_) {
    if (search_provider_icon_sprite_->layer()->parent() != layer_) {
      layer_->AddChild(search_provider_icon_sprite_->layer().get());
    }
    search_provider_icon_sprite_->DrawSpriteFrame(
        resource_manager_,
        panel_icon_resource_id_,
        search_provider_icon_sprite_metadata_resource_id_,
        search_provider_icon_sprite_completion_percentage_);
  } else {
    if (search_provider_icon_sprite_->layer().get() &&
        search_provider_icon_sprite_->layer()->parent()) {
      search_provider_icon_sprite_->layer()->RemoveFromParent();
    }
    return nullptr;
  }

  return search_provider_icon_sprite_->layer();
}

void ContextualSearchLayer::SetProperties(
    int panel_shadow_resource_id,
    int search_context_resource_id,
    int search_term_resource_id,
    int search_caption_resource_id,
    int search_bar_shadow_resource_id,
    int panel_icon_resource_id,
    int search_provider_icon_sprite_metadata_resource_id,
    int arrow_up_resource_id,
    int close_icon_resource_id,
    int progress_bar_background_resource_id,
    int progress_bar_resource_id,
    int search_promo_resource_id,
    int peek_promo_ripple_resource_id,
    int peek_promo_text_resource_id,
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
    float search_caption_opacity,
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

  search_provider_icon_sprite_visible_ = search_provider_icon_sprite_visible;
  search_provider_icon_sprite_metadata_resource_id_ =
      search_provider_icon_sprite_metadata_resource_id;
  search_provider_icon_sprite_completion_percentage_ =
      search_provider_icon_sprite_completion_percentage;

  // Grabs the dynamic Search Context resource.
  ui::ResourceManager::Resource* search_context_resource =
      resource_manager_->GetResource(ui::ANDROID_RESOURCE_TYPE_DYNAMIC,
                                     search_context_resource_id);

  // Round values to avoid pixel gap between layers.
  search_bar_height = floor(search_bar_height);

  float search_bar_top = search_peek_promo_height;
  float search_bar_bottom = search_bar_top + search_bar_height;
  bool should_render_progress_bar =
      progress_bar_visible && progress_bar_opacity > 0.f;

  OverlayPanelLayer::SetResourceIds(
      search_term_resource_id,
      panel_shadow_resource_id,
      search_bar_shadow_resource_id,
      panel_icon_resource_id,
      close_icon_resource_id);

  float content_view_top = search_bar_bottom + search_promo_height;
  float should_render_bar_border = search_bar_border_visible
      && !should_render_progress_bar;

  // -----------------------------------------------------------------
  // Overlay Panel
  // -----------------------------------------------------------------
  OverlayPanelLayer::SetProperties(
      dp_to_px,
      content_view_core,
      content_view_top,
      search_panel_x,
      search_panel_y,
      search_panel_width,
      search_panel_height,
      search_bar_margin_side,
      search_bar_height,
      search_bar_top,
      search_term_opacity,
      should_render_bar_border,
      search_bar_border_height,
      search_bar_shadow_visible,
      search_bar_shadow_opacity,
      close_icon_opacity);

  bool is_rtl = l10n_util::IsLayoutRtl();

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
  // Search Context
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

  // -----------------------------------------------------------------
  // Search Caption Text
  // -----------------------------------------------------------------
  if (search_caption_opacity != 0.f && search_caption_.get()) {
    if (search_caption_->parent() != text_container_) {
      AddBarTextLayer(search_caption_);
    }
    // Grabs the dynamic Search Caption resource.
    ui::ResourceManager::Resource* search_caption_resource =
        resource_manager_->GetResource(ui::ANDROID_RESOURCE_TYPE_DYNAMIC,
                                       search_caption_resource_id);
    // Calculate position of the Caption, and the main bar text.
    // Without a caption the bar text is not moved from it's default centered
    // position. When there is a Caption interpolate its position between the
    // default and adjusted (moved up by the size of the caption and margin).
    float bar_text_height = bar_text_->bounds().height();
    float search_caption_height = search_caption_resource->size.height();
    float text_margin = floor(
        (search_bar_height - bar_text_height - search_caption_height) / 5);
    float search_caption_top =
        search_bar_top + bar_text_height + text_margin * 2;
    // Get the current centered position set up by the OverlayPanelLayer.
    float bar_text_top_centered = bar_text_->position().y();
    float bar_text_adjust = search_caption_height + text_margin;
    float bar_text_top = bar_text_top_centered -
                            bar_text_adjust * search_caption_opacity / 2;
    // Move the main bar text up.
    bar_text_->SetPosition(gfx::PointF(0.f, bar_text_top));
    // Add the caption
    search_caption_->SetUIResourceId(
        search_caption_resource->ui_resource->id());
    search_caption_->SetBounds(search_caption_resource->size);
    search_caption_->SetPosition(gfx::PointF(0.f, search_caption_top));
    search_caption_->SetOpacity(search_caption_opacity);
  } else if (search_caption_.get() && search_caption_->parent()) {
    search_caption_->RemoveFromParent();
  }

  // ---------------------------------------------------------------------------
  // Arrow Icon
  // ---------------------------------------------------------------------------
  // Grabs the arrow icon resource.
  ui::ResourceManager::Resource* arrow_icon_resource =
      resource_manager_->GetResource(ui::ANDROID_RESOURCE_TYPE_STATIC,
                                     arrow_up_resource_id);

  // Positions the icon at the end of the bar.
  float arrow_icon_left;
  if (is_rtl) {
    arrow_icon_left = search_bar_margin_side;
  } else {
    arrow_icon_left = search_panel_width - arrow_icon_resource->size.width()
        - search_bar_margin_side;
  }

  // Centers the Arrow Icon vertically in the bar.
  float arrow_icon_top = search_bar_top + search_bar_height / 2 -
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
  // Progress Bar
  // ---------------------------------------------------------------------------
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
}

ContextualSearchLayer::ContextualSearchLayer(
    ui::ResourceManager* resource_manager)
    : OverlayPanelLayer(resource_manager),
      search_context_(cc::UIResourceLayer::Create()),
      search_provider_icon_sprite_(CrushedSpriteLayer::Create()),
      arrow_icon_(cc::UIResourceLayer::Create()),
      search_promo_(cc::UIResourceLayer::Create()),
      search_promo_container_(cc::SolidColorLayer::Create()),
      peek_promo_container_(cc::SolidColorLayer::Create()),
      peek_promo_ripple_(cc::NinePatchLayer::Create()),
      peek_promo_text_(cc::UIResourceLayer::Create()),
      progress_bar_(cc::NinePatchLayer::Create()),
      progress_bar_background_(cc::NinePatchLayer::Create()),
      search_caption_(cc::UIResourceLayer::Create()) {
  // Search Peek Promo
  peek_promo_container_->SetIsDrawable(true);
  peek_promo_container_->SetBackgroundColor(kSearchBarBackgroundColor);
  peek_promo_ripple_->SetIsDrawable(true);
  peek_promo_ripple_->SetFillCenter(true);
  peek_promo_text_->SetIsDrawable(true);
  peek_promo_container_->AddChild(peek_promo_ripple_);
  peek_promo_container_->AddChild(peek_promo_text_);

  // Search Bar Text
  search_context_->SetIsDrawable(true);
  // NOTE(mdjones): This can be called multiple times to add other text layers.
  AddBarTextLayer(search_context_);

  // Search Bar Caption
  search_caption_->SetIsDrawable(true);

  // Arrow Icon
  arrow_icon_->SetIsDrawable(true);
  layer_->AddChild(arrow_icon_);

  // Search Opt Out Promo
  search_promo_container_->SetIsDrawable(true);
  search_promo_container_->SetBackgroundColor(kSearchBackgroundColor);
  search_promo_->SetIsDrawable(true);

  // Progress Bar Background
  progress_bar_background_->SetIsDrawable(true);
  progress_bar_background_->SetFillCenter(true);

  // Progress Bar
  progress_bar_->SetIsDrawable(true);
  progress_bar_->SetFillCenter(true);
}

ContextualSearchLayer::~ContextualSearchLayer() {
}

}  //  namespace android
}  //  namespace chrome
