// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/ui/main_stage/assistant_main_stage.h"

#include "ash/assistant/assistant_controller.h"
#include "ash/assistant/assistant_interaction_controller.h"
#include "ash/assistant/assistant_ui_controller.h"
#include "ash/assistant/ui/assistant_ui_constants.h"
#include "ash/assistant/ui/main_stage/assistant_query_view.h"
#include "ash/assistant/ui/main_stage/suggestion_container_view.h"
#include "ash/assistant/ui/main_stage/ui_element_container_view.h"
#include "ash/assistant/util/animation_util.h"
#include "base/bind.h"
#include "base/time/time.h"
#include "ui/compositor/callback_layer_animation_observer.h"
#include "ui/compositor/layer_animation_element.h"
#include "ui/compositor/layer_animator.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/layout_manager.h"

namespace ash {

namespace {

// Animation.
// TODO(dmblack): Rename these constants to refer to the specific views they are
// animating to avoid confusion.
constexpr base::TimeDelta kAnimationExitFadeDuration =
    base::TimeDelta::FromMilliseconds(50);
constexpr int kAnimationExitTranslationDip = -103;
constexpr base::TimeDelta kAnimationExitTranslateDuration =
    base::TimeDelta::FromMilliseconds(333);
constexpr base::TimeDelta kAnimationFadeInDelay =
    base::TimeDelta::FromMilliseconds(200);
constexpr base::TimeDelta kAnimationFadeInDuration =
    base::TimeDelta::FromMilliseconds(83);
constexpr base::TimeDelta kAnimationFadeOutDuration =
    base::TimeDelta::FromMilliseconds(83);
constexpr base::TimeDelta kAnimationTranslateUpDelay =
    base::TimeDelta::FromMilliseconds(83);
constexpr base::TimeDelta kAnimationTranslateUpDuration =
    base::TimeDelta::FromMilliseconds(333);

// Pending query animation.
constexpr base::TimeDelta kPendingQueryAnimationFadeInDuration =
    base::TimeDelta::FromMilliseconds(433);

// Suggestion container animation.
constexpr int kSuggestionContainerAnimationTranslationDip = 22;
constexpr base::TimeDelta kSuggestionContainerAnimationTranslationDelay =
    base::TimeDelta::FromMilliseconds(66);
constexpr base::TimeDelta kSuggestionContainerAnimationTranslationDuration =
    base::TimeDelta::FromMilliseconds(416);
constexpr base::TimeDelta kSuggestionContainerAnimationFadeInDelay =
    base::TimeDelta::FromMilliseconds(149);
constexpr base::TimeDelta kSuggestionContainerAnimationFadeInDuration =
    base::TimeDelta::FromMilliseconds(250);
constexpr base::TimeDelta kSuggestionContainerAnimationFadeOutDuration =
    base::TimeDelta::FromMilliseconds(100);

// StackLayout -----------------------------------------------------------------

// A layout manager which lays out its views atop each other. This differs from
// FillLayout in that we respect the preferred size of views during layout. In
// contrast, FillLayout will cause its views to match the bounds of the host.
class StackLayout : public views::LayoutManager {
 public:
  StackLayout() = default;
  ~StackLayout() override = default;

  gfx::Size GetPreferredSize(const views::View* host) const override {
    gfx::Size preferred_size;

    for (int i = 0; i < host->child_count(); ++i)
      preferred_size.SetToMax(host->child_at(i)->GetPreferredSize());

    return preferred_size;
  }

  int GetPreferredHeightForWidth(const views::View* host,
                                 int width) const override {
    int preferred_height = 0;

    for (int i = 0; i < host->child_count(); ++i) {
      preferred_height = std::max(host->child_at(i)->GetHeightForWidth(width),
                                  preferred_height);
    }

    return preferred_height;
  }

