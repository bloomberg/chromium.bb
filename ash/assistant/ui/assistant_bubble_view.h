// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_UI_ASSISTANT_BUBBLE_VIEW_H_
#define ASH_ASSISTANT_UI_ASSISTANT_BUBBLE_VIEW_H_

#include <deque>
#include <memory>
#include <string>
#include <vector>

#include "ash/assistant/model/assistant_interaction_model_observer.h"
#include "base/macros.h"
#include "ui/app_list/views/suggestion_chip_view.h"
#include "ui/views/view.h"

namespace ash {

class AshAssistantController;
class AssistantCardElement;
class AssistantTextElement;
class AssistantUiElement;

namespace {
class InteractionContainer;
class SuggestionsContainer;
class UiElementContainer;
}  // namespace

class AssistantBubbleView : public views::View,
                            public AssistantInteractionModelObserver,
                            public app_list::SuggestionChipListener {
 public:
  explicit AssistantBubbleView(AshAssistantController* assistant_controller);
  ~AssistantBubbleView() override;

  // views::View:
  gfx::Size CalculatePreferredSize() const override;
  void ChildPreferredSizeChanged(views::View* child) override;
  void ChildVisibilityChanged(views::View* child) override;

  // AssistantInteractionModelObserver:
  void OnInputModalityChanged(InputModality input_modality) override;
  void OnUiElementAdded(const AssistantUiElement* ui_element) override;
  void OnUiElementsCleared() override;
  void OnQueryChanged(const Query& query) override;
  void OnQueryCleared() override;
  void OnSuggestionsAdded(const std::vector<std::string>& suggestions) override;
  void OnSuggestionsCleared() override;

  // app_list::SuggestionChipListener:
  void OnSuggestionChipPressed(
      app_list::SuggestionChipView* suggestion_chip_view) override;

 private:
  void InitLayout();

  // Assistant cards are rendered asynchronously before being added to the view
  // hierarchy. For this reason, it is necessary to pend any UI elements that
  // arrive between the time a render request is sent and the time at which the
  // view is finally embedded. Failure to do so could result in a mismatch
  // between the ordering of UI elements received and their corresponding views.
  void SetProcessingUiElement(bool is_processing);
  void ProcessPendingUiElements();

  void OnCardAdded(const AssistantCardElement* card_element);
  void OnCardReady(const base::UnguessableToken& embed_token);
  void OnReleaseCards();
  void OnTextAdded(const AssistantTextElement* text_element);

  AshAssistantController* assistant_controller_;  // Owned by Shell.
  InteractionContainer* interaction_container_;  // Owned by view hierarchy.
  UiElementContainer* ui_element_container_;     // Owned by view hierarchy.
  SuggestionsContainer* suggestions_container_;  // Owned by view hierarchy.

  // Uniquely identifies cards owned by AssistantCardRenderer.
  std::vector<base::UnguessableToken> id_token_list_;

  // Owned by AssistantInteractionModel.
  std::deque<const AssistantUiElement*> pending_ui_element_list_;

  // Whether a UI element is currently being processed. If true, new UI elements
  // are added to |pending_ui_element_list_| and processed later.
  bool is_processing_ui_element_ = false;

  // Weak pointer factory used for card rendering requests.
  base::WeakPtrFactory<AssistantBubbleView> render_request_weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(AssistantBubbleView);
};

}  // namespace ash

#endif  // ASH_ASSISTANT_UI_ASSISTANT_BUBBLE_VIEW_H_
