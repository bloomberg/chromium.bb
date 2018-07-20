// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/assistant_interaction_controller.h"

#include "ash/assistant/assistant_controller.h"
#include "ash/assistant/assistant_ui_controller.h"
#include "ash/assistant/model/assistant_interaction_model_observer.h"
#include "ash/assistant/model/assistant_query.h"
#include "ash/assistant/model/assistant_ui_element.h"
#include "ash/shell.h"
#include "base/strings/utf_string_conversions.h"

namespace ash {

AssistantInteractionController::AssistantInteractionController(
    AssistantController* assistant_controller)
    : assistant_controller_(assistant_controller),
      assistant_interaction_subscriber_binding_(this) {
  AddModelObserver(this);
  assistant_controller_->AddObserver(this);
  Shell::Get()->highlighter_controller()->AddObserver(this);
}

AssistantInteractionController::~AssistantInteractionController() {
  Shell::Get()->highlighter_controller()->RemoveObserver(this);
  assistant_controller_->RemoveObserver(this);
  RemoveModelObserver(this);
}

void AssistantInteractionController::SetAssistant(
    chromeos::assistant::mojom::Assistant* assistant) {
  assistant_ = assistant;

  // Subscribe to Assistant interaction events.
  chromeos::assistant::mojom::AssistantInteractionSubscriberPtr ptr;
  assistant_interaction_subscriber_binding_.Bind(mojo::MakeRequest(&ptr));
  assistant_->AddAssistantInteractionSubscriber(std::move(ptr));
}

void AssistantInteractionController::AddModelObserver(
    AssistantInteractionModelObserver* observer) {
  assistant_interaction_model_.AddObserver(observer);
}

void AssistantInteractionController::RemoveModelObserver(
    AssistantInteractionModelObserver* observer) {
  assistant_interaction_model_.RemoveObserver(observer);
}

void AssistantInteractionController::OnAssistantControllerConstructed() {
  assistant_controller_->ui_controller()->AddModelObserver(this);
}

void AssistantInteractionController::OnAssistantControllerDestroying() {
  assistant_controller_->ui_controller()->RemoveModelObserver(this);
}

void AssistantInteractionController::OnCommittedQueryChanged(
    const AssistantQuery& committed_query) {
  // We clear the interaction when a query is committed, but need to retain
  // the committed query as it is query that is currently being fulfilled.
  assistant_interaction_model_.ClearInteraction(
      /*retain_committed_query=*/true);
}

void AssistantInteractionController::OnUiVisibilityChanged(
    bool visible,
    AssistantSource source) {
  if (visible) {
    // TODO(dmblack): When the UI becomes visible, we may need to immediately
    // start a voice interaction depending on |source| and user preference.
    if (source == AssistantSource::kStylus)
      assistant_interaction_model_.SetInputModality(InputModality::kStylus);
    return;
  }

  // When the UI is hidden, we need to stop any active interaction. We also
  // reset the interaction state and restore the default input modality.
  StopActiveInteraction();
  assistant_interaction_model_.ClearInteraction();
  assistant_interaction_model_.SetInputModality(InputModality::kKeyboard);
}

void AssistantInteractionController::OnHighlighterEnabledChanged(
    HighlighterEnabledState state) {
  switch (state) {
    case HighlighterEnabledState::kEnabled:
      assistant_interaction_model_.SetInputModality(InputModality::kStylus);
      break;
    case HighlighterEnabledState::kDisabledByUser:
      FALLTHROUGH;
    case HighlighterEnabledState::kDisabledBySessionComplete:
      assistant_interaction_model_.SetInputModality(InputModality::kKeyboard);
      break;
    case HighlighterEnabledState::kDisabledBySessionAbort:
      // When metalayer mode has been aborted, no action necessary. Abort occurs
      // as a result of an interaction starting, most likely due to hotword
      // detection. Setting the input modality in these cases would have the
      // unintended consequence of stopping the active interaction.
      break;
  }
}

void AssistantInteractionController::OnInteractionStateChanged(
    InteractionState interaction_state) {
  if (interaction_state != InteractionState::kActive)
    return;

  // Metalayer mode should not be sticky. Disable it on interaction start.
  Shell::Get()->highlighter_controller()->AbortSession();
}

void AssistantInteractionController::OnInputModalityChanged(
    InputModality input_modality) {
  if (input_modality == InputModality::kVoice)
    return;

  // When switching to a non-voice input modality we instruct the underlying
  // service to terminate any pending query. We do not do this when switching to
  // voice input modality because initiation of a voice interaction will
  // automatically interrupt any pre-existing activity. Stopping the active
  // interaction here for voice input modality would actually have the undesired
  // effect of stopping the voice interaction.
  StopActiveInteraction();
}

void AssistantInteractionController::OnInteractionStarted(
    bool is_voice_interaction) {
  assistant_interaction_model_.SetInteractionState(InteractionState::kActive);

  // In the case of a voice interaction, we assume that the mic is open and
  // transition to voice input modality.
  if (is_voice_interaction) {
    assistant_interaction_model_.SetInputModality(InputModality::kVoice);
    assistant_interaction_model_.SetMicState(MicState::kOpen);
  } else {
    // In the case of a non-voice interaction, we commit the pending query.
    // This will trigger a clearing of the interaction which wipes the stage.
    assistant_interaction_model_.CommitPendingQuery();
    assistant_interaction_model_.SetMicState(MicState::kClosed);
  }
}

void AssistantInteractionController::OnInteractionFinished(
    AssistantInteractionResolution resolution) {
  assistant_interaction_model_.SetInteractionState(InteractionState::kInactive);

  // When a voice query is interrupted we do not receive any follow up speech
  // recognition events but the mic is closed.
  if (resolution == AssistantInteractionResolution::kInterruption) {
    assistant_interaction_model_.SetMicState(MicState::kClosed);
  }
}

void AssistantInteractionController::OnHtmlResponse(
    const std::string& response) {
  if (assistant_interaction_model_.interaction_state() !=
      InteractionState::kActive) {
    return;
  }

  assistant_interaction_model_.AddUiElement(
      std::make_unique<AssistantCardElement>(response));
}

void AssistantInteractionController::OnSuggestionChipPressed(int id) {
  const AssistantSuggestion* suggestion =
      assistant_interaction_model_.GetSuggestionById(id);

  // If the suggestion contains a non-empty action url, we will handle the
  // suggestion chip pressed event by launching the action url in the browser.
  if (!suggestion->action_url.is_empty()) {
    assistant_controller_->OpenUrl(suggestion->action_url);
    return;
  }

  // Otherwise, we will submit a simple text query using the suggestion text.
  StartTextInteraction(suggestion->text);
}

void AssistantInteractionController::OnSuggestionsResponse(
    std::vector<AssistantSuggestionPtr> response) {
  if (assistant_interaction_model_.interaction_state() !=
      InteractionState::kActive) {
    return;
  }

  assistant_interaction_model_.AddSuggestions(std::move(response));
}

void AssistantInteractionController::OnTextResponse(
    const std::string& response) {
  if (assistant_interaction_model_.interaction_state() !=
      InteractionState::kActive) {
    return;
  }

  assistant_interaction_model_.AddUiElement(
      std::make_unique<AssistantTextElement>(response));
}

void AssistantInteractionController::OnSpeechRecognitionStarted() {
  assistant_interaction_model_.SetInputModality(InputModality::kVoice);
  assistant_interaction_model_.SetMicState(MicState::kOpen);
  assistant_interaction_model_.SetPendingQuery(
      std::make_unique<AssistantVoiceQuery>());
}

void AssistantInteractionController::OnSpeechRecognitionIntermediateResult(
    const std::string& high_confidence_text,
    const std::string& low_confidence_text) {
  assistant_interaction_model_.SetPendingQuery(
      std::make_unique<AssistantVoiceQuery>(high_confidence_text,
                                            low_confidence_text));
}

void AssistantInteractionController::OnSpeechRecognitionEndOfUtterance() {
  assistant_interaction_model_.SetMicState(MicState::kClosed);
}

void AssistantInteractionController::OnSpeechRecognitionFinalResult(
    const std::string& final_result) {
  assistant_interaction_model_.SetPendingQuery(
      std::make_unique<AssistantVoiceQuery>(final_result));
  assistant_interaction_model_.CommitPendingQuery();
}

void AssistantInteractionController::OnSpeechLevelUpdated(float speech_level) {
  assistant_interaction_model_.SetSpeechLevel(speech_level);
}

void AssistantInteractionController::OnOpenUrlResponse(const GURL& url) {
  if (assistant_interaction_model_.interaction_state() !=
      InteractionState::kActive) {
    return;
  }
  assistant_controller_->OpenUrl(url);
}

void AssistantInteractionController::OnDialogPlateButtonPressed(
    DialogPlateButtonId id) {
  if (id == DialogPlateButtonId::kKeyboardInputToggle) {
    assistant_interaction_model_.SetInputModality(InputModality::kKeyboard);
    return;
  }

  if (id != DialogPlateButtonId::kVoiceInputToggle)
    return;

  switch (assistant_interaction_model_.mic_state()) {
    case MicState::kClosed:
      StartVoiceInteraction();
      break;
    case MicState::kOpen:
      StopActiveInteraction();
      break;
  }
}

void AssistantInteractionController::OnDialogPlateContentsCommitted(
    const std::string& text) {
  DCHECK(!text.empty());
  StartTextInteraction(text);
}

void AssistantInteractionController::StartTextInteraction(
    const std::string text) {
  StopActiveInteraction();

  assistant_interaction_model_.SetPendingQuery(
      std::make_unique<AssistantTextQuery>(text));

  assistant_->SendTextQuery(text);
}

void AssistantInteractionController::StartVoiceInteraction() {
  StopActiveInteraction();

  assistant_interaction_model_.SetPendingQuery(
      std::make_unique<AssistantVoiceQuery>());

  assistant_->StartVoiceInteraction();
}

void AssistantInteractionController::StopActiveInteraction() {
  // Even though the interaction state will be asynchronously set to inactive
  // via a call to OnInteractionFinished(Resolution), we explicitly set it to
  // inactive here to prevent processing any additional UI related service
  // events belonging to the interaction being stopped.
  assistant_interaction_model_.SetInteractionState(InteractionState::kInactive);
  assistant_interaction_model_.ClearPendingQuery();
  assistant_->StopActiveInteraction();
}

}  // namespace ash
