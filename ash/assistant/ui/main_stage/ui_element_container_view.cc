// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/ui/main_stage/ui_element_container_view.h"

#include "ash/assistant/assistant_controller.h"
#include "ash/assistant/assistant_interaction_controller.h"
#include "ash/assistant/model/assistant_response.h"
#include "ash/assistant/model/assistant_ui_element.h"
#include "ash/assistant/ui/assistant_ui_constants.h"
#include "ash/assistant/ui/main_stage/assistant_text_element_view.h"
#include "ash/assistant/util/animation_util.h"
#include "ash/public/cpp/app_list/answer_card_contents_registry.h"
#include "base/base64.h"
#include "base/callback.h"
#include "base/time/time.h"
#include "base/unguessable_token.h"
#include "ui/aura/window.h"
#include "ui/compositor/callback_layer_animation_observer.h"
#include "ui/compositor/layer_animation_element.h"
#include "ui/compositor/layer_animator.h"
#include "ui/views/controls/native/native_view_host.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

namespace ash {

namespace {

// Appearance.
constexpr int kPaddingHorizontalDip = 32;

// Card element animation.
constexpr float kCardElementAnimationFadeOutOpacity = 0.26f;

// Text element animation.
constexpr float kTextElementAnimationFadeOutOpacity = 0.f;

// UI element animation.
constexpr base::TimeDelta kUiElementAnimationFadeInDelay =
    base::TimeDelta::FromMilliseconds(83);
constexpr base::TimeDelta kUiElementAnimationFadeInDuration =
    base::TimeDelta::FromMilliseconds(250);
constexpr base::TimeDelta kUiElementAnimationFadeOutDuration =
    base::TimeDelta::FromMilliseconds(167);

// WebContents.
constexpr char kDataUriPrefix[] = "data:text/html;base64,";

// CardElementViewHolder -------------------------------------------------------

// This class uses a child widget to host a view for a card element that has an
// aura::Window. The child widget's layer becomes the root of the card's layer
// hierarchy.
class CardElementViewHolder : public views::NativeViewHost,
                              public views::ViewObserver {
 public:
  explicit CardElementViewHolder(views::View* card_element_view)
      : card_element_view_(card_element_view) {
    views::Widget::InitParams params(views::Widget::InitParams::TYPE_CONTROL);

    params.name = GetClassName();
    params.delegate = new views::WidgetDelegateView();
    params.opacity = views::Widget::InitParams::TRANSLUCENT_WINDOW;
    params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;

    child_widget_ = std::make_unique<views::Widget>();
    child_widget_->Init(params);

    contents_view_ = params.delegate->GetContentsView();
    contents_view_->SetLayoutManager(std::make_unique<views::FillLayout>());
    contents_view_->AddChildView(card_element_view_);

    card_element_view_->AddObserver(this);
  }

  ~CardElementViewHolder() override {
    card_element_view_->RemoveObserver(this);
  }

  // views::NativeViewHost:
  const char* GetClassName() const override { return "CardElementViewHolder"; }

  // views::ViewObserver:
  void OnViewPreferredSizeChanged(views::View* view) override {
    contents_view_->SetPreferredSize(view->GetPreferredSize());
    SetPreferredSize(view->GetPreferredSize());
  }

  void Attach() {
    views::NativeViewHost::Attach(child_widget_->GetNativeView());
  }

 private:
  views::View* const card_element_view_;  // Owned by WebContentsManager.

  std::unique_ptr<views::Widget> child_widget_;

  views::View* contents_view_ = nullptr;  // Owned by |child_widget_|.

  DISALLOW_COPY_AND_ASSIGN(CardElementViewHolder);
};

}  // namespace

// UiElementContainerView ------------------------------------------------------

UiElementContainerView::UiElementContainerView(
    AssistantController* assistant_controller)
    : assistant_controller_(assistant_controller),
      ui_elements_exit_animation_observer_(
          std::make_unique<ui::CallbackLayerAnimationObserver>(
              /*animation_ended_callback=*/base::BindRepeating(
                  &UiElementContainerView::OnAllUiElementsExitAnimationEnded,
                  base::Unretained(this)))),
      render_request_weak_factory_(this) {
  InitLayout();

  // The Assistant controller indirectly owns the view hierarchy to which
  // UiElementContainerView belongs so is guaranteed to outlive it.
  assistant_controller_->interaction_controller()->AddModelObserver(this);
}

UiElementContainerView::~UiElementContainerView() {
  assistant_controller_->interaction_controller()->RemoveModelObserver(this);
  ReleaseAllCards();
}

gfx::Size UiElementContainerView::CalculatePreferredSize() const {
  return gfx::Size(INT_MAX, GetHeightForWidth(INT_MAX));
}

int UiElementContainerView::GetHeightForWidth(int width) const {
  return content_view()->GetHeightForWidth(width);
}

void UiElementContainerView::OnContentsPreferredSizeChanged(
    views::View* content_view) {
  const int preferred_height = content_view->GetHeightForWidth(width());
  content_view->SetSize(gfx::Size(width(), preferred_height));
}

void UiElementContainerView::InitLayout() {
  views::BoxLayout* layout_manager =
      content_view()->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kVertical,
          gfx::Insets(0, kPaddingHorizontalDip), kSpacingDip));

  layout_manager->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::CROSS_AXIS_ALIGNMENT_START);
}

