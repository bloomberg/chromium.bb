// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_ASSISTANT_INTERACTION_CONTROLLER_H_
#define ASH_ASSISTANT_ASSISTANT_INTERACTION_CONTROLLER_H_

#include <memory>
#include <string>
#include <vector>

#include "ash/assistant/model/assistant_interaction_model.h"
#include "ash/assistant/model/assistant_interaction_model_observer.h"
#include "ash/assistant/model/assistant_ui_model_observer.h"
#include "ash/assistant/ui/dialog_plate/dialog_plate.h"
#include "ash/highlighter/highlighter_controller.h"
#include "base/macros.h"
#include "chromeos/services/assistant/public/mojom/assistant.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace ash {

class AssistantController;
class AssistantInteractionModelObserver;
class AssistantUiController;

class AssistantInteractionController
    : public chromeos::assistant::mojom::AssistantEventSubscriber,
      public AssistantInteractionModelObserver,
      public AssistantUiModelObserver,
      public HighlighterController::Observer,
      public DialogPlateObserver {
 public:
  using AssistantSuggestion = chromeos::assistant::mojom::AssistantSuggestion;
  using AssistantSuggestionPtr =
      chromeos::assistant::mojom::AssistantSuggestionPtr;
  using AssistantInteractionResolution =
      chromeos::assistant::mojom::AssistantInteractionResolution;

  explicit AssistantInteractionController(
      AssistantController* assistant_controller);
  ~AssistantInteractionController() override;

  // Provides a pointer to the |assistant| owned by AssistantController.
  void SetAssistant(chromeos::assistant::mojom::Assistant* assistant);

  // Provides a pointer to the |assistant_ui_controller| owned by
  // AssistantController.
  void SetAssistantUiController(AssistantUiController* assistant_ui_controller);

  // Returns a reference to the underlying model.
  const AssistantInteractionModel* model() const {
    return &assistant_interaction_model_;
  }

  // Adds/removes the specified interaction model |observer|.
  void AddModelObserver(AssistantInteractionModelObserver* observer);
  void RemoveModelObserver(AssistantInteractionModelObserver* observer);

  // Invoked on suggestion chip pressed event.
  void OnSuggestionChipPressed(int id);

  // AssistantInteractionModelObserver:
  void OnInteractionStateChanged(InteractionState interaction_state) override;
  void OnInputModalityChanged(InputModality input_modality) override;
  void OnCommittedQueryChanged(const AssistantQuery& committed_query) override;

  // AssistantUiModelObserver:
  void OnUiVisibilityChanged(bool visible, AssistantSource source) override;

  // HighlighterController::Observer:
  void OnHighlighterEnabledChanged(HighlighterEnabledState state) override;

  // chromeos::assistant::mojom::AssistantEventSubscriber:
  void OnInteractionStarted() override;
  void OnInteractionFinished(
      AssistantInteractionResolution resolution) override;
  void OnHtmlResponse(const std::string& response) override;
  void OnSuggestionsResponse(
      std::vector<AssistantSuggestionPtr> response) override;
  void OnTextResponse(const std::string& response) override;
  void OnOpenUrlResponse(const GURL& url) override;
  void OnSpeechRecognitionStarted() override;
  void OnSpeechRecognitionIntermediateResult(
      const std::string& high_confidence_text,
      const std::string& low_confidence_text) override;
  void OnSpeechRecognitionEndOfUtterance() override;
  void OnSpeechRecognitionFinalResult(const std::string& final_result) override;
  void OnSpeechLevelUpdated(float speech_level) override;

  // DialogPlateObserver:
  void OnDialogPlateButtonPressed(DialogPlateButtonId id) override;
  void OnDialogPlateContentsCommitted(const std::string& text) override;

 private:
  void StartTextInteraction(const std::string text);
  void StartVoiceInteraction();
  void StopActiveInteraction();

  void OpenUrl(const GURL& url);

  AssistantController* const assistant_controller_;  // Owned by Shell.

  // Owned by AssistantController.
  chromeos::assistant::mojom::Assistant* assistant_ = nullptr;

  // Owned by AssisantController.
  AssistantUiController* assistant_ui_controller_ = nullptr;

  mojo::Binding<chromeos::assistant::mojom::AssistantEventSubscriber>
      assistant_event_subscriber_binding_;

  AssistantInteractionModel assistant_interaction_model_;

  DISALLOW_COPY_AND_ASSIGN(AssistantInteractionController);
};

}  // namespace ash

#endif  // ASH_ASSISTANT_ASSISTANT_INTERACTION_CONTROLLER_H_
