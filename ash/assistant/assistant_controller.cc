// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/assistant_controller.h"

#include "ash/assistant/model/assistant_interaction_model_observer.h"
#include "ash/assistant/model/assistant_ui_element.h"
#include "ash/assistant/ui/assistant_bubble.h"
#include "ash/session/session_controller.h"
#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "base/unguessable_token.h"

namespace ash {

AssistantController::AssistantController()
    : assistant_controller_binding_(this),
      assistant_event_subscriber_binding_(this),
      assistant_bubble_(std::make_unique<AssistantBubble>(this)) {
  AddInteractionModelObserver(this);
  Shell::Get()->highlighter_controller()->AddObserver(this);
}

AssistantController::~AssistantController() {
  Shell::Get()->highlighter_controller()->RemoveObserver(this);
  RemoveInteractionModelObserver(this);

  assistant_controller_binding_.Close();
  assistant_event_subscriber_binding_.Close();
}

void AssistantController::BindRequest(
    mojom::AssistantControllerRequest request) {
  assistant_controller_binding_.Bind(std::move(request));
}

void AssistantController::SetAssistant(
    chromeos::assistant::mojom::AssistantPtr assistant) {
  assistant_ = std::move(assistant);

  // Subscribe to Assistant events.
  chromeos::assistant::mojom::AssistantEventSubscriberPtr ptr;
  assistant_event_subscriber_binding_.Bind(mojo::MakeRequest(&ptr));
  assistant_->AddAssistantEventSubscriber(std::move(ptr));
}

void AssistantController::SetAssistantCardRenderer(
    mojom::AssistantCardRendererPtr assistant_card_renderer) {
  assistant_card_renderer_ = std::move(assistant_card_renderer);
}

void AssistantController::RenderCard(
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

void AssistantController::ReleaseCard(const base::UnguessableToken& id_token) {
  DCHECK(assistant_card_renderer_);
  assistant_card_renderer_->Release(id_token);
}

void AssistantController::ReleaseCards(
    const std::vector<base::UnguessableToken>& id_tokens) {
  DCHECK(assistant_card_renderer_);
  assistant_card_renderer_->ReleaseAll(id_tokens);
}

void AssistantController::AddInteractionModelObserver(
    AssistantInteractionModelObserver* observer) {
  assistant_interaction_model_.AddObserver(observer);
}

void AssistantController::RemoveInteractionModelObserver(
    AssistantInteractionModelObserver* observer) {
  assistant_interaction_model_.RemoveObserver(observer);
}

void AssistantController::StartInteraction() {
  // TODO(dmblack): Instruct underlying service to start listening if current
  // input modality is VOICE.
  if (assistant_interaction_model_.interaction_state() ==
      InteractionState::kInactive) {
    OnInteractionStarted();
  }
}

void AssistantController::StopInteraction() {
  if (assistant_interaction_model_.interaction_state() !=
      InteractionState::kInactive) {
    OnInteractionDismissed();
  }
}

void AssistantController::ToggleInteraction() {
  if (assistant_interaction_model_.interaction_state() ==
      InteractionState::kInactive) {
    StartInteraction();
  } else {
    StopInteraction();
  }
}

void AssistantController::OnInteractionStateChanged(
    InteractionState interaction_state) {
  if (interaction_state == InteractionState::kInactive) {
    assistant_interaction_model_.ClearInteraction();

    // TODO(dmblack): Input modality should default back to the user's
    // preferred input modality.
    assistant_interaction_model_.SetInputModality(InputModality::kVoice);
  }
}

void AssistantController::OnHighlighterEnabledChanged(bool enabled) {
  // TODO(warx): add a reason enum to distinguish the case of deselecting the
  // tool and done with a stylus selection.
  assistant_interaction_model_.SetInputModality(InputModality::kStylus);
  assistant_interaction_model_.SetInteractionState(
      enabled ? InteractionState::kActive : InteractionState::kInactive);
}

void AssistantController::OnInteractionStarted() {
  assistant_interaction_model_.SetInteractionState(InteractionState::kActive);
}

void AssistantController::OnInteractionFinished(
    AssistantInteractionResolution resolution) {}

void AssistantController::OnInteractionDismissed() {
  assistant_interaction_model_.SetInteractionState(InteractionState::kInactive);

  // When the user-facing interaction is dismissed, we instruct the service to
  // terminate any listening, speaking, or query in flight.
  DCHECK(assistant_);
  assistant_->StopActiveInteraction();
}

void AssistantController::OnDialogPlateContentsChanged(
    const std::string& text) {
  if (text.empty()) {
    // Note: This does not open the mic. It only updates the input modality to
    // voice so that we will show the mic icon in the UI.
    assistant_interaction_model_.SetInputModality(InputModality::kVoice);
  } else {
    // If switching to keyboard modality from voice, we instruct the service to
    // terminate any listening, speaking, or query in flight.
    // TODO(dmblack): We probably want this same logic when switching to any
    // modality from voice. Handle this instead in OnInputModalityChanged, but
    // we will need to add a previous modality parameter to that API.
    if (assistant_interaction_model_.input_modality() ==
        InputModality::kVoice) {
      DCHECK(assistant_);
      assistant_->StopActiveInteraction();
    }
    assistant_interaction_model_.SetInputModality(InputModality::kKeyboard);
    assistant_interaction_model_.SetMicState(MicState::kClosed);
  }
}

void AssistantController::OnDialogPlateContentsCommitted(
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

void AssistantController::OnHtmlResponse(const std::string& response) {
  assistant_interaction_model_.AddUiElement(
      std::make_unique<AssistantCardElement>(response));
}

void AssistantController::OnSuggestionChipPressed(const std::string& text) {
  Query query;
  query.high_confidence_text = text;

  assistant_interaction_model_.ClearInteraction();
  assistant_interaction_model_.SetQuery(query);

  DCHECK(assistant_);
  assistant_->SendTextQuery(text);
}

void AssistantController::OnSuggestionsResponse(
    std::vector<AssistantSuggestionPtr> response) {
  assistant_interaction_model_.AddSuggestions(std::move(response));
}

void AssistantController::OnTextResponse(const std::string& response) {
  assistant_interaction_model_.AddUiElement(
      std::make_unique<AssistantTextElement>(response));
}

void AssistantController::OnSpeechRecognitionStarted() {
  assistant_interaction_model_.ClearInteraction();
  assistant_interaction_model_.SetInputModality(InputModality::kVoice);
  assistant_interaction_model_.SetMicState(MicState::kOpen);
}

void AssistantController::OnSpeechRecognitionIntermediateResult(
    const std::string& high_confidence_text,
    const std::string& low_confidence_text) {
  assistant_interaction_model_.SetQuery(
      {high_confidence_text, low_confidence_text});
}

void AssistantController::OnSpeechRecognitionEndOfUtterance() {
  assistant_interaction_model_.SetMicState(MicState::kClosed);
}

void AssistantController::OnSpeechRecognitionFinalResult(
    const std::string& final_result) {
  Query query;
  query.high_confidence_text = final_result;
  assistant_interaction_model_.SetQuery(query);
}

void AssistantController::OnSpeechLevelUpdated(float speech_level) {
  // TODO(dmblack): Handle.
  NOTIMPLEMENTED();
}

void AssistantController::OnOpenUrlResponse(const GURL& url) {
  Shell::Get()->shell_delegate()->OpenUrlFromArc(url);
  StopInteraction();
}

}  // namespace ash