void UiElementContainerView::OnCommittedQueryChanged(
    const AssistantQuery& query) {
  // We don't allow processing of events while waiting for the next query
  // response. The contents will be faded out, so it should not be interactive.
  // We also scroll to the top to play nice with the transition animation.
  set_can_process_events_within_subtree(false);
  ScrollToPosition(vertical_scroll_bar(), 0);

  if (!assistant::ui::kIsMotionSpecEnabled)
    return;

  // When a query is committed, we fade out the views for the previous response
  // until the next Assistant response has been received.
  using namespace assistant::util;
  for (const std::pair<ui::Layer*, float>& pair : ui_element_layers_) {
    pair.first->GetAnimator()->StartAnimation(
        CreateLayerAnimationSequence(CreateOpacityElement(
            /*opacity=*/pair.second, kUiElementAnimationFadeOutDuration)));
  }
}

void UiElementContainerView::OnResponseChanged(
    const AssistantResponse& response) {
  // If the motion spec is disabled, we clear any previous response and then
  // add the new response in a single step.
  if (!assistant::ui::kIsMotionSpecEnabled) {
    OnResponseCleared();
    OnResponseAdded(response);
    return;
  }

  // If the motion spec is enabled but we haven't cached any layers, there is
  // nothing to animate off stage so we can proceed to add the new response.
  if (ui_element_layers_.empty()) {
    OnResponseAdded(response);
    return;
  }

  using namespace assistant::util;

  // There is a previous response on stage, so we'll animate it off before
  // adding the new response. The new response will be added upon invocation of
  // the exit animation ended callback.
  for (const std::pair<ui::Layer*, float> pair : ui_element_layers_) {
    StartLayerAnimationSequence(
        pair.first->GetAnimator(),
        // Fade out the opacity to 0%.
        CreateLayerAnimationSequence(
            CreateOpacityElement(0.f, kUiElementAnimationFadeOutDuration)),
        // Observe the animation.
        ui_elements_exit_animation_observer_.get());
  }

  // Set the observer to active so that we receive callback events.
  ui_elements_exit_animation_observer_->SetActive();
}

void UiElementContainerView::OnResponseCleared() {
  // Prevent any in-flight card rendering requests from returning.
  render_request_weak_factory_.InvalidateWeakPtrs();

  content_view()->RemoveAllChildViews(/*delete_children=*/true);
  ui_element_layers_.clear();

  ReleaseAllCards();

  // We can clear any pending UI elements as they are no longer relevant.
  pending_ui_element_list_.clear();
  SetProcessingUiElement(false);
}

