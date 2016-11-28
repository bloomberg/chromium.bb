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
#include "third_party/skia/include/core/SkColor.h"
#include "ui/android/resources/crushed_sprite_resource.h"
#include "ui/android/resources/resource_manager.h"
#include "ui/base/l10n/l10n_util_android.h"
#include "ui/gfx/color_utils.h"

namespace {

const SkColor kSearchBackgroundColor = SkColorSetRGB(0xee, 0xee, 0xee);
const SkColor kSearchBarBackgroundColor = SkColorSetRGB(0xff, 0xff, 0xff);
const SkColor kPeekPromoRippleBackgroundColor = SkColorSetRGB(0x42, 0x85, 0xF4);
const SkColor kTouchHighlightColor = SkColorSetARGB(0x33, 0x99, 0x99, 0x99);

// The alpha blend used in the Peek Promo Background in order to achieve
// a lighter shade of the color of the Peek Promo Ripple.
const SkAlpha kPeekPromoBackgroundMaximumAlphaBlend = 0.25f * 255;

}  // namespace

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
    int search_caption_resource_id,
    int search_bar_shadow_resource_id,
    int sprite_resource_id,
    int search_provider_icon_sprite_metadata_resource_id,
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
    bool search_provider_icon_sprite_visible,
    float search_provider_icon_sprite_completion_percentage,
    bool quick_action_icon_visible,
    bool thumbnail_visible,
    float static_image_visibility_percentage,
    int static_image_size,
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
    float touch_highlight_width) {

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
      sprite_resource_id,
      close_icon_resource_id);

  float content_view_top = search_bar_bottom + search_promo_height;
  float should_render_bar_border = search_bar_border_visible
      && !should_render_progress_bar;

  // -----------------------------------------------------------------
  // Overlay Panel
  // -----------------------------------------------------------------
  OverlayPanelLayer::SetProperties(
      dp_to_px,
      content_layer,
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
  // Search Term, Context and Search Caption
  // ---------------------------------------------------------------------------
  SetupTextLayer(
      search_bar_top,
      search_bar_height,
      search_text_layer_min_height,
      search_caption_resource_id,
      search_caption_visible,
      search_caption_animation_percentage,
      search_term_opacity,
      search_context_resource_id,
      search_context_opacity,
      search_term_caption_spacing);

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

  // ---------------------------------------------------------------------------
  // Divider Line
  // ---------------------------------------------------------------------------
  if (divider_line_visibility_percentage > 0.f) {
    if (divider_line_->parent() != layer_)
      layer_->AddChild(divider_line_);

    // The divider line animates in from the bottom.
    float divider_line_y_offset =
        ((search_bar_height - divider_line_height) / 2) +
        (divider_line_height * (1.f - divider_line_visibility_percentage));
    divider_line_->SetPosition(gfx::PointF(divider_line_x_offset,
                                           divider_line_y_offset));

    // The divider line should not draw below its final resting place.
    // Set bounds to restrict the vertical draw position.
    divider_line_->SetBounds(
        gfx::Size(divider_line_width,
                  divider_line_height * divider_line_visibility_percentage));
    divider_line_->SetBackgroundColor(divider_line_color);
  } else if (divider_line_->parent()) {
    divider_line_->RemoveFromParent();
  }

  // ---------------------------------------------------------------------------
  // Touch Highlight Layer
  // ---------------------------------------------------------------------------
  if (touch_highlight_visible) {
    if (touch_highlight_layer_->parent() != layer_)
      layer_->AddChild(touch_highlight_layer_);
    gfx::Size background_size(touch_highlight_width, search_bar_height);
    touch_highlight_layer_->SetBounds(background_size);
    touch_highlight_layer_->SetPosition(gfx::PointF(
        touch_highlight_x_offset, search_bar_top));
  } else {
    touch_highlight_layer_->RemoveFromParent();
  }

  // ---------------------------------------------------------------------------
  // Icon Layer
  // ---------------------------------------------------------------------------
  static_image_size_ = static_image_size;
  SetupIconLayer(search_provider_icon_sprite_visible,
                 search_provider_icon_sprite_metadata_resource_id,
                 search_provider_icon_sprite_completion_percentage,
                 quick_action_icon_visible,
                 quick_action_icon_resource_id,
                 thumbnail_visible,
                 static_image_visibility_percentage);
}

