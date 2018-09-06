// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/ui/main_stage/ui_element_container_view.h"

#include "ash/assistant/assistant_controller.h"
#include "ash/assistant/assistant_interaction_controller.h"
#include "ash/assistant/model/assistant_response.h"
#include "ash/assistant/model/assistant_ui_element.h"
#include "ash/assistant/ui/assistant_container_view.h"
#include "ash/assistant/ui/assistant_ui_constants.h"
#include "ash/assistant/ui/main_stage/assistant_text_element_view.h"
#include "ash/assistant/util/animation_util.h"
#include "ash/public/cpp/app_list/answer_card_contents_registry.h"
#include "ash/shell.h"
#include "base/base64.h"
#include "base/callback.h"
#include "base/time/time.h"
#include "base/unguessable_token.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/compositor/callback_layer_animation_observer.h"
#include "ui/compositor/layer_animation_element.h"
#include "ui/compositor/layer_animator.h"
#include "ui/events/event.h"
#include "ui/events/event_sink.h"
#include "ui/events/event_utils.h"
#include "ui/views/border.h"
#include "ui/views/controls/native/native_view_host.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

namespace ash {

namespace {

// Appearance.
constexpr int kFirstCardMarginTopDip = 40;
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

// Helpers ---------------------------------------------------------------------

void CreateAndSendMouseClick(aura::WindowTreeHost* host,
                             const gfx::Point& location_in_pixels) {
  ui::MouseEvent press_event(ui::ET_MOUSE_PRESSED, location_in_pixels,
                             location_in_pixels, ui::EventTimeForNow(),
                             ui::EF_LEFT_MOUSE_BUTTON,
                             ui::EF_LEFT_MOUSE_BUTTON);

  // Send an ET_MOUSE_PRESSED event.
  ui::EventDispatchDetails details =
      host->event_sink()->OnEventFromSource(&press_event);

  if (details.dispatcher_destroyed)
    return;

  ui::MouseEvent release_event(ui::ET_MOUSE_RELEASED, location_in_pixels,
                               location_in_pixels, ui::EventTimeForNow(),
                               ui::EF_LEFT_MOUSE_BUTTON,
                               ui::EF_LEFT_MOUSE_BUTTON);

  // Send an ET_MOUSE_RELEASED event.
  ignore_result(host->event_sink()->OnEventFromSource(&release_event));
}

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

  void OnGestureEvent(ui::GestureEvent* event) override {
    // We need to route GESTURE_TAP events to our Assistant card because links
    // should be tappable. The Assistant card window will not receive gesture
    // events so we convert the gesture into analogous mouse events.
    if (event->type() != ui::ET_GESTURE_TAP) {
      views::View::OnGestureEvent(event);
      return;
    }

    // Consume the original event.
    event->StopPropagation();
    event->SetHandled();

    aura::Window* root_window = GetWidget()->GetNativeWindow()->GetRootWindow();

    // Get the appropriate event location in pixels.
    gfx::Point location_in_pixels = event->location();
    ConvertPointToScreen(this, &location_in_pixels);
    aura::WindowTreeHost* host = root_window->GetHost();
    host->ConvertDIPToPixels(&location_in_pixels);

    wm::CursorManager* cursor_manager = Shell::Get()->cursor_manager();

    // We want to prevent the cursor from changing its visibility during our
    // mouse events because we are actually handling a gesture. To accomplish
    // this, we cache the cursor's visibility and lock it in its current state.
    const bool visible = cursor_manager->IsCursorVisible();
    cursor_manager->LockCursor();

    CreateAndSendMouseClick(host, location_in_pixels);

    // Restore the original cursor visibility that may have changed during our
    // sequence of mouse events. This change would not have been perceivable to
    // the user since it occurred within our lock.
    if (visible)
      cursor_manager->ShowCursor();
    else
      cursor_manager->HideCursor();

    // Release our cursor lock.
    cursor_manager->UnlockCursor();
  }

  // views::ViewObserver:
  void OnViewPreferredSizeChanged(views::View* view) override {
    gfx::Size preferred_size = view->GetPreferredSize();

    if (border()) {
      // When a border is present we need to explicitly account for it in our
      // size calculations by enlarging our preferred size by the border insets.
      const gfx::Insets insets = border()->GetInsets();
      preferred_size.Enlarge(insets.width(), insets.height());
    }

    contents_view_->SetPreferredSize(preferred_size);
    SetPreferredSize(preferred_size);
  }

