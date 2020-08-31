// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_ASSISTANT_SUGGESTIONS_CONTROLLER_IMPL_H_
#define ASH_ASSISTANT_ASSISTANT_SUGGESTIONS_CONTROLLER_IMPL_H_

#include <memory>

#include "ash/assistant/assistant_proactive_suggestions_controller.h"
#include "ash/assistant/model/assistant_suggestions_model.h"
#include "ash/assistant/model/assistant_ui_model_observer.h"
#include "ash/public/cpp/assistant/assistant_state.h"
#include "ash/public/cpp/assistant/controller/assistant_controller.h"
#include "ash/public/cpp/assistant/controller/assistant_controller_observer.h"
#include "ash/public/cpp/assistant/controller/assistant_suggestions_controller.h"
#include "base/macros.h"
#include "base/scoped_observer.h"

namespace ash {

class AssistantControllerImpl;
class AssistantSuggestionsModelObserver;
class ProactiveSuggestions;

// The implementation of the Assistant controller in charge of suggestions.
class AssistantSuggestionsControllerImpl
    : public AssistantSuggestionsController,
      public AssistantControllerObserver,
      public AssistantUiModelObserver,
      public AssistantStateObserver {
 public:
  explicit AssistantSuggestionsControllerImpl(
      AssistantControllerImpl* assistant_controller);
  ~AssistantSuggestionsControllerImpl() override;

  // AssistantSuggestionsController:
  const AssistantSuggestionsModel* GetModel() const override;
  void AddModelObserver(AssistantSuggestionsModelObserver*) override;
  void RemoveModelObserver(AssistantSuggestionsModelObserver*) override;

  // AssistantControllerObserver:
  void OnAssistantControllerConstructed() override;
  void OnAssistantControllerDestroying() override;

  // AssistantUiModelObserver:
  void OnUiVisibilityChanged(
      AssistantVisibility new_visibility,
      AssistantVisibility old_visibility,
      base::Optional<AssistantEntryPoint> entry_point,
      base::Optional<AssistantExitPoint> exit_point) override;

  // Invoked when the active set of |proactive_suggestions| has changed. Note
  // that this method should only be called by the sub-controller for the
  // proactive suggestions feature to update model state.
  void OnProactiveSuggestionsChanged(
      scoped_refptr<const ProactiveSuggestions> proactive_suggestions);

 private:
  // AssistantStateObserver:
  void OnAssistantContextEnabled(bool enabled) override;

  void UpdateConversationStarters();
  void FetchConversationStarters();
  void ProvideConversationStarters();

  AssistantControllerImpl* const assistant_controller_;  // Owned by Shell.

  // A sub-controller for the proactive suggestions feature. Note that this will
  // only exist if the proactive suggestions feature is enabled.
  std::unique_ptr<AssistantProactiveSuggestionsController>
      proactive_suggestions_controller_;

  AssistantSuggestionsModel model_;

  ScopedObserver<AssistantController, AssistantControllerObserver>
      assistant_controller_observer_{this};

  // A WeakPtrFactory used to manage lifecycle of conversation starter requests
  // to the server (via the dedicated ConversationStartersClient).
  base::WeakPtrFactory<AssistantSuggestionsControllerImpl>
      conversation_starters_weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(AssistantSuggestionsControllerImpl);
};

}  // namespace ash

#endif  // ASH_ASSISTANT_ASSISTANT_SUGGESTIONS_CONTROLLER_IMPL_H_
