// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_ASH_ASSISTANT_CONTROLLER_H_
#define ASH_ASSISTANT_ASH_ASSISTANT_CONTROLLER_H_

#include "ash/assistant/model/assistant_interaction_model_impl.h"
#include "ash/public/interfaces/ash_assistant_controller.mojom.h"
#include "ash/shell_observer.h"
#include "base/macros.h"
#include "chromeos/services/assistant/public/mojom/assistant.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "ui/app_list/assistant_controller.h"

namespace app_list {
class AssistantInteractionModelObserver;
}  // namespace app_list

namespace aura {
class Window;
}  // namespace aura

namespace ash {

class AshAssistantController
    : public app_list::AssistantController,
      public mojom::AshAssistantController,
      public chromeos::assistant::mojom::AssistantEventSubscriber,
      public ShellObserver {
 public:
  AshAssistantController();
  ~AshAssistantController() override;

  void BindRequest(mojom::AshAssistantControllerRequest request);

  // app_list::AssistantController:
  void AddInteractionModelObserver(
      app_list::AssistantInteractionModelObserver* observer) override;
  void RemoveInteractionModelObserver(
      app_list::AssistantInteractionModelObserver* observer) override;
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

  // ShellObserver:
  void OnAppListVisibilityChanged(bool shown,
                                  aura::Window* root_window) override;

 private:
  mojo::Binding<mojom::AshAssistantController> assistant_controller_binding_;
  mojo::Binding<chromeos::assistant::mojom::AssistantEventSubscriber>
      assistant_event_subscriber_binding_;
  AssistantInteractionModelImpl assistant_interaction_model_;

  // Interface to Chrome OS Assistant service.
  chromeos::assistant::mojom::AssistantPtr assistant_;

  // TODO(b/77637813): Remove when pulling Assistant out of launcher.
  bool is_app_list_shown_ = false;

  DISALLOW_COPY_AND_ASSIGN(AshAssistantController);
};

}  // namespace ash

#endif  // ASH_ASSISTANT_ASH_ASSISTANT_CONTROLLER_H_
