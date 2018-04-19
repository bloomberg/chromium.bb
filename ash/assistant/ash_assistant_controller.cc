// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/ash_assistant_controller.h"

#include "ash/session/session_controller.h"
#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "base/unguessable_token.h"
#include "ui/app_list/assistant_interaction_model_observer.h"

namespace ash {

AshAssistantController::AshAssistantController()
    : assistant_controller_binding_(this),
      assistant_event_subscriber_binding_(this) {
  Shell::Get()->AddShellObserver(this);
}

AshAssistantController::~AshAssistantController() {
  assistant_controller_binding_.Close();
  assistant_event_subscriber_binding_.Close();

  Shell::Get()->RemoveShellObserver(this);
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

void AshAssistantController::AddInteractionModelObserver(
    app_list::AssistantInteractionModelObserver* observer) {
  assistant_interaction_model_.AddObserver(observer);
}

void AshAssistantController::RemoveInteractionModelObserver(
    app_list::AssistantInteractionModelObserver* observer) {
  assistant_interaction_model_.RemoveObserver(observer);
}

void AshAssistantController::OnInteractionStarted() {
  // TODO(dmblack): Handle.
  NOTIMPLEMENTED();
}

void AshAssistantController::OnInteractionFinished(
    chromeos::assistant::mojom::AssistantInteractionResolution resolution) {
  // TODO(dmblack): Handle.
  NOTIMPLEMENTED();
}

void AshAssistantController::OnHtmlResponse(const std::string& response) {
  if (!is_app_list_shown_)
    return;

  assistant_interaction_model_.SetCard(response);
}

void AshAssistantController::OnSuggestionChipPressed(const std::string& text) {
  app_list::Query query;
  query.high_confidence_text = text;

  assistant_interaction_model_.ClearInteraction();
  assistant_interaction_model_.SetQuery(query);

  DCHECK(assistant_);
  assistant_->SendTextQuery(text);
}

void AshAssistantController::OnSuggestionsResponse(
    const std::vector<std::string>& response) {
  if (!is_app_list_shown_)
    return;

  assistant_interaction_model_.AddSuggestions(response);
}

void AshAssistantController::OnTextResponse(const std::string& response) {
  if (!is_app_list_shown_)
    return;

  assistant_interaction_model_.AddText(response);
}

void AshAssistantController::OnSpeechRecognitionStarted() {
  if (!is_app_list_shown_)
    return;

  assistant_interaction_model_.ClearInteraction();
}

void AshAssistantController::OnSpeechRecognitionIntermediateResult(
    const std::string& high_confidence_text,
    const std::string& low_confidence_text) {
  if (!is_app_list_shown_)
    return;

  assistant_interaction_model_.SetQuery(
      {high_confidence_text, low_confidence_text});
}

void AshAssistantController::OnSpeechRecognitionEndOfUtterance() {
  if (!is_app_list_shown_)
    return;

  // TODO(dmblack): Handle.
  NOTIMPLEMENTED();
}

void AshAssistantController::OnSpeechRecognitionFinalResult(
    const std::string& final_result) {
  if (!is_app_list_shown_)
    return;

  app_list::Query query;
  query.high_confidence_text = final_result;
  assistant_interaction_model_.SetQuery(query);
}

void AshAssistantController::OnSpeechLevelUpdated(float speech_level) {
  if (!is_app_list_shown_)
    return;

  // TODO(dmblack): Handle.
  NOTIMPLEMENTED();
}

void AshAssistantController::OnOpenUrlResponse(const GURL& url) {
  Shell::Get()->shell_delegate()->OpenUrlFromArc(url);
}

// TODO(b/77637813): Remove when pulling Assistant out of launcher.
void AshAssistantController::OnAppListVisibilityChanged(
    bool shown,
    aura::Window* root_window) {
  is_app_list_shown_ = shown;
  if (!is_app_list_shown_)
    assistant_interaction_model_.ClearInteraction();
}

}  // namespace ash
