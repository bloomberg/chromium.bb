// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/ash_assistant_controller.h"

#include "ash/assistant/model/assistant_interaction_model_observer.h"
#include "ash/assistant/model/assistant_ui_element.h"
#include "ash/assistant/ui/assistant_bubble.h"
#include "ash/session/session_controller.h"
#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "base/unguessable_token.h"

namespace ash {

namespace {

constexpr base::TimeDelta kAutoDismissDelay = base::TimeDelta::FromSeconds(5);

}  // namespace

AshAssistantController::AshAssistantController()
    : assistant_controller_binding_(this),
      assistant_event_subscriber_binding_(this),
      assistant_bubble_(std::make_unique<AssistantBubble>(this)) {
  AddInteractionModelObserver(this);
}

AshAssistantController::~AshAssistantController() {
  RemoveInteractionModelObserver(this);

  assistant_controller_binding_.Close();
  assistant_event_subscriber_binding_.Close();
}

void AshAssistantController::BindRequest(
    mojom::AshAssistantControllerRequest request) {
  assistant_controller_binding_.Bind(std::move(request));
}

void AshAssistantController::SetAssistant(
    chromeos::assistant::mojom::AssistantPtr assistant) {
  assistant_ = std::move(assistant);

  // Subscribe to Assistant events.
  chromeos::assistant::mojom::AssistantEventSubscriberPtr ptr;
  assistant_event_subscriber_binding_.Bind(mojo::MakeRequest(&ptr));
  assistant_->AddAssistantEventSubscriber(std::move(ptr));
}

void AshAssistantController::SetAssistantCardRenderer(
    mojom::AssistantCardRendererPtr assistant_card_renderer) {
  assistant_card_renderer_ = std::move(assistant_card_renderer);
}

void AshAssistantController::RenderCard(
    const base::UnguessableToken& id_token,
    mojom::AssistantCardParamsPtr params,
    mojom::AssistantCardRenderer::RenderCallback callback) {
  DCHECK(assistant_card_renderer_);

  const mojom::UserSession* user_session =
      Shell::Get()->session_controller()->GetUserSession(0);

  if (!user_session) {
    LOG(WARNING) << "Unable to retrieve active user session.";
    return;
  }

  AccountId account_id = user_session->user_info->account_id;

  assistant_card_renderer_->Render(account_id, id_token, std::move(params),
                                   std::move(callback));
}

void AshAssistantController::ReleaseCard(
    const base::UnguessableToken& id_token) {
  DCHECK(assistant_card_renderer_);
  assistant_card_renderer_->Release(id_token);
}

void AshAssistantController::ReleaseCards(
    const std::vector<base::UnguessableToken>& id_tokens) {
  DCHECK(assistant_card_renderer_);
  assistant_card_renderer_->ReleaseAll(id_tokens);
}

void AshAssistantController::AddInteractionModelObserver(
    AssistantInteractionModelObserver* observer) {
  assistant_interaction_model_.AddObserver(observer);
}

void AshAssistantController::RemoveInteractionModelObserver(
    AssistantInteractionModelObserver* observer) {
  assistant_interaction_model_.RemoveObserver(observer);
}

void AshAssistantController::StartInteraction() {
  // TODO(dmblack): Instruct underlying service to start listening if current
  // input modality is VOICE.
  if (assistant_interaction_model_.interaction_state() ==
      InteractionState::kInactive) {
    OnInteractionStarted();
  }
}

void AshAssistantController::StopInteraction() {
  // TODO(dmblack): Instruct underlying service to stop listening.
  if (assistant_interaction_model_.interaction_state() !=
      InteractionState::kInactive) {
    OnInteractionDismissed();
  }
}

void AshAssistantController::ToggleInteraction() {
  if (assistant_interaction_model_.interaction_state() ==
      InteractionState::kInactive) {
    StartInteraction();
  } else {
    StopInteraction();
  }
}

void AshAssistantController::OnInteractionStateChanged(
    InteractionState interaction_state) {
  if (interaction_state == InteractionState::kInactive) {
    assistant_interaction_model_.ClearInteraction();

    // TODO(dmblack): Input modality should default back to the user's
    // preferred input modality.
    assistant_interaction_model_.SetInputModality(InputModality::kVoice);
  }
}

void AshAssistantController::OnInteractionStarted() {
  assistant_bubble_timer_.Stop();
  assistant_interaction_model_.SetInteractionState(InteractionState::kActive);
}

void AshAssistantController::OnInteractionFinished(
    chromeos::assistant::mojom::AssistantInteractionResolution resolution) {
  assistant_bubble_timer_.Start(
      FROM_HERE, kAutoDismissDelay, this,
      &AshAssistantController::OnInteractionDismissed);
}

void AshAssistantController::OnInteractionDismissed() {
  assistant_bubble_timer_.Stop();
  assistant_interaction_model_.SetInteractionState(InteractionState::kInactive);
}

void AshAssistantController::OnDialogPlateContentsChanged(
    const std::string& text) {
  assistant_bubble_timer_.Stop();

  if (text.empty()) {
    // Note: This does not open the mic. It only updates the input modality to
    // voice so that we will show the mic icon in the UI.
    assistant_interaction_model_.SetInputModality(InputModality::kVoice);
  } else {
    // TODO(dmblack): Instruct the underlying service to stop any in flight
    // voice interaction.
    assistant_interaction_model_.SetInputModality(InputModality::kKeyboard);
    assistant_interaction_model_.SetMicState(MicState::kClosed);
  }
}

void AshAssistantController::OnDialogPlateContentsCommitted(
    const std::string& text) {
  Query query;
  query.high_confidence_text = text;

  assistant_interaction_model_.ClearInteraction();
  assistant_interaction_model_.SetQuery(query);

  // Note: This does not open the mic. It only updates the input modality to
  // voice so that we will show the mic icon in the UI.
  assistant_interaction_model_.SetInputModality(InputModality::kVoice);

  DCHECK(assistant_);
  assistant_->SendTextQuery(text);
}

void AshAssistantController::OnHtmlResponse(const std::string& response) {
  assistant_interaction_model_.AddUiElement(
      std::make_unique<AssistantCardElement>(response));
}

void AshAssistantController::OnSuggestionChipPressed(const std::string& text) {
  Query query;
  query.high_confidence_text = text;

  assistant_interaction_model_.ClearInteraction();
  assistant_interaction_model_.SetQuery(query);

  DCHECK(assistant_);
  assistant_->SendTextQuery(text);
}

void AshAssistantController::OnSuggestionsResponse(
    const std::vector<std::string>& response) {
  assistant_interaction_model_.AddSuggestions(response);
}

void AshAssistantController::OnTextResponse(const std::string& response) {
  assistant_interaction_model_.AddUiElement(
      std::make_unique<AssistantTextElement>(response));
}

void AshAssistantController::OnSpeechRecognitionStarted() {
  assistant_interaction_model_.ClearInteraction();
  assistant_interaction_model_.SetInputModality(InputModality::kVoice);
  assistant_interaction_model_.SetMicState(MicState::kOpen);
}

void AshAssistantController::OnSpeechRecognitionIntermediateResult(
    const std::string& high_confidence_text,
    const std::string& low_confidence_text) {
  assistant_interaction_model_.SetQuery(
      {high_confidence_text, low_confidence_text});
}

void AshAssistantController::OnSpeechRecognitionEndOfUtterance() {
  assistant_interaction_model_.SetMicState(MicState::kClosed);
}

void AshAssistantController::OnSpeechRecognitionFinalResult(
    const std::string& final_result) {
  Query query;
  query.high_confidence_text = final_result;
  assistant_interaction_model_.SetQuery(query);
}

void AshAssistantController::OnSpeechLevelUpdated(float speech_level) {
  // TODO(dmblack): Handle.
  NOTIMPLEMENTED();
}

void AshAssistantController::OnOpenUrlResponse(const GURL& url) {
  Shell::Get()->shell_delegate()->OpenUrlFromArc(url);
  StopInteraction();
}

}  // namespace ash
