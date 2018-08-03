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
#include "ash/assistant/ui/main_stage/assistant_progress_indicator.h"
#include "ash/assistant/util/animation_util.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/time/time.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/compositor/layer_animation_sequence.h"
#include "ui/compositor/layer_animator.h"
#include "ui/views/border.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"

namespace ash {

namespace {

// Appearance.
constexpr int kIconSizeDip = 24;
constexpr int kInitialHeightDip = 72;

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

int AssistantHeaderView::GetHeightForWidth(int width) const {
  return greeting_label_->visible() ? kInitialHeightDip
                                    : views::View::GetHeightForWidth(width);
}

void AssistantHeaderView::ChildVisibilityChanged(views::View* child) {
  if (!assistant::ui::kIsMotionSpecEnabled) {
    layout_manager_->set_cross_axis_alignment(
        greeting_label_->visible()
            ? views::BoxLayout::CrossAxisAlignment::CROSS_AXIS_ALIGNMENT_CENTER
            : views::BoxLayout::CrossAxisAlignment::CROSS_AXIS_ALIGNMENT_START);
  }
  PreferredSizeChanged();
}

void AssistantHeaderView::InitLayout() {
  layout_manager_ = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(), kSpacingDip));

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

  // Greeting label.
  greeting_label_ = new views::Label(
      l10n_util::GetStringUTF16(IDS_ASH_ASSISTANT_PROMPT_DEFAULT));
  greeting_label_->SetAutoColorReadabilityEnabled(false);
  greeting_label_->SetEnabledColor(kTextColorPrimary);
  greeting_label_->SetFontList(
      assistant::ui::GetDefaultFontList()
          .DeriveWithSizeDelta(8)
          .DeriveWithWeight(gfx::Font::Weight::MEDIUM));
  greeting_label_->SetHorizontalAlignment(
      gfx::HorizontalAlignment::ALIGN_CENTER);
  greeting_label_->SetMultiLine(true);
  AddChildView(greeting_label_);

  // Progress indicator.
  // Note that we add an empty border to increase the top margin.
  progress_indicator_ = new AssistantProgressIndicator();
  progress_indicator_->SetBorder(
      views::CreateEmptyBorder(/*top=*/kSpacingDip, 0, 0, 0));
  progress_indicator_->SetVisible(false);
  AddChildView(progress_indicator_);
}

void AssistantHeaderView::OnCommittedQueryChanged(
    const AssistantQuery& committed_query) {
  greeting_label_->SetVisible(false);
  progress_indicator_->SetVisible(true);
}

void AssistantHeaderView::OnResponseChanged(const AssistantResponse& response) {
  progress_indicator_->SetVisible(false);

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

  molecule_icon_->layer()->SetTransform(gfx::Transform());
  greeting_label_->SetVisible(true);
  progress_indicator_->SetVisible(false);
}

}  // namespace ash