scoped_refptr<cc::Layer> ContextualSearchLayer::GetIconLayer() {
  return icon_layer_;
}

void ContextualSearchLayer::SetupIconLayer(
    bool search_provider_icon_sprite_visible,
    int search_provider_icon_sprite_metadata_resource_id,
    float search_provider_icon_sprite_completion_percentage,
    bool quick_action_icon_visible,
    int quick_action_icon_resource_id,
    bool thumbnail_visible,
    float static_image_visibility_percentage) {
  icon_layer_->SetBounds(gfx::Size(static_image_size_, static_image_size_));
  icon_layer_->SetMasksToBounds(true);

  scoped_refptr<cc::UIResourceLayer> static_image_layer;

  if (quick_action_icon_visible) {
    if (quick_action_icon_layer_->parent() != icon_layer_)
      icon_layer_->AddChild(quick_action_icon_layer_);

    ui::ResourceManager::Resource* quick_action_icon_resource =
        resource_manager_->GetResource(ui::ANDROID_RESOURCE_TYPE_DYNAMIC,
                                       quick_action_icon_resource_id);
    if (quick_action_icon_resource) {
      quick_action_icon_layer_->SetUIResourceId(
          quick_action_icon_resource->ui_resource->id());
      quick_action_icon_layer_->SetBounds(gfx::Size(static_image_size_,
                                                    static_image_size_));

      SetStaticImageProperties(quick_action_icon_layer_, 0, 0,
                               static_image_visibility_percentage);
    }
  } else if (quick_action_icon_layer_->parent()) {
    quick_action_icon_layer_->RemoveFromParent();
  }

  // Thumbnail
  if (!quick_action_icon_visible && thumbnail_visible) {
    if (thumbnail_layer_->parent() != icon_layer_)
          icon_layer_->AddChild(thumbnail_layer_);

    SetStaticImageProperties(thumbnail_layer_,
                             thumbnail_top_margin_,
                             thumbnail_side_margin_,
                             static_image_visibility_percentage);
  } else if (thumbnail_layer_->parent()) {
    thumbnail_layer_->RemoveFromParent();
  }

  // Search Provider Icon Sprite
  if (search_provider_icon_sprite_visible) {
    if (search_provider_icon_sprite_->layer()->parent() != icon_layer_)
      icon_layer_->AddChild(search_provider_icon_sprite_->layer().get());

    search_provider_icon_sprite_->DrawSpriteFrame(
        resource_manager_,
        panel_icon_resource_id_,
        search_provider_icon_sprite_metadata_resource_id,
        search_provider_icon_sprite_completion_percentage);

    search_provider_icon_sprite_->layer()->SetOpacity(
        1.f - static_image_visibility_percentage);

    float icon_y_offset =
        -(static_image_size_ * static_image_visibility_percentage);
    search_provider_icon_sprite_->layer()->SetPosition(
        gfx::PointF(0.f, icon_y_offset));

  } else if (search_provider_icon_sprite_->layer().get() &&
      search_provider_icon_sprite_->layer()->parent()) {
    search_provider_icon_sprite_->layer()->RemoveFromParent();
  }
}

