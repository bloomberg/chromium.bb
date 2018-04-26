// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_ASH_ASSISTANT_CONTROLLER_H_
#define ASH_ASSISTANT_ASH_ASSISTANT_CONTROLLER_H_

#include <memory>
#include <string>
#include <vector>

#include "ash/assistant/model/assistant_interaction_model_impl.h"
#include "ash/public/interfaces/ash_assistant_controller.mojom.h"
#include "ash/public/interfaces/assistant_card_renderer.mojom.h"
#include "base/macros.h"
#include "base/timer/timer.h"
#include "chromeos/services/assistant/public/mojom/assistant.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "ui/app_list/assistant_controller.h"

namespace app_list {
class AssistantInteractionModelObserver;
}  // namespace app_list

namespace base {
class UnguessableToken;
}  // namespace base

namespace ash {

class AssistantBubble;

class AshAssistantController
    : public app_list::AssistantController,
      public mojom::AshAssistantController,
      public chromeos::assistant::mojom::AssistantEventSubscriber {
 public:
  AshAssistantController();
  ~AshAssistantController() override;

  void BindRequest(mojom::AshAssistantControllerRequest request);

  // app_list::AssistantController:
  void AddInteractionModelObserver(
      app_list::AssistantInteractionModelObserver* observer) override;
  void RemoveInteractionModelObserver(
      app_list::AssistantInteractionModelObserver* observer) override;
  void RenderCard(
      const base::UnguessableToken& id_token,
      mojom::AssistantCardParamsPtr params,
      mojom::AssistantCardRenderer::RenderCallback callback) override;
  void ReleaseCard(const base::UnguessableToken& id_token) override;
  void ReleaseCards(
      const std::vector<base::UnguessableToken>& id_tokens) override;
  void OnSuggestionChipPressed(const std::string& text) override;

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
  AssistantInteractionModelImpl assistant_interaction_model_;

  chromeos::assistant::mojom::AssistantPtr assistant_;
  mojom::AssistantCardRendererPtr assistant_card_renderer_;

  std::unique_ptr<AssistantBubble> assistant_bubble_;
  base::OneShotTimer assistant_bubble_timer_;

  DISALLOW_COPY_AND_ASSIGN(AshAssistantController);
};

}  // namespace ash

#endif  // ASH_ASSISTANT_ASH_ASSISTANT_CONTROLLER_H_
