// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/model/assistant_interaction_model_impl.h"

#include "ui/app_list/assistant_interaction_model_observer.h"
#include "ui/app_list/assistant_ui_element.h"

namespace ash {

AssistantInteractionModelImpl::AssistantInteractionModelImpl() = default;

AssistantInteractionModelImpl::~AssistantInteractionModelImpl() = default;

void AssistantInteractionModelImpl::AddObserver(
    app_list::AssistantInteractionModelObserver* observer) {
  observers_.AddObserver(observer);
}

void AssistantInteractionModelImpl::RemoveObserver(
    app_list::AssistantInteractionModelObserver* observer) {
  observers_.RemoveObserver(observer);
}

void AssistantInteractionModelImpl::ClearInteraction() {
  ClearUiElements();
  ClearQuery();
  ClearSuggestions();
}

void AssistantInteractionModelImpl::AddUiElement(
    std::unique_ptr<app_list::AssistantUiElement> ui_element) {
  app_list::AssistantUiElement* ptr = ui_element.get();
  ui_element_list_.push_back(std::move(ui_element));
  NotifyUiElementAdded(ptr);
}

void AssistantInteractionModelImpl::ClearUiElements() {
  ui_element_list_.clear();
  NotifyUiElementsCleared();
}

void AssistantInteractionModelImpl::SetQuery(const app_list::Query& query) {
  query_ = query;
  NotifyQueryChanged();
}

void AssistantInteractionModelImpl::ClearQuery() {
  query_ = {};
  NotifyQueryCleared();
}

void AssistantInteractionModelImpl::AddSuggestions(
    const std::vector<std::string>& suggestions) {
  suggestions_list_.insert(suggestions_list_.end(), suggestions.begin(),
                           suggestions.end());
  NotifySuggestionsAdded(suggestions);
}

void AssistantInteractionModelImpl::ClearSuggestions() {
  suggestions_list_.clear();
  NotifySuggestionsCleared();
}

void AssistantInteractionModelImpl::NotifyUiElementAdded(
    const app_list::AssistantUiElement* ui_element) {
  for (app_list::AssistantInteractionModelObserver& observer : observers_)
    observer.OnUiElementAdded(ui_element);
}

void AssistantInteractionModelImpl::NotifyUiElementsCleared() {
  for (app_list::AssistantInteractionModelObserver& observer : observers_)
    observer.OnUiElementsCleared();
}

void AssistantInteractionModelImpl::NotifyQueryChanged() {
  for (app_list::AssistantInteractionModelObserver& observer : observers_)
    observer.OnQueryChanged(query_);
}

void AssistantInteractionModelImpl::NotifyQueryCleared() {
  for (app_list::AssistantInteractionModelObserver& observer : observers_)
    observer.OnQueryCleared();
}

void AssistantInteractionModelImpl::NotifySuggestionsAdded(
    const std::vector<std::string>& suggestions) {
  for (app_list::AssistantInteractionModelObserver& observer : observers_)
    observer.OnSuggestionsAdded(suggestions);
}

void AssistantInteractionModelImpl::NotifySuggestionsCleared() {
  for (app_list::AssistantInteractionModelObserver& observer : observers_)
    observer.OnSuggestionsCleared();
}

}  // namespace ash
