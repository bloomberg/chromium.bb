// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/shelf/shelf_background_animator.h"

#include <algorithm>

#include "ash/common/material_design/material_design_controller.h"
#include "ash/common/shelf/shelf_background_animator_observer.h"
#include "ash/common/shelf/shelf_constants.h"
#include "ash/common/shelf/wm_shelf.h"

namespace ash {

namespace {
// The total number of animators that will call BackgroundAnimationEnded().
const int kNumAnimators = 3;

const int kMaxAlpha = 255;
}  // namespace

ShelfBackgroundAnimator::ShelfBackgroundAnimator(
    ShelfBackgroundType background_type,
    WmShelf* wm_shelf)
    : wm_shelf_(wm_shelf) {
  if (wm_shelf_)
    wm_shelf_->AddObserver(this);
  // Initialize animators so that adding observers get notified with consistent
  // values.
  AnimateBackground(background_type, BACKGROUND_CHANGE_IMMEDIATE);
}

ShelfBackgroundAnimator::~ShelfBackgroundAnimator() {
  if (wm_shelf_)
    wm_shelf_->RemoveObserver(this);
}

void ShelfBackgroundAnimator::AddObserver(
    ShelfBackgroundAnimatorObserver* observer) {
  observers_.AddObserver(observer);
  Initialize(observer);
}

void ShelfBackgroundAnimator::RemoveObserver(
    ShelfBackgroundAnimatorObserver* observer) {
  observers_.RemoveObserver(observer);
}

void ShelfBackgroundAnimator::Initialize(
    ShelfBackgroundAnimatorObserver* observer) const {
  observer->UpdateShelfOpaqueBackground(opaque_background_animator_->alpha());
  observer->UpdateShelfAssetBackground(asset_background_animator_->alpha());
  observer->UpdateShelfItemBackground(item_background_animator_->alpha());
}

void ShelfBackgroundAnimator::PaintBackground(
    ShelfBackgroundType background_type,
    BackgroundAnimatorChangeType change_type) {
  if (target_background_type_ == background_type &&
      change_type == BACKGROUND_CHANGE_ANIMATE) {
    return;
  }

  AnimateBackground(background_type, change_type);
}

void ShelfBackgroundAnimator::OnBackgroundTypeChanged(
    ShelfBackgroundType background_type,
    BackgroundAnimatorChangeType change_type) {
  PaintBackground(background_type, change_type);
}

void ShelfBackgroundAnimator::UpdateBackground(BackgroundAnimator* animator,
                                               int alpha) {
  OnAlphaChanged(animator, alpha);
}

void ShelfBackgroundAnimator::BackgroundAnimationEnded(
    BackgroundAnimator* animator) {
  ++successful_animator_count_;
  DCHECK_LE(successful_animator_count_, kNumAnimators);
  // UpdateBackground() is only called when alpha values change, this ensures
  // observers are always notified for every background change.
  OnAlphaChanged(animator, animator->alpha());
}

void ShelfBackgroundAnimator::OnAlphaChanged(BackgroundAnimator* animator,
                                             int alpha) {
  if (animator == opaque_background_animator_.get()) {
    FOR_EACH_OBSERVER(ShelfBackgroundAnimatorObserver, observers_,
                      UpdateShelfOpaqueBackground(alpha));
  } else if (animator == asset_background_animator_.get()) {
    FOR_EACH_OBSERVER(ShelfBackgroundAnimatorObserver, observers_,
                      UpdateShelfAssetBackground(alpha));
  } else if (animator == item_background_animator_.get()) {
    FOR_EACH_OBSERVER(ShelfBackgroundAnimatorObserver, observers_,
                      UpdateShelfItemBackground(alpha));
  } else {
    NOTREACHED();
  }
}

void ShelfBackgroundAnimator::AnimateBackground(
    ShelfBackgroundType background_type,
    BackgroundAnimatorChangeType change_type) {
  // Ensure BackgroundAnimationEnded() has been called for all the
  // BackgroundAnimators owned by this so that |successful_animator_count_|
  // is stable and doesn't get updated as a side effect of destroying/animating
  // the animators.
  StopAnimators();

  bool show_background = true;
  if (can_reuse_animators_ && previous_background_type_ == background_type) {
    DCHECK_EQ(opaque_background_animator_->paints_background(),
              asset_background_animator_->paints_background());
    DCHECK_EQ(asset_background_animator_->paints_background(),
              item_background_animator_->paints_background());

    show_background = !opaque_background_animator_->paints_background();
  } else {
    CreateAnimators(background_type, change_type);

    // If all the previous animators completed successfully and the animation
    // was between 2 distinct states, then the last alpha values are valid
    // end state values.
    can_reuse_animators_ = target_background_type_ != background_type &&
                           successful_animator_count_ == kNumAnimators;
  }

  successful_animator_count_ = 0;

  opaque_background_animator_->SetPaintsBackground(show_background,
                                                   change_type);
  asset_background_animator_->SetPaintsBackground(show_background, change_type);
  item_background_animator_->SetPaintsBackground(show_background, change_type);

  if (target_background_type_ != background_type) {
    previous_background_type_ = target_background_type_;
    target_background_type_ = background_type;
  }
}

void ShelfBackgroundAnimator::CreateAnimators(
    ShelfBackgroundType background_type,
    BackgroundAnimatorChangeType change_type) {
  const int opaque_background_alpha =
      opaque_background_animator_ ? opaque_background_animator_->alpha() : 0;
  const int asset_background_alpha =
      asset_background_animator_ ? asset_background_animator_->alpha() : 0;
  const int item_background_alpha =
      item_background_animator_ ? item_background_animator_->alpha() : 0;

  const bool is_material = MaterialDesignController::IsShelfMaterial();
  int duration_ms = 0;

  switch (background_type) {
    case SHELF_BACKGROUND_DEFAULT:
      duration_ms = is_material ? 500 : 1000;
      opaque_background_animator_.reset(
          new BackgroundAnimator(this, opaque_background_alpha, 0));
      asset_background_animator_.reset(
          new BackgroundAnimator(this, asset_background_alpha, 0));
      item_background_animator_.reset(
          new BackgroundAnimator(this, item_background_alpha,
                                 GetShelfConstant(SHELF_BACKGROUND_ALPHA)));
      break;
    case SHELF_BACKGROUND_OVERLAP:
      duration_ms = is_material ? 500 : 1000;
      opaque_background_animator_.reset(new BackgroundAnimator(
          this, opaque_background_alpha,
          is_material ? GetShelfConstant(SHELF_BACKGROUND_ALPHA) : 0));
      asset_background_animator_.reset(new BackgroundAnimator(
          this, asset_background_alpha,
          is_material ? 0 : GetShelfConstant(SHELF_BACKGROUND_ALPHA)));
      item_background_animator_.reset(new BackgroundAnimator(
          this, item_background_alpha,
          is_material ? 0 : GetShelfConstant(SHELF_BACKGROUND_ALPHA)));
      break;
    case SHELF_BACKGROUND_MAXIMIZED:
      duration_ms = is_material ? 250 : 1000;
      opaque_background_animator_.reset(
          new BackgroundAnimator(this, opaque_background_alpha, kMaxAlpha));
      asset_background_animator_.reset(new BackgroundAnimator(
          this, asset_background_alpha,
          is_material ? 0 : GetShelfConstant(SHELF_BACKGROUND_ALPHA)));
      item_background_animator_.reset(new BackgroundAnimator(
          this, item_background_alpha, is_material ? 0 : kMaxAlpha));
      break;
  }

  opaque_background_animator_->SetDuration(duration_ms);
  asset_background_animator_->SetDuration(duration_ms);
  item_background_animator_->SetDuration(duration_ms);
}

void ShelfBackgroundAnimator::StopAnimators() {
  if (opaque_background_animator_)
    opaque_background_animator_->Stop();
  if (asset_background_animator_)
    asset_background_animator_->Stop();
  if (item_background_animator_)
    item_background_animator_->Stop();
}

}  // namespace ash
