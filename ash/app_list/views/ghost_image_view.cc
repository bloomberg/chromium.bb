// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/ghost_image_view.h"

#include <algorithm>
#include <vector>

#include "ash/app_list/model/app_list_folder_item.h"
#include "ash/app_list/model/app_list_item_list.h"
#include "ash/app_list/views/app_list_item_view.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/animation/tween.h"
#include "ui/gfx/canvas.h"

namespace app_list {

namespace {

constexpr int kGhostCircleStrokeWidth = 2;
constexpr int kGhostColor = SkColorSetARGB(0x4C, 232, 234, 237);
constexpr int kFolderGhostColor = SkColorSetARGB(0x4C, 95, 99, 104);
constexpr base::TimeDelta kGhostFadeInOutLength =
    base::TimeDelta::FromMilliseconds(180);
constexpr int kInnerFolderGhostIconRadius = 14;
constexpr gfx::Tween::Type kGhostTween = gfx::Tween::FAST_OUT_SLOW_IN;

}  // namespace

GhostImageView::GhostImageView(AppListItemView* drag_view,
                               bool is_folder,
                               bool is_in_folder,
                               const gfx::Rect& drop_target_bounds,
                               int page)
    : is_hiding_(false),
      is_in_folder_(is_in_folder),
      is_folder_(is_folder),
      page_(page),
      drop_target_bounds_(drop_target_bounds),
      icon_bounds_(drag_view->GetIconBounds()) {
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);
  layer()->SetOpacity(0.0f);
  SetBoundsRect(drop_target_bounds);

  if (is_folder_) {
    AppListFolderItem* folder_item =
        static_cast<AppListFolderItem*>(drag_view->item());
    num_items_ = std::min(FolderImage::kNumFolderTopItems,
                          folder_item->item_list()->item_count());
  }
}

GhostImageView::~GhostImageView() {
  StopObservingImplicitAnimations();
}

void GhostImageView::FadeOut() {
  if (is_hiding_)
    return;
  is_hiding_ = true;
  DoAnimation(true /* fade out */);
}

void GhostImageView::FadeIn() {
  DoAnimation(false /* fade in */);
}

void GhostImageView::SetTransitionOffset(
    const gfx::Vector2d& transition_offset) {
  SetPosition(drop_target_bounds_.origin() + transition_offset);
}

const char* GhostImageView::GetClassName() const {
  return "GhostImageView";
}

void GhostImageView::DoAnimation(bool hide) {
  ui::ScopedLayerAnimationSettings animation(layer()->GetAnimator());
  animation.SetTransitionDuration(kGhostFadeInOutLength);
  animation.SetTweenType(kGhostTween);

  if (hide) {
    animation.AddObserver(this);
    animation.SetPreemptionStrategy(
        ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
    layer()->SetOpacity(0.0f);
    return;
  }
  layer()->SetOpacity(1.0f);
}

void GhostImageView::OnPaint(gfx::Canvas* canvas) {
  const gfx::PointF circle_center(icon_bounds_.CenterPoint());
  const float ghost_radius = icon_bounds_.width() / 2;

  // Draw a circle to represent the ghost image icon.
  cc::PaintFlags circle_flags;
  circle_flags.setAntiAlias(true);
  circle_flags.setColor(is_in_folder_ ? kFolderGhostColor : kGhostColor);
  circle_flags.setStyle(cc::PaintFlags::kStroke_Style);
  circle_flags.setStrokeWidth(kGhostCircleStrokeWidth);
  canvas->DrawCircle(circle_center, ghost_radius, circle_flags);

  if (is_folder_) {
    // Draw a mask so inner folder icons do not overlap the outer circle.
    SkPath outer_circle_mask;
    outer_circle_mask.addCircle(circle_center.x(), circle_center.y(),
                                ghost_radius - kGhostCircleStrokeWidth / 2);
    canvas->ClipPath(outer_circle_mask, true);

    // Returns the bounds for each inner icon in the folder icon.
    std::vector<gfx::Rect> top_icon_bounds =
        FolderImage::GetTopIconsBounds(icon_bounds_, num_items_.value());

    // Draw ghost items within the ghost folder circle.
    for (gfx::Rect bounds : top_icon_bounds) {
      canvas->DrawCircle(gfx::PointF(bounds.CenterPoint()),
                         kInnerFolderGhostIconRadius, circle_flags);
    }
  }
  ImageView::OnPaint(canvas);
}

void GhostImageView::OnImplicitAnimationsCompleted() {
  // Delete this GhostImageView when the fade out animation is done.
  delete this;
}

}  // namespace app_list
