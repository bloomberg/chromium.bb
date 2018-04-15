// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/ash_assistant_controller.h"

#include "ash/shell.h"
#include "ash/shell_delegate.h"

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
  // Subscribe to Assistant events.
  chromeos::assistant::mojom::AssistantEventSubscriberPtr ptr;
  assistant_event_subscriber_binding_.Bind(mojo::MakeRequest(&ptr));
  assistant->AddAssistantEventSubscriber(std::move(ptr));
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

  app_list::RecognizedSpeech recognized_speech{high_confidence_text,
                                               low_confidence_text};
  assistant_interaction_model_.SetRecognizedSpeech(recognized_speech);
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

  app_list::RecognizedSpeech recognized_speech;
  recognized_speech.high_confidence_text = final_result;
  assistant_interaction_model_.SetRecognizedSpeech(recognized_speech);
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
