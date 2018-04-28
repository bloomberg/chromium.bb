// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/model/assistant_interaction_model.h"

#include "ash/assistant/model/assistant_interaction_model_observer.h"
#include "ash/assistant/model/assistant_ui_element.h"

namespace ash {

AssistantInteractionModel::AssistantInteractionModel() = default;

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