  void Layout(views::View* host) override {
    const int host_width = host->GetContentsBounds().width();

    for (int i = 0; i < host->child_count(); ++i) {
      views::View* child = host->child_at(i);

      int child_width = std::min(child->GetPreferredSize().width(), host_width);
      int child_height = child->GetHeightForWidth(child_width);

      // Children are horizontally centered, top aligned.
      child->SetBounds(/*x=*/(host_width - child_width) / 2, /*y=*/0,
                       child_width, child_height);
    }
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(StackLayout);
};

}  // namespace

// AssistantMainStage ----------------------------------------------------------

AssistantMainStage::AssistantMainStage(
    AssistantController* assistant_controller)
    : assistant_controller_(assistant_controller),
      active_query_exit_animation_observer_(
          std::make_unique<ui::CallbackLayerAnimationObserver>(
              /*animation_ended_callback=*/base::BindRepeating(
                  &AssistantMainStage::OnActiveQueryExitAnimationEnded,
                  base::Unretained(this)))),
      suggestion_container_animation_observer_(
          std::make_unique<ui::CallbackLayerAnimationObserver>(
              /*animation_started_callback=*/base::BindRepeating(
                  &AssistantMainStage::OnSuggestionContainerAnimationStarted,
                  base::Unretained(this)),
              /*animation_ended_callback=*/base::BindRepeating(
                  &AssistantMainStage::OnSuggestionContainerAnimationEnded,
                  base::Unretained(this)))) {
  InitLayout(assistant_controller);

  // The view hierarchy will be destructed before Shell, which owns
  // AssistantController, so AssistantController is guaranteed to outlive the
  // AssistantMainStage.
  assistant_controller_->interaction_controller()->AddModelObserver(this);
  assistant_controller_->ui_controller()->AddModelObserver(this);
}

AssistantMainStage::~AssistantMainStage() {
  assistant_controller_->ui_controller()->RemoveModelObserver(this);
  assistant_controller_->interaction_controller()->RemoveModelObserver(this);
}

void AssistantMainStage::ChildPreferredSizeChanged(views::View* child) {
  PreferredSizeChanged();
}

void AssistantMainStage::ChildVisibilityChanged(views::View* child) {
  PreferredSizeChanged();
}

void AssistantMainStage::OnViewBoundsChanged(views::View* view) {
  if (view == active_query_view_) {
    UpdateActiveQueryViewSpacer();
  } else if (view == committed_query_view_) {
    UpdateQueryViewTransform(committed_query_view_);
  } else if (view == pending_query_view_) {
    UpdateQueryViewTransform(pending_query_view_);
  } else if (view == query_layout_container_) {
    if (committed_query_view_)
      UpdateQueryViewTransform(committed_query_view_);
    if (pending_query_view_)
      UpdateQueryViewTransform(pending_query_view_);
  }
}

void AssistantMainStage::OnViewPreferredSizeChanged(views::View* view) {
  PreferredSizeChanged();
}

void AssistantMainStage::OnViewVisibilityChanged(views::View* view) {
  PreferredSizeChanged();
}

void AssistantMainStage::InitLayout(AssistantController* assistant_controller) {
  SetLayoutManager(std::make_unique<views::FillLayout>());

  // The children of AssistantMainStage will be animated on their own layers and
  // we want them to be clipped by their parent layer.
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);
  layer()->SetMasksToBounds(true);

  InitContentLayoutContainer(assistant_controller);
  InitQueryLayoutContainer(assistant_controller);
}

void AssistantMainStage::InitContentLayoutContainer(
    AssistantController* assistant_controller) {
  // Note that we will observe children of |content_layout_container| to handle
  // preferred size and visibility change events in AssistantMainStage. This is
  // necessary because |content_layout_container| may not change size in
  // response to these events, necessitating an explicit layout pass.
  views::View* content_layout_container = new views::View();

  views::BoxLayout* layout_manager = content_layout_container->SetLayoutManager(
      std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kVertical));