void ContextualSearchLayer::SetStaticImageProperties(
    scoped_refptr<cc::UIResourceLayer> static_image_layer,
    float top_margin,
    float side_margin,
    float visibility_percentage) {
  static_image_layer->SetOpacity(visibility_percentage);

  // When animating, the static image and icon sprite slide through
  // |icon_layer_|. This effect is achieved by changing the y-offset
  // for each child layer.
  // If the static image has a height less than |static_image_size_|, it will
  // have a top margin that needs to be accounted for while running the
  // animation. The final |static_image_y_offset| should be equal to
  // |tpp_margin|.
  float static_image_y_offset =
      (static_image_size_ * (1.f - visibility_percentage))
      + top_margin;
  static_image_layer->SetPosition(
      gfx::PointF(side_margin, static_image_y_offset));
}

void ContextualSearchLayer::SetupTextLayer(float bar_top,
                                           float bar_height,
                                           float search_text_layer_min_height,
                                           int caption_resource_id,
                                           bool caption_visible,
                                           float animation_percentage,
                                           float search_term_opacity,
                                           int context_resource_id,
                                           float context_opacity,
                                           float term_caption_spacing) {
  // ---------------------------------------------------------------------------
  // Setup the Drawing Hierarchy
  // ---------------------------------------------------------------------------
  // Search Term
  bool bar_text_visible = search_term_opacity > 0.0f;
  if (bar_text_visible && bar_text_->parent() != text_layer_)
    text_layer_->AddChild(bar_text_);

  // Search Context
  ui::ResourceManager::Resource* context_resource =
      resource_manager_->GetResource(
          ui::ANDROID_RESOURCE_TYPE_DYNAMIC, context_resource_id);
  if (context_resource) {
    search_context_->SetUIResourceId(context_resource->ui_resource->id());
    search_context_->SetBounds(context_resource->size);
  }

  // Search Caption
  ui::ResourceManager::Resource* caption_resource = nullptr;
  if (caption_visible) {
    // Grabs the dynamic Search Caption resource so we can get a snapshot.
    caption_resource  = resource_manager_->GetResource(
        ui::ANDROID_RESOURCE_TYPE_DYNAMIC, caption_resource_id);
  }

  if (caption_visible && animation_percentage != 0.f) {
    if (search_caption_->parent() != text_layer_) {
      text_layer_->AddChild(search_caption_);
    }
    if (caption_resource) {
      search_caption_->SetUIResourceId(caption_resource->ui_resource->id());
      search_caption_->SetBounds(caption_resource->size);
    }
  } else if (search_caption_->parent()) {
    search_caption_->RemoveFromParent();
  }

  // ---------------------------------------------------------------------------
  // Calculate Text Layer Size
  // ---------------------------------------------------------------------------
  // If space allows, the Term, Context and Caption should occupy a Text
  // section of fixed size.
  // We may not be able to fit these inside the ideal size as the user may have
  // their Font Size set to large.

  // The Term might not be visible or initialized yet, so set up main_text with
  // whichever main bar text seems appropriate.
  scoped_refptr<cc::UIResourceLayer> main_text =
      (bar_text_visible ? bar_text_ : search_context_);

  // The search_caption_ may not have had it's resource set by this point, if so
  // the bounds will be zero and everything will still work.
  float term_height = main_text->bounds().height();
  float caption_height = search_caption_->bounds().height();

  float layer_height = std::max(search_text_layer_min_height,
      term_height + caption_height + term_caption_spacing);
  float layer_width =
      std::max(main_text->bounds().width(), search_caption_->bounds().width());

  float layer_top = bar_top + (bar_height - layer_height) / 2;
  text_layer_->SetBounds(gfx::Size(layer_width, layer_height));
  text_layer_->SetPosition(gfx::PointF(0.f, layer_top));
  text_layer_->SetMasksToBounds(true);

  // ---------------------------------------------------------------------------
  // Layout Text Layer
  // ---------------------------------------------------------------------------
  // ---Top of Search Bar--- <- bar_top
  //
  // ---Top of Text Layer--- <- layer_top
  //                         } remaining_height / 2
  // Term & Context          } term_height
  //                         } term_caption_spacing
  // Caption                 } caption_height
  //                         } remaining_height / 2
  // --Bottom of Text Layer-
  //
  // --Bottom of Search Bar-
  // If the Caption is not visible the Term is centered in this space, when
  // the Caption becomes visible it is animated sliding up into it's position
  // with the spacings determined by UI.
  // The Term and the Context are assumed to be the same height and will be
  // positioned one on top of the other. When the Context is resolved it will
  // fade out and the Term will fade in.

  search_context_->SetOpacity(context_opacity);
  bar_text_->SetOpacity(1.f - context_opacity);

  // If there is no caption, just vertically center the Search Term.
  float term_top = (layer_height - term_height) / 2;

  // If we aren't displaying the caption we're done.
  if (!caption_visible || animation_percentage == 0.f || !caption_resource) {
    bar_text_->SetPosition(gfx::PointF(0.f, term_top));
    search_context_->SetPosition(gfx::PointF(0.f, term_top));
    return;
  }

  // Calculate the positions for the Term and Caption when the Caption
  // animation is complete.
  float remaining_height = layer_height
                         - term_height
                         - term_caption_spacing
                         - caption_height;

  float term_top_end = remaining_height / 2;
  float caption_top_end = term_top_end + term_height + term_caption_spacing;

  // Interpolate between the animation start and end positions (short cut
  // if the animation is at the end or start).
  term_top = term_top * (1.f - animation_percentage)
           + term_top_end * animation_percentage;

  // The Caption starts off the bottom of the Text Layer.
  float caption_top = layer_height * (1.f - animation_percentage)
                    + caption_top_end * animation_percentage;

  bar_text_->SetPosition(gfx::PointF(0.f, term_top));
  search_context_->SetPosition(gfx::PointF(0.f, term_top));
  search_caption_->SetPosition(gfx::PointF(0.f, caption_top));
}

