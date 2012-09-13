// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/shadow.h"

#include "ash/wm/image_grid.h"
#include "grit/ash_resources.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/compositor/scoped_layer_animation_settings.h"

namespace {

// Shadow opacity for different styles.
const float kActiveShadowOpacity = 1.0f;
const float kInactiveShadowOpacity = 0.2f;
const float kSmallShadowOpacity = 1.0f;

// Duration for opacity animation in milliseconds.
const int64 kAnimationDurationMs = 200;

float GetOpacityForStyle(ash::internal::Shadow::Style style) {
  switch (style) {
    case ash::internal::Shadow::STYLE_ACTIVE:
      return kActiveShadowOpacity;
    case ash::internal::Shadow::STYLE_INACTIVE:
      return kInactiveShadowOpacity;
    case ash::internal::Shadow::STYLE_SMALL:
      return kSmallShadowOpacity;
    default:
      NOTREACHED() << "Unhandled style " << style;
  }
  return 1.0f;
}

}  // namespace

namespace ash {
namespace internal {

Shadow::Shadow() : style_(STYLE_ACTIVE) {
}

Shadow::~Shadow() {
}

void Shadow::Init(Style style) {
  style_ = style;
  image_grid_.reset(new ImageGrid);
  UpdateImagesForStyle();
  image_grid_->layer()->set_name("Shadow");
  image_grid_->layer()->SetOpacity(GetOpacityForStyle(style_));
}

void Shadow::SetContentBounds(const gfx::Rect& content_bounds) {
  content_bounds_ = content_bounds;
  UpdateImageGridBounds();
}

ui::Layer* Shadow::layer() const {
  return image_grid_->layer();
}

void Shadow::SetStyle(Style style) {
  if (style_ == style)
    return;

  Style old_style = style_;
  style_ = style;

  // Stop waiting for any as yet unfinished implicit animations.
  StopObservingImplicitAnimations();

  // If we're switching to or from the small style, don't bother with
  // animations.
  if (style == STYLE_SMALL || old_style == STYLE_SMALL) {
    UpdateImagesForStyle();
    image_grid_->layer()->SetOpacity(GetOpacityForStyle(style));
    return;
  }

  // If we're becoming active, switch images now.  Because the inactive image
  // has a very low opacity the switch isn't noticeable and this approach
  // allows us to use only a single set of shadow images at a time.
  if (style == STYLE_ACTIVE) {
    UpdateImagesForStyle();
    // Opacity was baked into inactive image, start opacity low to match.
    image_grid_->layer()->SetOpacity(kInactiveShadowOpacity);
  }

  {
    // Property sets within this scope will be implicitly animated.
    ui::ScopedLayerAnimationSettings settings(layer()->GetAnimator());
    settings.AddObserver(this);
    settings.SetTransitionDuration(
        base::TimeDelta::FromMilliseconds(kAnimationDurationMs));
    switch (style_) {
      case STYLE_ACTIVE:
        image_grid_->layer()->SetOpacity(kActiveShadowOpacity);
        break;
      case STYLE_INACTIVE:
        image_grid_->layer()->SetOpacity(kInactiveShadowOpacity);
        break;
      default:
        NOTREACHED() << "Unhandled style " << style_;
        break;
    }
  }
}

void Shadow::OnImplicitAnimationsCompleted() {
  // If we just finished going inactive, switch images.  This doesn't cause
  // a visual pop because the inactive image opacity is so low.
  if (style_ == STYLE_INACTIVE) {
    UpdateImagesForStyle();
    // Opacity is baked into inactive image, so set fully opaque.
    image_grid_->layer()->SetOpacity(1.0f);
  }
}

void Shadow::UpdateImagesForStyle() {
  ResourceBundle& res = ResourceBundle::GetSharedInstance();
  switch (style_) {
    case STYLE_ACTIVE:
      image_grid_->SetImages(
          &res.GetImageNamed(IDR_AURA_SHADOW_ACTIVE_TOP_LEFT),
          &res.GetImageNamed(IDR_AURA_SHADOW_ACTIVE_TOP),
          &res.GetImageNamed(IDR_AURA_SHADOW_ACTIVE_TOP_RIGHT),
          &res.GetImageNamed(IDR_AURA_SHADOW_ACTIVE_LEFT),
          NULL,
          &res.GetImageNamed(IDR_AURA_SHADOW_ACTIVE_RIGHT),
          &res.GetImageNamed(IDR_AURA_SHADOW_ACTIVE_BOTTOM_LEFT),
          &res.GetImageNamed(IDR_AURA_SHADOW_ACTIVE_BOTTOM),
          &res.GetImageNamed(IDR_AURA_SHADOW_ACTIVE_BOTTOM_RIGHT));
      break;
    case STYLE_INACTIVE:
      image_grid_->SetImages(
          &res.GetImageNamed(IDR_AURA_SHADOW_INACTIVE_TOP_LEFT),
          &res.GetImageNamed(IDR_AURA_SHADOW_INACTIVE_TOP),
          &res.GetImageNamed(IDR_AURA_SHADOW_INACTIVE_TOP_RIGHT),
          &res.GetImageNamed(IDR_AURA_SHADOW_INACTIVE_LEFT),
          NULL,
          &res.GetImageNamed(IDR_AURA_SHADOW_INACTIVE_RIGHT),
          &res.GetImageNamed(IDR_AURA_SHADOW_INACTIVE_BOTTOM_LEFT),
          &res.GetImageNamed(IDR_AURA_SHADOW_INACTIVE_BOTTOM),
          &res.GetImageNamed(IDR_AURA_SHADOW_INACTIVE_BOTTOM_RIGHT));
      break;
    case STYLE_SMALL:
      image_grid_->SetImages(
          &res.GetImageNamed(IDR_AURA_SHADOW_SMALL_TOP_LEFT),
          &res.GetImageNamed(IDR_AURA_SHADOW_SMALL_TOP),
          &res.GetImageNamed(IDR_AURA_SHADOW_SMALL_TOP_RIGHT),
          &res.GetImageNamed(IDR_AURA_SHADOW_SMALL_LEFT),
          NULL,
          &res.GetImageNamed(IDR_AURA_SHADOW_SMALL_RIGHT),
          &res.GetImageNamed(IDR_AURA_SHADOW_SMALL_BOTTOM_LEFT),
          &res.GetImageNamed(IDR_AURA_SHADOW_SMALL_BOTTOM),
          &res.GetImageNamed(IDR_AURA_SHADOW_SMALL_BOTTOM_RIGHT));
      break;
    default:
      NOTREACHED() << "Unhandled style " << style_;
      break;
  }

  // Image sizes may have changed.
  UpdateImageGridBounds();
}

void Shadow::UpdateImageGridBounds() {
  // Update bounds based on content bounds and image sizes.
  image_grid_->SetContentBounds(content_bounds_);
}

}  // namespace internal
}  // namespace ash
