// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_ASH_ASSISTANT_CONTROLLER_H_
#define ASH_ASSISTANT_ASH_ASSISTANT_CONTROLLER_H_

#include <memory>
#include <string>
#include <vector>

#include "ash/assistant/model/assistant_interaction_model.h"
#include "ash/assistant/model/assistant_interaction_model_observer.h"
#include "ash/public/interfaces/ash_assistant_controller.mojom.h"
#include "ash/public/interfaces/assistant_card_renderer.mojom.h"
#include "base/macros.h"
#include "base/timer/timer.h"
#include "chromeos/services/assistant/public/mojom/assistant.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace base {
class UnguessableToken;
}  // namespace base

namespace ash {

class AssistantBubble;
class AssistantInteractionModelObserver;

class AshAssistantController
    : public mojom::AshAssistantController,
      public chromeos::assistant::mojom::AssistantEventSubscriber,
      public AssistantInteractionModelObserver {
 public:
  AshAssistantController();
  ~AshAssistantController() override;

  void BindRequest(mojom::AshAssistantControllerRequest request);

  // Returns a reference to the underlying interaction model.
  const AssistantInteractionModel* interaction_model() const {
    return &assistant_interaction_model_;
  }

  // Registers the specified |observer| with the interaction model observer
  // pool.
  void AddInteractionModelObserver(AssistantInteractionModelObserver* observer);

  // Unregisters the specified |observer| from the interaction model observer
  // pool.
  void RemoveInteractionModelObserver(
      AssistantInteractionModelObserver* observer);

  // Renders a card, uniquely identified by |id_token|, according to the
  // specified |params|. When the card is ready for embedding, the supplied
  // |callback| is run with a token for embedding.
  void RenderCard(const base::UnguessableToken& id_token,
                  mojom::AssistantCardParamsPtr params,
                  mojom::AssistantCardRenderer::RenderCallback callback);

  // Releases resources for the card uniquely identified by |id_token|.
  void ReleaseCard(const base::UnguessableToken& id_token);

  // Releases resources for any card uniquely identified in |id_token_list|.
  void ReleaseCards(const std::vector<base::UnguessableToken>& id_tokens);

  // Invoke to modify the Assistant interaction state.
  void StartInteraction();
  void StopInteraction();
  void ToggleInteraction();

  // Invoked on dialog plate contents changed event.
  void OnDialogPlateContentsChanged(const std::string& text);

  // Invoked on dialog plate contents committed event.
  void OnDialogPlateContentsCommitted(const std::string& text);

  // Invoked on suggestion chip pressed event.
  void OnSuggestionChipPressed(const std::string& text);

  // AssistantInteractionModelObserver:
  void OnInteractionStateChanged(InteractionState interaction_state) override;

  // chromeos::assistant::mojom::AssistantEventSubscriber:
  void OnInteractionStarted() override;
  void OnInteractionFinished(
      chromeos::assistant::mojom::AssistantInteractionResolution resolution)
      override;
  void OnHtmlResponse(const std::string& response) override;
  void OnSuggestionsResponse(const std::vector<std::string>& response) override;
  void OnTextResponse(const std::string& response) override;
  void OnOpenUrlResponse(const GURL& url) override;
  void OnSpeechRecognitionStarted() override;
  void OnSpeechRecognitionIntermediateResult(
      const std::string& high_confidence_text,
      const std::string& low_confidence_text) override;
  void OnSpeechRecognitionEndOfUtterance() override;
  void OnSpeechRecognitionFinalResult(const std::string& final_result) override;
  void OnSpeechLevelUpdated(float speech_level) override;

  // mojom::AshAssistantController:
  void SetAssistant(
      chromeos::assistant::mojom::AssistantPtr assistant) override;
  void SetAssistantCardRenderer(
      mojom::AssistantCardRendererPtr assistant_card_renderer) override;

 private:
  void OnInteractionDismissed();

  mojo::Binding<mojom::AshAssistantController> assistant_controller_binding_;
  mojo::Binding<chromeos::assistant::mojom::AssistantEventSubscriber>
      assistant_event_subscriber_binding_;
  AssistantInteractionModel assistant_interaction_model_;

  chromeos::assistant::mojom::AssistantPtr assistant_;
  mojom::AssistantCardRendererPtr assistant_card_renderer_;

  std::unique_ptr<AssistantBubble> assistant_bubble_;
  base::OneShotTimer assistant_bubble_timer_;

  DISALLOW_COPY_AND_ASSIGN(AshAssistantController);
};

}  // namespace ash

#endif  // ASH_ASSISTANT_ASH_ASSISTANT_CONTROLLER_H_