void ContextualSearchLayer::SetThumbnail(const SkBitmap* thumbnail) {
  // Determine the scaled thumbnail width and height. If both the height and
  // width of |thumbnail| are larger than |static_image_size_|, the thumbnail
  // will be scaled down by a call to Layer::SetBounds() below.
  int min_dimension = std::min(thumbnail->width(), thumbnail->height());
  int scaled_thumbnail_width = thumbnail->width();
  int scaled_thumbnail_height = thumbnail->height();
  if (min_dimension > static_image_size_) {
    scaled_thumbnail_width =
        scaled_thumbnail_width * static_image_size_ / min_dimension;
    scaled_thumbnail_height =
        scaled_thumbnail_height * static_image_size_ / min_dimension;
  }

  // Determine the UV transform coordinates. This will crop the thumbnail.
  // (0, 0) is the default top left corner. (1, 1) is the default bottom
  // right corner.
  float top_left_x = 0;
  float top_left_y = 0;
  float bottom_right_x = 1;
  float bottom_right_y = 1;

  if (scaled_thumbnail_width > static_image_size_) {
    // Crop an even amount on the left and right sides of the thumbnail.
    float top_left_x_px = (scaled_thumbnail_width - static_image_size_) / 2.f;
    float bottom_right_x_px = top_left_x_px + static_image_size_;

    top_left_x = top_left_x_px / scaled_thumbnail_width;
    bottom_right_x = bottom_right_x_px / scaled_thumbnail_width;
  } else if (scaled_thumbnail_height > static_image_size_) {
    // Crop an even amount on the top and bottom of the thumbnail.
    float top_left_y_px = (scaled_thumbnail_height - static_image_size_) / 2.f;
    float bottom_right_y_px = top_left_y_px + static_image_size_;

    top_left_y = top_left_y_px / scaled_thumbnail_height;
    bottom_right_y = bottom_right_y_px / scaled_thumbnail_height;
  }

  // If the original |thumbnail| height or width is smaller than
  // |static_image_size_| determine the side and top margins needed to center
  // the thumbnail.
  thumbnail_side_margin_ = 0;
  thumbnail_top_margin_ = 0;

  if (scaled_thumbnail_width < static_image_size_) {
    thumbnail_side_margin_ =
        (static_image_size_ - scaled_thumbnail_width) / 2.f;
  }

  if (scaled_thumbnail_height < static_image_size_) {
    thumbnail_top_margin_ =
        (static_image_size_ - scaled_thumbnail_height) / 2.f;
  }

  // Determine the layer bounds. This will down scale the thumbnail if
  // necessary and ensure it is displayed at |static_image_size_|. If
  // either the original |thumbnail| height or width is smaller than
  // |static_image_size_|, the thumbnail will not be scaled.
  int layer_width = std::min(static_image_size_, scaled_thumbnail_width);
  int layer_height = std::min(static_image_size_, scaled_thumbnail_height);

  // UIResourceLayer requires an immutable copy of the input |thumbnail|.
  SkBitmap thumbnail_copy;
  if (thumbnail->isImmutable()) {
    thumbnail_copy = *thumbnail;
  } else {
    thumbnail->copyTo(&thumbnail_copy);
    thumbnail_copy.setImmutable();
  }

  thumbnail_layer_->SetBitmap(thumbnail_copy);
  thumbnail_layer_->SetBounds(gfx::Size(layer_width, layer_height));
  thumbnail_layer_->SetPosition(
      gfx::PointF(thumbnail_side_margin_, thumbnail_top_margin_));
  thumbnail_layer_->SetUV(gfx::PointF(top_left_x, top_left_y),
                          gfx::PointF(bottom_right_x, bottom_right_y));
}

