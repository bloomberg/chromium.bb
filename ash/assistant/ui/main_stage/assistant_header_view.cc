// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/ui/main_stage/assistant_header_view.h"

#include <memory>

#include "ash/assistant/assistant_controller.h"
#include "ash/assistant/assistant_interaction_controller.h"
#include "ash/assistant/assistant_ui_controller.h"
#include "ash/assistant/model/assistant_interaction_model.h"
#include "ash/assistant/model/assistant_query.h"
#include "ash/assistant/ui/assistant_ui_constants.h"
#include "ash/assistant/ui/logo_view/base_logo_view.h"
#include "ash/assistant/util/animation_util.h"
#include "base/time/time.h"
#include "ui/compositor/layer_animation_sequence.h"
#include "ui/compositor/layer_animator.h"
#include "ui/views/layout/box_layout.h"

namespace ash {

namespace {

// Appearance.
constexpr int kIconSizeDip = 24;

// Appear animation.
constexpr base::TimeDelta kAppearAnimationFadeInDelay =
    base::TimeDelta::FromMilliseconds(33);
constexpr base::TimeDelta kAppearAnimationFadeInDuration =
    base::TimeDelta::FromMilliseconds(167);
constexpr base::TimeDelta kAppearAnimationTranslateUpDuration =
    base::TimeDelta::FromMilliseconds(250);
constexpr int kAppearAnimationTranslationUpDip = 115;

// Response animation.
constexpr base::TimeDelta kResponseAnimationFadeInDelay =
    base::TimeDelta::FromMilliseconds(50);
constexpr base::TimeDelta kResponseAnimationFadeInDuration =
    base::TimeDelta::FromMilliseconds(83);
constexpr base::TimeDelta kResponseAnimationFadeOutDelay =
    base::TimeDelta::FromMilliseconds(33);
constexpr base::TimeDelta kResponseAnimationFadeOutDuration =
    base::TimeDelta::FromMilliseconds(83);
constexpr base::TimeDelta kResponseAnimationTranslateLeftDuration =
    base::TimeDelta::FromMilliseconds(333);

}  // namespace

AssistantHeaderView::AssistantHeaderView(
    AssistantController* assistant_controller)
    : assistant_controller_(assistant_controller) {
  InitLayout();

  // The Assistant controller indirectly owns the view hierarchy to which
  // AssistantHeaderView belongs so is guaranteed to outlive it.
  assistant_controller_->interaction_controller()->AddModelObserver(this);
  assistant_controller_->ui_controller()->AddModelObserver(this);
}

AssistantHeaderView::~AssistantHeaderView() {
  assistant_controller_->ui_controller()->RemoveModelObserver(this);
  assistant_controller_->interaction_controller()->RemoveModelObserver(this);
}

gfx::Size AssistantHeaderView::CalculatePreferredSize() const {
  return gfx::Size(INT_MAX, GetHeightForWidth(INT_MAX));
}

void AssistantHeaderView::ChildVisibilityChanged(views::View* child) {
  PreferredSizeChanged();
}

void AssistantHeaderView::InitLayout() {
  layout_manager_ = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));

  layout_manager_->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::CROSS_AXIS_ALIGNMENT_CENTER);

  // Molecule icon.
  molecule_icon_ = BaseLogoView::Create();
  molecule_icon_->SetPreferredSize(gfx::Size(kIconSizeDip, kIconSizeDip));
  molecule_icon_->SetState(BaseLogoView::State::kMoleculeWavy,
                           /*animate=*/false);

  // The molecule icon will be animated on its own layer.
  molecule_icon_->SetPaintToLayer();
  molecule_icon_->layer()->SetFillsBoundsOpaquely(false);

  AddChildView(molecule_icon_);
}

void AssistantHeaderView::OnCommittedQueryChanged(
    const AssistantQuery& committed_query) {
  if (!assistant::ui::kIsMotionSpecEnabled) {
    layout_manager_->set_cross_axis_alignment(
        views::BoxLayout::CrossAxisAlignment::CROSS_AXIS_ALIGNMENT_START);

    // Force a layout/paint pass.
    Layout();
    SchedulePaint();
  }
}

void AssistantHeaderView::OnResponseChanged(const AssistantResponse& response) {
  // We only handle the first response when animating the molecule icon. For
  // all subsequent responses the molecule icon remains unchanged.
  if (!is_first_response_)
    return;

  is_first_response_ = false;

  if (!assistant::ui::kIsMotionSpecEnabled)
    return;

  using namespace assistant::util;

  // The molecule icon will be animated from the center of its parent, to the
  // left hand side.
  gfx::Transform transform;
  transform.Translate(-(width() - molecule_icon_->width()) / 2, 0);

  // Animate the molecule icon.
  molecule_icon_->layer()->GetAnimator()->StartTogether(
      {// Animate the translation.
       CreateLayerAnimationSequence(CreateTransformElement(
           transform, kResponseAnimationTranslateLeftDuration)),
       // Animate the opacity.
       CreateLayerAnimationSequence(
           // Pause...
           ui::LayerAnimationElement::CreatePauseElement(
               ui::LayerAnimationElement::AnimatableProperty::OPACITY,
               kResponseAnimationFadeOutDelay),
           // ...then fade out...
           CreateOpacityElement(0.f, kResponseAnimationFadeOutDuration),
           // ...hold...
           ui::LayerAnimationElement::CreatePauseElement(
               ui::LayerAnimationElement::AnimatableProperty::OPACITY,
               kResponseAnimationFadeInDelay),
           // ...and fade back in.
           CreateOpacityElement(1.f, kResponseAnimationFadeInDuration))});
}

void AssistantHeaderView::OnUiVisibilityChanged(bool visible,
                                                AssistantSource source) {
  if (visible) {
    // When Assistant UI is shown and the motion spec is enabled, we animate in
    // the appearance of the molecule icon.
    if (assistant::ui::kIsMotionSpecEnabled) {
      using namespace assistant::util;

      // We're going to animate the molecule icon up into position so we'll need
      // to apply an initial transformation.
      gfx::Transform transform;
      transform.Translate(0, kAppearAnimationTranslationUpDip);

      // Set up our pre-animation values.
      molecule_icon_->layer()->SetOpacity(0.f);
      molecule_icon_->layer()->SetTransform(transform);

      // Start animating molecule icon.
      molecule_icon_->layer()->GetAnimator()->StartTogether(
          {// Animate the transformation.
           CreateLayerAnimationSequence(CreateTransformElement(
               gfx::Transform(), kAppearAnimationTranslateUpDuration,
               gfx::Tween::Type::FAST_OUT_SLOW_IN_2)),
           // Animate the opacity to 100% with delay.
           CreateLayerAnimationSequence(
               ui::LayerAnimationElement::CreatePauseElement(
                   ui::LayerAnimationElement::AnimatableProperty::OPACITY,
                   kAppearAnimationFadeInDelay),
               CreateOpacityElement(1.f, kAppearAnimationFadeInDuration))});
    }
    return;
  }

  // When Assistant UI is hidden, we restore initial state for the next session.
  is_first_response_ = true;

  if (!assistant::ui::kIsMotionSpecEnabled) {
    layout_manager_->set_cross_axis_alignment(
        views::BoxLayout::CrossAxisAlignment::CROSS_AXIS_ALIGNMENT_CENTER);

    // Force a layout/paint pass.
    Layout();
    SchedulePaint();
  }

  molecule_icon_->layer()->SetTransform(gfx::Transform());
}

}  // namespace ash