  // Active query spacer.
  // Note: This view reserves layout space for |active_query_view_|,
  // dynamically mirroring its preferred size and visibility.
  active_query_view_spacer_ = new views::View();
  active_query_view_spacer_->AddObserver(this);
  content_layout_container->AddChildView(active_query_view_spacer_);

  // UI element container.
  ui_element_container_ = new UiElementContainerView(assistant_controller);
  ui_element_container_->AddObserver(this);
  content_layout_container->AddChildView(ui_element_container_);

  layout_manager->SetFlexForView(ui_element_container_, 1);

  // Suggestion container.
  suggestion_container_ = new SuggestionContainerView(assistant_controller);
  suggestion_container_->AddObserver(this);

  // The suggestion container will be animated on its own layer.
  suggestion_container_->SetPaintToLayer();
  suggestion_container_->layer()->SetFillsBoundsOpaquely(false);

  content_layout_container->AddChildView(suggestion_container_);

  AddChildView(content_layout_container);
}

void AssistantMainStage::InitQueryLayoutContainer(
    AssistantController* assistant_controller) {
  // Note that we will observe children of |query_layout_container_| to handle
  // preferred size and visibility change events in AssistantMainStage. This is
  // necessary because |query_layout_container_| may not change size in response
  // to these events, thereby requiring an explicit layout pass.
  query_layout_container_ = new views::View();
  query_layout_container_->AddObserver(this);
  query_layout_container_->set_can_process_events_within_subtree(false);
  query_layout_container_->SetLayoutManager(std::make_unique<StackLayout>());

  AddChildView(query_layout_container_);
}

void AssistantMainStage::OnCommittedQueryChanged(const AssistantQuery& query) {
  // The pending query has been committed. Update our pointers.
  committed_query_view_ = pending_query_view_;
  pending_query_view_ = nullptr;

  // Update the view.
  committed_query_view_->SetQuery(query);

  // When the motion spec is disabled, we will immediately activate the
  // committed query to prevent deviation from current UI behavior. When the
  // motion spec is enabled, we activate the query upon receipt of the response.
  if (!assistant::ui::kIsMotionSpecEnabled)
    OnActivateQuery();
}

void AssistantMainStage::OnActivateQuery() {
  // Clear the previously active query.
  OnActiveQueryCleared();

  active_query_view_ = committed_query_view_;
  committed_query_view_ = nullptr;

  // Upon response delivery, we consider a query active. The view for the query
  // should move from the bottom of its parent to the top. This transition is
  // animated when the motion spec is enabled.
  if (!assistant::ui::kIsMotionSpecEnabled) {
    active_query_view_->layer()->SetTransform(gfx::Transform());
  } else {
    using namespace assistant::util;
    active_query_view_->layer()->GetAnimator()->StartTogether(
        {// Animate transformation.
         CreateLayerAnimationSequence(
             // Pause...
             ui::LayerAnimationElement::CreatePauseElement(
                 ui::LayerAnimationElement::AnimatableProperty::TRANSFORM,
                 kAnimationTranslateUpDelay),
             // ...then translate up to top of parent.
             CreateTransformElement(gfx::Transform(),
                                    kAnimationTranslateUpDuration,
                                    gfx::Tween::Type::FAST_OUT_SLOW_IN_2)),
         // Animate opacity.
         CreateLayerAnimationSequence(
             // Fade out to 0%...
             CreateOpacityElement(0.f, kAnimationFadeOutDuration),
             // ...then pause...
             ui::LayerAnimationElement::CreatePauseElement(
                 ui::LayerAnimationElement::AnimatableProperty::OPACITY,
                 kAnimationFadeInDelay),
             // ...then fade in to 100%.
             CreateOpacityElement(1.f, kAnimationFadeInDuration))});
  }

  UpdateActiveQueryViewSpacer();
  UpdateSuggestionContainer();
}