ContextualSearchLayer::ContextualSearchLayer(
    ui::ResourceManager* resource_manager)
    : OverlayPanelLayer(resource_manager),
      search_context_(cc::UIResourceLayer::Create()),
      icon_layer_(cc::Layer::Create()),
      search_provider_icon_sprite_(CrushedSpriteLayer::Create()),
      thumbnail_layer_(cc::UIResourceLayer::Create()),
      quick_action_icon_layer_(cc::UIResourceLayer::Create()),
      arrow_icon_(cc::UIResourceLayer::Create()),
      search_promo_(cc::UIResourceLayer::Create()),
      search_promo_container_(cc::SolidColorLayer::Create()),
      peek_promo_container_(cc::SolidColorLayer::Create()),
      peek_promo_ripple_(cc::NinePatchLayer::Create()),
      peek_promo_text_(cc::UIResourceLayer::Create()),
      progress_bar_(cc::NinePatchLayer::Create()),
      progress_bar_background_(cc::NinePatchLayer::Create()),
      search_caption_(cc::UIResourceLayer::Create()),
      text_layer_(cc::UIResourceLayer::Create()),
      divider_line_(cc::SolidColorLayer::Create()),
      touch_highlight_layer_(cc::SolidColorLayer::Create()) {
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

  // Icon - holds thumbnail, search provider sprite and/or quick action icon
  icon_layer_->SetIsDrawable(true);
  layer_->AddChild(icon_layer_);

  // Thumbnail
  thumbnail_layer_->SetIsDrawable(true);

  // Quick action icon
  quick_action_icon_layer_->SetIsDrawable(true);

  // Divider line
  divider_line_->SetIsDrawable(true);

  // Content layer
  text_layer_->SetIsDrawable(true);
  // NOTE(mdjones): This can be called multiple times to add other text layers.
  AddBarTextLayer(text_layer_);
  text_layer_->AddChild(search_context_);

  // Touch Highlight Layer
  touch_highlight_layer_->SetIsDrawable(true);
  touch_highlight_layer_->SetBackgroundColor(kTouchHighlightColor);
}

ContextualSearchLayer::~ContextualSearchLayer() {
}

}  //  namespace android
