// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/model/assistant_interaction_model_impl.h"

#include "ui/app_list/assistant_interaction_model_observer.h"

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
  ClearCard();
  ClearRecognizedSpeech();
  ClearSuggestions();
  ClearText();
}

void AssistantInteractionModelImpl::SetCard(const std::string& html) {
  card_ = html;
  NotifyCardChanged();
}

void AssistantInteractionModelImpl::ClearCard() {
  card_.clear();
  NotifyCardCleared();
}

void AssistantInteractionModelImpl::SetRecognizedSpeech(
    const app_list::RecognizedSpeech& recognized_speech) {
  recognized_speech_ = recognized_speech;
  NotifyRecognizedSpeechChanged();
}

void AssistantInteractionModelImpl::ClearRecognizedSpeech() {
  recognized_speech_ = {};
  NotifyRecognizedSpeechCleared();
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

void AssistantInteractionModelImpl::AddText(const std::string& text) {
  text_list_.push_back(text);
  NotifyTextAdded(text);
}

void AssistantInteractionModelImpl::ClearText() {
  text_list_.clear();
  NotifyTextCleared();
}

void AssistantInteractionModelImpl::NotifyCardChanged() {
  for (app_list::AssistantInteractionModelObserver& observer : observers_)
    observer.OnCardChanged(card_);
}

void AssistantInteractionModelImpl::NotifyCardCleared() {
  for (app_list::AssistantInteractionModelObserver& observer : observers_)
    observer.OnCardCleared();
}

void AssistantInteractionModelImpl::NotifyRecognizedSpeechChanged() {
  for (app_list::AssistantInteractionModelObserver& observer : observers_)
    observer.OnRecognizedSpeechChanged(recognized_speech_);
}

void AssistantInteractionModelImpl::NotifyRecognizedSpeechCleared() {
  for (app_list::AssistantInteractionModelObserver& observer : observers_)
    observer.OnRecognizedSpeechCleared();
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

void AssistantInteractionModelImpl::NotifyTextAdded(const std::string& text) {
  for (app_list::AssistantInteractionModelObserver& observer : observers_)
    observer.OnTextAdded(text);
}

void AssistantInteractionModelImpl::NotifyTextCleared() {
  for (app_list::AssistantInteractionModelObserver& observer : observers_)
    observer.OnTextCleared();
}

}  // namespace ash