void AssistantMainStage::OnActiveQueryCleared() {
  if (!active_query_view_)
    return;

  if (!assistant::ui::kIsMotionSpecEnabled) {
    delete active_query_view_;
    active_query_view_ = nullptr;
    return;
  }

  using namespace assistant::util;

  // The active query view will translate off stage.
  gfx::Transform transform;
  transform.Translate(0, kAnimationExitTranslationDip);

  // Animate the exit of the action query view.
  StartLayerAnimationSequencesTogether(
      active_query_view_->layer()->GetAnimator(),
      {// Animate transformation.
       CreateLayerAnimationSequence(
           CreateTransformElement(transform, kAnimationExitTranslateDuration,
                                  gfx::Tween::Type::FAST_OUT_SLOW_IN_2)),
       // Animate opacity to 0%.
       CreateLayerAnimationSequence(
           CreateOpacityElement(0.f, kAnimationExitFadeDuration))},
      // Observe the animation.
      active_query_exit_animation_observer_.get());

  // Set the animation observer to active so that we receive callback events.
  active_query_exit_animation_observer_->SetActive();

  // Note that we have not yet deleted the query view but it will be cleaned up
  // when the animation observer callback runs.
  active_query_view_ = nullptr;
}

bool AssistantMainStage::OnActiveQueryExitAnimationEnded(
    const ui::CallbackLayerAnimationObserver& observer) {
  // The exited active query view will always be the first child of its parent.
  delete query_layout_container_->child_at(0);
  UpdateActiveQueryViewSpacer();

  // Return false to prevent the observer from destroying itself.
  return false;
}

void AssistantMainStage::OnPendingQueryChanged(const AssistantQuery& query) {
  // It is possible for the user to pend multiple queries in rapid succession.
  // When this happens, a new query can be pended before the previously
  // committed query was answered and activated. When this occurs, we discard
  // the view for the previously committed query as it has been aborted.
  if (committed_query_view_) {
    delete committed_query_view_;
    committed_query_view_ = nullptr;
  }

  if (!pending_query_view_) {
    pending_query_view_ = new AssistantQueryView();
    pending_query_view_->AddObserver(this);

    // The query view will be animated on its own layer.
    pending_query_view_->SetPaintToLayer();
    pending_query_view_->layer()->SetFillsBoundsOpaquely(false);

    query_layout_container_->AddChildView(pending_query_view_);

    if (assistant::ui::kIsMotionSpecEnabled) {
      using namespace assistant::util;

      // Starting from 0% opacity...
      pending_query_view_->layer()->SetOpacity(0.f);

      // ...animate the pending query view to 100% opacity.
      pending_query_view_->layer()->GetAnimator()->StartAnimation(
          CreateLayerAnimationSequence(
              CreateOpacityElement(1.f, kPendingQueryAnimationFadeInDuration)));
    }

    UpdateSuggestionContainer();
  }

  pending_query_view_->SetQuery(query);
}

void AssistantMainStage::OnPendingQueryCleared() {
  if (pending_query_view_) {
    delete pending_query_view_;
    pending_query_view_ = nullptr;
  }

  // If the pending query is cleared but a committed query exists, we don't
  // need to update the suggestions container because the suggestion container
  // visibility state should not have changed.
  if (!committed_query_view_)
    UpdateSuggestionContainer();
}

void AssistantMainStage::OnResponseChanged(const AssistantResponse& response) {
  // If the motion spec is enabled, we only consider the query active once
  // the response has been received. When the motion spec is disabled, we
  // immediately activate the query as it is committed.
  if (assistant::ui::kIsMotionSpecEnabled)
    OnActivateQuery();
}

void AssistantMainStage::OnUiVisibilityChanged(bool visible,
                                               AssistantSource source) {
  if (visible)
    return;

  delete active_query_view_;
  active_query_view_ = nullptr;

  delete committed_query_view_;
  committed_query_view_ = nullptr;

  delete pending_query_view_;
  pending_query_view_ = nullptr;

  UpdateActiveQueryViewSpacer();
  UpdateSuggestionContainer();
}