void UiElementContainerView::OnResponseAdded(
    const AssistantResponse& response) {
  for (const std::unique_ptr<AssistantUiElement>& ui_element :
       response.GetUiElements()) {
    // If we are processing a UI element we need to pend the incoming elements
    // instead of handling them immediately.
    if (is_processing_ui_element_) {
      pending_ui_element_list_.push_back(ui_element.get());
      continue;
    }
    OnUiElementAdded(ui_element.get());
  }

  // If we're no longer processing any UI elements, then all UI elements have
  // been successfully added.
  if (!is_processing_ui_element_)
    OnAllUiElementsAdded();
}

void UiElementContainerView::OnAllUiElementsAdded() {
  DCHECK(!is_processing_ui_element_);

  // Now that the response for the current query has been added to the view
  // hierarchy, we can re-enable processing of events.
  set_can_process_events_within_subtree(true);

  // If the motion spec is disabled, there's nothing to do because the views
  // do not need to be animated in.
  if (!assistant::ui::kIsMotionSpecEnabled)
    return;

  using namespace assistant::util;

  // Now that we've received and added all UI elements for the current query
  // response, we can animate them in.
  for (const std::pair<ui::Layer*, float>& pair : ui_element_layers_) {
    // We fade in the views to full opacity after a slight delay.
    pair.first->GetAnimator()->StartAnimation(CreateLayerAnimationSequence(
        ui::LayerAnimationElement::CreatePauseElement(
            ui::LayerAnimationElement::AnimatableProperty::OPACITY,
            kUiElementAnimationFadeInDelay),
        CreateOpacityElement(1.f, kUiElementAnimationFadeInDuration)));
  }
}

bool UiElementContainerView::OnAllUiElementsExitAnimationEnded(
    const ui::CallbackLayerAnimationObserver& observer) {
  // All UI elements have finished their exit animations so its safe to perform
  // clearing of their views and managed resources.
  OnResponseCleared();

  const AssistantResponse* response =
      assistant_controller_->interaction_controller()->model()->response();

  // If there is a response present (and there should be), it is safe to add it
  // now that we've cleared the previous content from the stage.
  if (response)
    OnResponseAdded(*response);

  // Return false to prevent the observer from destroying itself.
  return false;
}

void UiElementContainerView::OnUiElementAdded(
    const AssistantUiElement* ui_element) {
  switch (ui_element->GetType()) {
    case AssistantUiElementType::kCard:
      OnCardElementAdded(static_cast<const AssistantCardElement*>(ui_element));
      break;
    case AssistantUiElementType::kText:
      OnTextElementAdded(static_cast<const AssistantTextElement*>(ui_element));
      break;
  }
}

void UiElementContainerView::OnCardElementAdded(
    const AssistantCardElement* card_element) {
  DCHECK(!is_processing_ui_element_);

  // We need to pend any further UI elements until the card has been rendered.
  // This insures that views will be added to the view hierarchy in the order in
  // which they were received.
  SetProcessingUiElement(true);

  // Generate a unique identifier for the card. This will be used to clean up
  // card resources when it is no longer needed.
  base::UnguessableToken id_token = base::UnguessableToken::Create();

  // Encode the card HTML in base64.
  std::string encoded_html;
  base::Base64Encode(card_element->html(), &encoded_html);

  // Configure parameters for the card.
  ash::mojom::ManagedWebContentsParamsPtr params(
      ash::mojom::ManagedWebContentsParams::New());
  params->url = GURL(kDataUriPrefix + encoded_html);
  params->min_size_dip =
      gfx::Size(kPreferredWidthDip - 2 * kPaddingHorizontalDip, 1);
  params->max_size_dip =
      gfx::Size(kPreferredWidthDip - 2 * kPaddingHorizontalDip, INT_MAX);

  // The card will be rendered by AssistantCardRenderer, running the specified
  // callback when the card is ready for embedding.
  assistant_controller_->ManageWebContents(
      id_token, std::move(params),
      base::BindOnce(&UiElementContainerView::OnCardReady,
                     render_request_weak_factory_.GetWeakPtr()));

  // Cache the card identifier for freeing up resources when it is no longer
  // needed.
  id_token_list_.push_back(id_token);
}

