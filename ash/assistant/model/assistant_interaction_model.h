// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_MODEL_ASSISTANT_INTERACTION_MODEL_H_
#define ASH_ASSISTANT_MODEL_ASSISTANT_INTERACTION_MODEL_H_

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/observer_list.h"

namespace ash {

class AssistantInteractionModelObserver;
class AssistantUiElement;

// Enumeration of interaction input modalities.
enum class InputModality {
  kKeyboard,
  kStylus,
  kVoice,
};

// TODO(dmblack): This is an oversimplification. We will eventually want to
// distinctly represent listening/thinking/etc. states explicitly so they can
// be adequately represented in the UI.
// Enumeration of interaction states.
enum class InteractionState {
  kActive,
  kInactive,
};

// Models the state of the query. For a text query, only the high confidence
// text portion will be populated. At start of a voice query, both the high and
// low confidence text portions will be empty. As speech recognition continues,
// the low confidence portion will become non-empty. As speech recognition
// improves, both the high and low confidence portions of the query will be
// non-empty. When speech is fully recognized, only the high confidence portion
// will be populated.
struct Query {
  // High confidence portion of the query.
  std::string high_confidence_text;
  // Low confidence portion of the query.
  std::string low_confidence_text;

  // Returns true if query is empty, false otherwise.
  bool empty() const {
    return high_confidence_text.empty() && low_confidence_text.empty();
  }
};

// Models the Assistant interaction. This includes query state, state of speech
// recognition, as well as renderable AssistantUiElements and suggestions.
class AssistantInteractionModel {
 public:
  AssistantInteractionModel();
  ~AssistantInteractionModel();

  // Adds/removes the specified interaction model |observer|.
  void AddObserver(AssistantInteractionModelObserver* observer);
  void RemoveObserver(AssistantInteractionModelObserver* observer);

  // Sets the interaction state.
  void SetInteractionState(InteractionState interaction_state);

  // Returns the interaction state.
  InteractionState interaction_state() const { return interaction_state_; }

  // Resets the interaction to its initial state.
  void ClearInteraction();

  // Updates the input modality for the interaction.
  void SetInputModality(InputModality input_modality);

  // Returns the input modality for the interaction.
  InputModality input_modality() const { return input_modality_; }

  // Adds the specified |ui_element| that should be rendered for the
  // interaction.
  void AddUiElement(std::unique_ptr<AssistantUiElement> ui_element);

  // Clears all UI elements for the interaction.
  void ClearUiElements();

  // Updates the query for the interaction.
  void SetQuery(const Query& query);

  // Returns the query for the interaction.
  const Query& query() const { return query_; }

  // Clears the query for the interaction.
  void ClearQuery();

  // Adds the specified |suggestions| that should be rendered for the
  // interaction.
  void AddSuggestions(const std::vector<std::string>& suggestions);

  // Clears all suggestions for the interaction.
  void ClearSuggestions();

 private:
  void NotifyInteractionStateChanged();
  void NotifyInputModalityChanged();
  void NotifyUiElementAdded(const AssistantUiElement* ui_element);
  void NotifyUiElementsCleared();
  void NotifyQueryChanged();
  void NotifyQueryCleared();
  void NotifySuggestionsAdded(const std::vector<std::string>& suggestions);
  void NotifySuggestionsCleared();

  InteractionState interaction_state_ = InteractionState::kInactive;
  InputModality input_modality_;
  Query query_;
  std::vector<std::string> suggestions_list_;
  std::vector<std::unique_ptr<AssistantUiElement>> ui_element_list_;

  base::ObserverList<AssistantInteractionModelObserver> observers_;

  DISALLOW_COPY_AND_ASSIGN(AssistantInteractionModel);
};

}  // namespace ash

#endif  // ASH_ASSISTANT_MODEL_ASSISTANT_INTERACTION_MODEL_H_