void AssistantMainStage::UpdateActiveQueryViewSpacer() {
  // The spacer reserves room in the layout for the active query view, so it
  // needs to match its size.
  active_query_view_spacer_->SetPreferredSize(
      active_query_view_ ? active_query_view_->size() : gfx::Size());
}

void AssistantMainStage::UpdateQueryViewTransform(views::View* query_view) {
  // Unless activated, a view for a query should be bottom aligned in its
  // parent. We calculate a top offset and apply a transformation to the query
  // view to have that effect and animate the transformation back to identity
  // when the query is activated on delivery of its response.
  DCHECK_NE(query_view, active_query_view_);

  const int top_offset =
      query_layout_container_->height() - query_view->height();

  gfx::Transform transform;
  transform.Translate(0, top_offset);
  query_view->layer()->SetTransform(transform);
}

void AssistantMainStage::UpdateSuggestionContainer() {
  // The suggestion container is only visible when the committed/pending query
  // views are not. When it is not visible, it should not process events.
  bool visible = !committed_query_view_ && !pending_query_view_;

  if (!assistant::ui::kIsMotionSpecEnabled) {
    suggestion_container_->layer()->SetOpacity(visible ? 1.f : 0.f);
    suggestion_container_->set_can_process_events_within_subtree(
        visible ? true : false);
    return;
  }

  using namespace assistant::util;

  if (visible) {
    // The suggestion container will animate up into position so we need to set
    // an initial offset transformation from which to animate.
    gfx::Transform transform;
    transform.Translate(0, kSuggestionContainerAnimationTranslationDip);
    suggestion_container_->layer()->SetTransform(transform);

    // Animate the entry of the suggestion container.
    StartLayerAnimationSequencesTogether(
        suggestion_container_->layer()->GetAnimator(),
        {// Animate the translation with delay.
         CreateLayerAnimationSequence(
             ui::LayerAnimationElement::CreatePauseElement(
                 ui::LayerAnimationElement::AnimatableProperty::TRANSFORM,
                 kSuggestionContainerAnimationTranslationDelay),
             CreateTransformElement(
                 gfx::Transform(),
                 kSuggestionContainerAnimationTranslationDuration,
                 gfx::Tween::Type::FAST_OUT_SLOW_IN_2)),
         // Animate the fade in with delay.
         CreateLayerAnimationSequence(
             ui::LayerAnimationElement::CreatePauseElement(
                 ui::LayerAnimationElement::AnimatableProperty::OPACITY,
                 kSuggestionContainerAnimationFadeInDelay),
             CreateOpacityElement(
                 1.f, kSuggestionContainerAnimationFadeInDuration))},
        // Observer animation start/end events.
        suggestion_container_animation_observer_.get());
  } else {
    // Animate the exit of the suggestion container.
    StartLayerAnimationSequence(
        suggestion_container_->layer()->GetAnimator(),
        // Animate fade out.
        CreateLayerAnimationSequence(CreateOpacityElement(
            0.f, kSuggestionContainerAnimationFadeOutDuration)),
        // Observe animation start/end events.
        suggestion_container_animation_observer_.get());
  }

  // Set the observer to active so that we'll receive start/end events.
  suggestion_container_animation_observer_->SetActive();
}

void AssistantMainStage::OnSuggestionContainerAnimationStarted(
    const ui::CallbackLayerAnimationObserver& observer) {
  // The suggestion container should not process events while animating.
  suggestion_container_->set_can_process_events_within_subtree(false);
}

bool AssistantMainStage::OnSuggestionContainerAnimationEnded(
    const ui::CallbackLayerAnimationObserver& observer) {
  // The suggestion container should only process events when visible. It is
  // only visible when there is no committed or pending query view.
  suggestion_container_->set_can_process_events_within_subtree(
      !committed_query_view_ && !pending_query_view_);

  // Return false so that the observer does not destroy itself.
  return false;
}

}  // namespace ash
