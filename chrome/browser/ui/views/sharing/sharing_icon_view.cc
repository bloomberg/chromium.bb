// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/sharing/sharing_icon_view.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/animation/ink_drop.h"

namespace {
// TODO(knollr): move these into IconLabelBubbleView.
constexpr int kIconTextSpacing = 8;
constexpr int kIconTextSpacingTouch = 10;

// Progress state when the full length of the animation text is visible.
constexpr double kAnimationTextFullLengthShownProgressState = 0.5;
}  // namespace

SharingIconView::SharingIconView(PageActionIconView::Delegate* delegate)
    : PageActionIconView(/*command_updater=*/nullptr,
                         /*command_id=*/0,
                         delegate) {
  SetVisible(false);
  SetUpForInOutAnimation();
}

SharingIconView::~SharingIconView() = default;

void SharingIconView::StartLoadingAnimation() {
  if (loading_animation_)
    return;

  loading_animation_ = true;
  AnimateIn(IDS_BROWSER_SHARING_OMNIBOX_SENDING_LABEL);
  SchedulePaint();
}

void SharingIconView::StopLoadingAnimation() {
  if (!loading_animation_)
    return;

  loading_animation_ = false;
  UnpauseAnimation();
  SchedulePaint();
}

// TODO(knollr): Introduce IconState / ControllerState {eg, Hidden, Success,
// Sending} to define the various cases instead of a number of if else
// statements.
bool SharingIconView::Update() {
  auto* controller = GetController();
  if (!controller)
    return false;

  // To ensure that we reset error icon badge.
  if (!GetVisible()) {
    set_should_show_error(controller->send_failed());
    UpdateIconImage();
  }

  if (controller->is_loading())
    StartLoadingAnimation();
  else
    StopLoadingAnimation();

  if (last_controller_ != controller) {
    ResetSlideAnimation(/*show=*/false);
  }

  last_controller_ = controller;

  const bool is_bubble_showing = IsBubbleShowing();
  const bool is_visible =
      is_bubble_showing || IsLoadingAnimationVisible() || label()->GetVisible();
  const bool visibility_changed = GetVisible() != is_visible;

  SetVisible(is_visible);
  UpdateInkDrop(is_bubble_showing);
  return visibility_changed;
}

double SharingIconView::WidthMultiplier() const {
  double multiplier = PageActionIconView::WidthMultiplier();

  double min_width = image()->GetPreferredSize().width() + GetInsets().width();
  double spacing = ui::MaterialDesignController::touch_ui()
                       ? kIconTextSpacingTouch
                       : kIconTextSpacing;
  double label_width = label()->GetPreferredSize().width();
  double max_width = min_width + spacing + label_width;

  // We offset the width multiplier to start expanding the label straight away
  // instead of completely hide the icon and expanding it from zero width.
  double offset = min_width / max_width;
  return multiplier * (1 - offset) + offset;
}

void SharingIconView::AnimationProgressed(const gfx::Animation* animation) {
  if (animation->is_animating() &&
      GetAnimationValue() >= kAnimationTextFullLengthShownProgressState &&
      loading_animation_) {
    PauseAnimation();
  }
  UpdateOpacity();
  return PageActionIconView::AnimationProgressed(animation);
}

void SharingIconView::AnimationEnded(const gfx::Animation* animation) {
  PageActionIconView::AnimationEnded(animation);
  UpdateOpacity();

  auto* controller = GetController();
  if (controller && should_show_error() != controller->send_failed()) {
    set_should_show_error(controller->send_failed());
    UpdateIconImage();
    controller->MaybeShowErrorDialog();
  }
  Update();
}

void SharingIconView::UpdateOpacity() {
  if (!IsShrinking()) {
    DestroyLayer();
    SetTextSubpixelRenderingEnabled(true);
    return;
  }

  if (!layer()) {
    SetPaintToLayer();
    SetTextSubpixelRenderingEnabled(false);
    layer()->SetFillsBoundsOpaquely(false);
  }

  layer()->SetOpacity(PageActionIconView::WidthMultiplier());
}

void SharingIconView::UpdateInkDrop(bool activate) {
  auto target_state =
      activate ? views::InkDropState::ACTIVATED : views::InkDropState::HIDDEN;
  if (GetInkDrop()->GetTargetInkDropState() != target_state)
    AnimateInkDrop(target_state, /*event=*/nullptr);
}

bool SharingIconView::IsTriggerableEvent(const ui::Event& event) {
  // We do nothing when the icon is clicked.
  return false;
}

const gfx::VectorIcon& SharingIconView::GetVectorIconBadge() const {
  return should_show_error_ ? kBlockedBadgeIcon : gfx::kNoneIcon;
}

void SharingIconView::OnExecuting(
    PageActionIconView::ExecuteSource execute_source) {}

bool SharingIconView::IsLoadingAnimationVisible() {
  return loading_animation_;
}
