// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/ui/main_stage/assistant_footer_view.h"

#include <utility>

#include "ash/assistant/assistant_controller.h"
#include "ash/assistant/assistant_setup_controller.h"
#include "ash/assistant/ui/assistant_ui_constants.h"
#include "ash/assistant/ui/main_stage/assistant_opt_in_view.h"
#include "ash/assistant/ui/main_stage/suggestion_container_view.h"
#include "ash/assistant/util/animation_util.h"
#include "ash/shell.h"
#include "ash/voice_interaction/voice_interaction_controller.h"
#include "base/bind.h"
#include "base/time/time.h"
#include "ui/compositor/callback_layer_animation_observer.h"
#include "ui/compositor/layer_animation_element.h"
#include "ui/compositor/layer_animator.h"
#include "ui/views/layout/fill_layout.h"

namespace ash {

namespace {

// Appearance.
constexpr int kPreferredHeightDip = 48;

// Animation.
constexpr base::TimeDelta kAnimationFadeInDelay =
    base::TimeDelta::FromMilliseconds(167);
constexpr base::TimeDelta kAnimationFadeInDuration =
    base::TimeDelta::FromMilliseconds(167);
constexpr base::TimeDelta kAnimationFadeOutDuration =
    base::TimeDelta::FromMilliseconds(167);

}  // namespace

AssistantFooterView::AssistantFooterView(
    AssistantController* assistant_controller)
    : assistant_controller_(assistant_controller),
      animation_observer_(std::make_unique<ui::CallbackLayerAnimationObserver>(
          /*animation_started_callback=*/base::BindRepeating(
              &AssistantFooterView::OnAnimationStarted,
              base::Unretained(this)),
          /*animation_ended_callback=*/base::BindRepeating(
              &AssistantFooterView::OnAnimationEnded,
              base::Unretained(this)))),
      voice_interaction_observer_binding_(this) {
  InitLayout();

  // Observe voice interaction for changes to consent state.
  mojom::VoiceInteractionObserverPtr ptr;
  voice_interaction_observer_binding_.Bind(mojo::MakeRequest(&ptr));
  Shell::Get()->voice_interaction_controller()->AddObserver(std::move(ptr));
}

AssistantFooterView::~AssistantFooterView() = default;

gfx::Size AssistantFooterView::CalculatePreferredSize() const {
  return gfx::Size(INT_MAX, GetHeightForWidth(INT_MAX));
}

int AssistantFooterView::GetHeightForWidth(int width) const {
  return kPreferredHeightDip;
}

void AssistantFooterView::ChildPreferredSizeChanged(views::View* child) {
  PreferredSizeChanged();
}

void AssistantFooterView::ChildVisibilityChanged(views::View* child) {
  PreferredSizeChanged();
}

void AssistantFooterView::SetFocusBehavior(FocusBehavior focus_behavior) {
  switch (focus_behavior) {
    case FocusBehavior::NEVER:
      // When focus behavior is NEVER, we update both views.
      suggestion_container_->SetFocusBehavior(FocusBehavior::NEVER);
      opt_in_view_->SetFocusBehavior(FocusBehavior::NEVER);
      break;
    case FocusBehavior::ALWAYS:
      // When focus behavior is ALWAYS, we need to respect opt in state.
      UpdateFocusBehavior();
      break;
    case FocusBehavior::ACCESSIBLE_ONLY:
      // We don't currently use ACCESSIBLE_ONLY focus behavior.
      NOTREACHED();
      break;
  }
}

void AssistantFooterView::InitLayout() {
  SetLayoutManager(std::make_unique<views::FillLayout>());

  // Initial view state is based on user consent state.
  const bool setup_completed =
      Shell::Get()->voice_interaction_controller()->setup_completed();

  // Suggestion container.
  suggestion_container_ = new SuggestionContainerView(assistant_controller_);
  suggestion_container_->set_can_process_events_within_subtree(setup_completed);

  // Suggestion container will be animated on its own layer.
  suggestion_container_->SetPaintToLayer();
  suggestion_container_->layer()->SetFillsBoundsOpaquely(false);
  suggestion_container_->layer()->SetOpacity(setup_completed ? 1.f : 0.f);

  AddChildView(suggestion_container_);

  // Opt in view.
  opt_in_view_ = new AssistantOptInView();
  opt_in_view_->set_can_process_events_within_subtree(!setup_completed);
  opt_in_view_->set_delegate(assistant_controller_->setup_controller());

  // Opt in view will be animated on its own layer.
  opt_in_view_->SetPaintToLayer();
  opt_in_view_->layer()->SetFillsBoundsOpaquely(false);
  opt_in_view_->layer()->SetOpacity(setup_completed ? 0.f : 1.f);

  AddChildView(opt_in_view_);

  // Initialize focus behavior.
  UpdateFocusBehavior(setup_completed);
}

void AssistantFooterView::OnVoiceInteractionSetupCompleted(bool completed) {
  using assistant::util::CreateLayerAnimationSequence;
  using assistant::util::CreateOpacityElement;
  using assistant::util::StartLayerAnimationSequence;

  // When the consent state changes, we need to hide/show the appropriate views.
  views::View* hide_view =
      completed ? static_cast<views::View*>(opt_in_view_)
                : static_cast<views::View*>(suggestion_container_);

  views::View* show_view =
      completed ? static_cast<views::View*>(suggestion_container_)
                : static_cast<views::View*>(opt_in_view_);

  // Views should not be focusable while they're animating.
  suggestion_container_->SetFocusBehavior(FocusBehavior::NEVER);
  opt_in_view_->SetFocusBehavior(FocusBehavior::NEVER);

  // Hide the view for the previous consent state by fading to 0% opacity.
  StartLayerAnimationSequence(hide_view->layer()->GetAnimator(),
                              CreateLayerAnimationSequence(CreateOpacityElement(
                                  0.f, kAnimationFadeOutDuration)),
                              // Observe the animation.
                              animation_observer_.get());

  // Show the view for the next consent state by fading to 100% opacity with
  // delay.
  StartLayerAnimationSequence(
      show_view->layer()->GetAnimator(),
      CreateLayerAnimationSequence(
          ui::LayerAnimationElement::CreatePauseElement(
              ui::LayerAnimationElement::AnimatableProperty::OPACITY,
              kAnimationFadeInDelay),
          CreateOpacityElement(1.f, kAnimationFadeInDuration)),
      // Observe the animation.
      animation_observer_.get());

  // Set the observer to active to receive animation callback events.
  animation_observer_->SetActive();
}

void AssistantFooterView::OnAnimationStarted(
    const ui::CallbackLayerAnimationObserver& observer) {
  // Our views should not process events while animating.
  suggestion_container_->set_can_process_events_within_subtree(false);
  opt_in_view_->set_can_process_events_within_subtree(false);
}

bool AssistantFooterView::OnAnimationEnded(
    const ui::CallbackLayerAnimationObserver& observer) {
  const bool setup_completed =
      Shell::Get()->voice_interaction_controller()->setup_completed();

  // Only the view relevant to our consent state should process events.
  suggestion_container_->set_can_process_events_within_subtree(setup_completed);
  opt_in_view_->set_can_process_events_within_subtree(!setup_completed);

  UpdateFocusBehavior(setup_completed);

  // Return false to prevent the observer from destroying itself.
  return false;
}

void AssistantFooterView::UpdateFocusBehavior(
    base::Optional<bool> setup_completed) {
  if (!setup_completed.has_value()) {
    setup_completed =
        Shell::Get()->voice_interaction_controller()->setup_completed();
  }

  suggestion_container_->SetFocusBehavior(
      setup_completed.value() ? FocusBehavior::ALWAYS : FocusBehavior::NEVER);
  opt_in_view_->SetFocusBehavior(
      setup_completed.value() ? FocusBehavior::NEVER : FocusBehavior::ALWAYS);
}

}  // namespace ash
