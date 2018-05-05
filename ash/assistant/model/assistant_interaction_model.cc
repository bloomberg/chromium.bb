// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/model/assistant_interaction_model.h"

#include "ash/assistant/model/assistant_interaction_model_observer.h"
#include "ash/assistant/model/assistant_ui_element.h"

namespace ash {

AssistantInteractionModel::AssistantInteractionModel() {
  // TODO(dmblack): Default input modality should be read from user preferences.
  input_modality_ = InputModality::kVoice;
}

AssistantInteractionModel::~AssistantInteractionModel() = default;

void AssistantInteractionModel::AddObserver(
    AssistantInteractionModelObserver* observer) {
  observers_.AddObserver(observer);
}

void AssistantInteractionModel::RemoveObserver(
    AssistantInteractionModelObserver* observer) {
  observers_.RemoveObserver(observer);
}

void AssistantInteractionModel::ClearInteraction() {
  ClearUiElements();
  ClearQuery();
  ClearSuggestions();
}

void AssistantInteractionModel::SetInteractionState(
    InteractionState interaction_state) {
  if (interaction_state == interaction_state_)
    return;

  interaction_state_ = interaction_state;
  NotifyInteractionStateChanged();
}

void AssistantInteractionModel::SetInputModality(InputModality input_modality) {
  if (input_modality == input_modality_)
    return;

  input_modality_ = input_modality;
  NotifyInputModalityChanged();
}

void AssistantInteractionModel::SetMicState(MicState mic_state) {
  if (mic_state == mic_state_)
    return;

  mic_state_ = mic_state;
  NotifyMicStateChanged();
}

void AssistantInteractionModel::AddUiElement(
    std::unique_ptr<AssistantUiElement> ui_element) {
  AssistantUiElement* ptr = ui_element.get();
  ui_element_list_.push_back(std::move(ui_element));
  NotifyUiElementAdded(ptr);
}

void AssistantInteractionModel::ClearUiElements() {
  ui_element_list_.clear();
  NotifyUiElementsCleared();
}

void AssistantInteractionModel::SetQuery(const Query& query) {
  query_ = query;
  NotifyQueryChanged();
}

void AssistantInteractionModel::ClearQuery() {
  query_ = {};
  NotifyQueryCleared();
}

void AssistantInteractionModel::AddSuggestions(
    const std::vector<std::string>& suggestions) {
  suggestions_list_.insert(suggestions_list_.end(), suggestions.begin(),
                           suggestions.end());
  NotifySuggestionsAdded(suggestions);
}

void AssistantInteractionModel::ClearSuggestions() {
  suggestions_list_.clear();
  NotifySuggestionsCleared();
}

void AssistantInteractionModel::NotifyInteractionStateChanged() {
  for (AssistantInteractionModelObserver& observer : observers_)
    observer.OnInteractionStateChanged(interaction_state_);
}

void AssistantInteractionModel::NotifyInputModalityChanged() {
  for (AssistantInteractionModelObserver& observer : observers_)
    observer.OnInputModalityChanged(input_modality_);
}

void AssistantInteractionModel::NotifyMicStateChanged() {
  for (AssistantInteractionModelObserver& observer : observers_)
    observer.OnMicStateChanged(mic_state_);
}

void AssistantInteractionModel::NotifyUiElementAdded(
    const AssistantUiElement* ui_element) {
  for (AssistantInteractionModelObserver& observer : observers_)
    observer.OnUiElementAdded(ui_element);
}

void AssistantInteractionModel::NotifyUiElementsCleared() {
  for (AssistantInteractionModelObserver& observer : observers_)
    observer.OnUiElementsCleared();
}

void AssistantInteractionModel::NotifyQueryChanged() {
  for (AssistantInteractionModelObserver& observer : observers_)
    observer.OnQueryChanged(query_);
}

void AssistantInteractionModel::NotifyQueryCleared() {
  for (AssistantInteractionModelObserver& observer : observers_)
    observer.OnQueryCleared();
}

void AssistantInteractionModel::NotifySuggestionsAdded(
    const std::vector<std::string>& suggestions) {
  for (AssistantInteractionModelObserver& observer : observers_)
    observer.OnSuggestionsAdded(suggestions);
}

void AssistantInteractionModel::NotifySuggestionsCleared() {
  for (AssistantInteractionModelObserver& observer : observers_)
    observer.OnSuggestionsCleared();
}

}  // namespace ash