void UiElementContainerView::OnCardReady(
    const base::Optional<base::UnguessableToken>& embed_token) {
  if (!embed_token.has_value()) {
    // TODO(dmblack): Maybe show a fallback view here?
    // Something went wrong when processing this card so we'll have to abort
    // the attempt. We should still resume processing any UI elements that are
    // in the pending queue.
    SetProcessingUiElement(false);
    return;
  }

  // When the card has been rendered in the same process, its view is
  // available in the AnswerCardContentsRegistry's token-to-view map.
  if (app_list::AnswerCardContentsRegistry::Get()) {
    CardElementViewHolder* view_holder = new CardElementViewHolder(
        app_list::AnswerCardContentsRegistry::Get()->GetView(
            embed_token.value()));

    content_view()->AddChildView(view_holder);
    view_holder->Attach();

    if (assistant::ui::kIsMotionSpecEnabled) {
      // The view will be animated on its own layer, so we need to do some
      // initial layer setup. We're going to fade the view in, so hide it.
      view_holder->native_view()->layer()->SetFillsBoundsOpaquely(false);
      view_holder->native_view()->layer()->SetOpacity(0.f);

      // We cache the layer for the view for use during animations and cache
      // its desired opacity that we'll animate to while processing the next
      // query response.
      ui_element_layers_.push_back(
          std::pair<ui::Layer*, float>(view_holder->native_view()->layer(),
                                       kCardElementAnimationFadeOutOpacity));
    }
  }
  // TODO(dmblack): Handle Mash case.

  // Once the card has been rendered and embedded, we can resume processing
  // any UI elements that are in the pending queue.
  SetProcessingUiElement(false);
}

void UiElementContainerView::OnTextElementAdded(
    const AssistantTextElement* text_element) {
  DCHECK(!is_processing_ui_element_);

  views::View* text_element_view = new AssistantTextElementView(text_element);

  if (assistant::ui::kIsMotionSpecEnabled) {
    // The view will be animated on its own layer, so we need to do some initial
    // layer setup. We're going to fade the view in, so hide it.
    text_element_view->SetPaintToLayer();
    text_element_view->layer()->SetFillsBoundsOpaquely(false);
    text_element_view->layer()->SetOpacity(0.f);

    // We cache the layer for the view for use during animations and cache its
    // desired opacity that we'll animate to while processing the next query
    // response.
    ui_element_layers_.push_back(std::pair<ui::Layer*, float>(
        text_element_view->layer(), kTextElementAnimationFadeOutOpacity));
  }

  content_view()->AddChildView(text_element_view);
}

void UiElementContainerView::SetProcessingUiElement(bool is_processing) {
  if (is_processing == is_processing_ui_element_)
    return;

  is_processing_ui_element_ = is_processing;

  // If we are no longer processing a UI element, we need to handle anything
  // that was put in the pending queue. Note that the elements left in the
  // pending queue may themselves require processing that again pends the queue.
  if (!is_processing_ui_element_)
    ProcessPendingUiElements();
}

void UiElementContainerView::ProcessPendingUiElements() {
  DCHECK(!is_processing_ui_element_);

  while (!is_processing_ui_element_ && !pending_ui_element_list_.empty()) {
    const AssistantUiElement* ui_element = pending_ui_element_list_.front();
    pending_ui_element_list_.pop_front();
    OnUiElementAdded(ui_element);
  }

  // If we're no longer processing any UI elements, then all UI elements have
  // been successfully added.
  if (!is_processing_ui_element_)
    OnAllUiElementsAdded();
}

void UiElementContainerView::ReleaseAllCards() {
  if (id_token_list_.empty())
    return;

  // Release any resources associated with the cards identified in
  // |id_token_list_| owned by AssistantCardRenderer.
  assistant_controller_->ReleaseWebContents(id_token_list_);
  id_token_list_.clear();
}

}  // namespace ash
