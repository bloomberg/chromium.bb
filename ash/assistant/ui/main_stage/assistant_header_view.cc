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

// Molecule icon animation.
constexpr base::TimeDelta kMoleculeIconAnimationTranslationDuration =
    base::TimeDelta::FromMilliseconds(333);
constexpr base::TimeDelta kMoleculeIconAnimationFadeInDelay =
    base::TimeDelta::FromMilliseconds(50);
constexpr base::TimeDelta kMoleculeIconAnimationFadeInDuration =
    base::TimeDelta::FromMilliseconds(83);
constexpr base::TimeDelta kMoleculeIconAnimationFadeOutDelay =
    base::TimeDelta::FromMilliseconds(33);
constexpr base::TimeDelta kMoleculeIconAnimationFadeOutDuration =
    base::TimeDelta::FromMilliseconds(83);

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
           transform, kMoleculeIconAnimationTranslationDuration)),
       // Animate the opacity.
       CreateLayerAnimationSequence(
           // Pause...
           ui::LayerAnimationElement::CreatePauseElement(
               ui::LayerAnimationElement::AnimatableProperty::OPACITY,
               kMoleculeIconAnimationFadeOutDelay),
           // ...then fade out...
           CreateOpacityElement(0.f, kMoleculeIconAnimationFadeOutDuration),
           // ...hold...
           ui::LayerAnimationElement::CreatePauseElement(
               ui::LayerAnimationElement::AnimatableProperty::OPACITY,
               kMoleculeIconAnimationFadeInDelay),
           // ...and fade back in.
           CreateOpacityElement(1.f, kMoleculeIconAnimationFadeInDuration))});
}

void AssistantHeaderView::OnUiVisibilityChanged(bool visible,
                                                AssistantSource source) {
  // Only when the Assistant UI is being hidden do we need to restore the
  // initial state for the next session.
  if (visible)
    return;

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