  void Attach() {
    views::NativeViewHost::Attach(child_widget_->GetNativeView());

    aura::Window* const top_level_window = native_view()->GetToplevelWindow();

    // Find the window for the Assistant card.
    aura::Window* window = native_view();
    while (window->parent() != top_level_window)
      window = window->parent();

    // The Assistant card window will consume all events that enter it. This
    // prevents us from being able to scroll the native view hierarchy
    // vertically. As such, we need to prevent the Assistant card window from
    // receiving events it doesn't need. It needs mouse click events for
    // handling links.
    AssistantContainerView::OnlyAllowMouseClickEvents(window);
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
  using assistant::util::CreateLayerAnimationSequence;
  using assistant::util::CreateOpacityElement;

  // We don't allow processing of events while waiting for the next query
  // response. The contents will be faded out, so it should not be interactive.
  // We also scroll to the top to play nice with the transition animation.
  set_can_process_events_within_subtree(false);
  ScrollToPosition(vertical_scroll_bar(), 0);

  // When a query is committed, we fade out the views for the previous response
  // until the next Assistant response has been received.
  for (const std::pair<ui::LayerOwner*, float>& pair : ui_element_views_) {
    pair.first->layer()->GetAnimator()->StartAnimation(
        CreateLayerAnimationSequence(CreateOpacityElement(
            /*opacity=*/pair.second, kUiElementAnimationFadeOutDuration)));
  }
}

void UiElementContainerView::OnResponseChanged(
    const AssistantResponse& response) {
  // If we don't have any pre-existing content, there is nothing to animate off
  // stage so we we can proceed to add the new response.
  if (!content_view()->has_children()) {
    OnResponseAdded(response);
    return;
  }

  using assistant::util::CreateLayerAnimationSequence;
  using assistant::util::CreateOpacityElement;
  using assistant::util::StartLayerAnimationSequence;

  // There is a previous response on stage, so we'll animate it off before
  // adding the new response. The new response will be added upon invocation of
  // the exit animation ended callback.
  for (const std::pair<ui::LayerOwner*, float>& pair : ui_element_views_) {
    StartLayerAnimationSequence(
        pair.first->layer()->GetAnimator(),
        // Fade out the opacity to 0%. Note that we approximate 0% by actually
        // using 0.01%. We do this to workaround a DCHECK that requires
        // aura::Windows to have a target opacity > 0% when shown. Because our
        // window will be removed after it reaches this value, it should be safe
        // to circumnavigate this DCHECK.
        CreateLayerAnimationSequence(
            CreateOpacityElement(0.0001f, kUiElementAnimationFadeOutDuration)),
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
  ui_element_views_.clear();

  ReleaseAllCards();

  // We can clear any pending UI elements as they are no longer relevant.
  pending_ui_element_list_.clear();
  SetProcessingUiElement(false);

  // Reset state for the next response.
  is_first_card_ = true;
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

  using assistant::util::CreateLayerAnimationSequence;
  using assistant::util::CreateOpacityElement;

  // Now that the response for the current query has been added to the view
  // hierarchy, we can re-enable processing of events.
  set_can_process_events_within_subtree(true);

  // Now that we've received and added all UI elements for the current query
  // response, we can animate them in.
  for (const std::pair<ui::LayerOwner*, float>& pair : ui_element_views_) {
    // We fade in the views to full opacity after a slight delay.
    pair.first->layer()->GetAnimator()->StartAnimation(
        CreateLayerAnimationSequence(
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

    if (is_first_card_) {
      is_first_card_ = false;

      // The first card requires a top margin of |kFirstCardMarginTopDip|, but
      // we need to account for child spacing because the first card is not
      // necessarily the first UI element.
      const int top_margin_dip = child_count() == 0
                                     ? kFirstCardMarginTopDip
                                     : kFirstCardMarginTopDip - kSpacingDip;

      // We effectively create a top margin by applying an empty border.
      view_holder->SetBorder(views::CreateEmptyBorder(top_margin_dip, 0, 0, 0));
    }

    content_view()->AddChildView(view_holder);
    view_holder->Attach();

    // The view will be animated on its own layer, so we need to do some initial
    // layer setup. We're going to fade the view in, so hide it. Note that we
    // approximate 0% opacity by actually using 0.01%. We do this to workaround
    // a DCHECK that requires aura::Windows to have a target opacity > 0% when
    // shown. Because our window will be animated to full opacity from this
    // value, it should be safe to circumnavigate this DCHECK.
    view_holder->native_view()->layer()->SetFillsBoundsOpaquely(false);
    view_holder->native_view()->layer()->SetOpacity(0.0001f);

    // We cache the native view for use during animations and its desired
    // opacity that we'll animate to while processing the next query response.
    ui_element_views_.push_back(std::pair<ui::LayerOwner*, float>(
        view_holder->native_view(), kCardElementAnimationFadeOutOpacity));
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

  // The view will be animated on its own layer, so we need to do some initial
  // layer setup. We're going to fade the view in, so hide it.
  text_element_view->SetPaintToLayer();
  text_element_view->layer()->SetFillsBoundsOpaquely(false);
  text_element_view->layer()->SetOpacity(0.f);

  // We cache the view for use during animations and its desired opacity that
  // we'll animate to while processing the next query response.
  ui_element_views_.push_back(std::pair<ui::LayerOwner*, float>(
      text_element_view, kTextElementAnimationFadeOutOpacity));

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
