// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/model/assistant_cache_model.h"

#include "ash/assistant/model/assistant_cache_model_observer.h"

namespace ash {

AssistantCacheModel::AssistantCacheModel() = default;

AssistantCacheModel::~AssistantCacheModel() = default;

void AssistantCacheModel::AddObserver(AssistantCacheModelObserver* observer) {
  observers_.AddObserver(observer);
}

void AssistantCacheModel::RemoveObserver(
    AssistantCacheModelObserver* observer) {
  observers_.RemoveObserver(observer);
}

void AssistantCacheModel::SetConversationStarters(
    std::vector<AssistantSuggestionPtr> conversation_starters) {
  conversation_starters_.clear();
  conversation_starters_.swap(conversation_starters);

  NotifyConversationStartersChanged();
}

const chromeos::assistant::mojom::AssistantSuggestion*
AssistantCacheModel::GetConversationStarterById(int id) const {
  // We consider the index of a conversation starter within our backing vector
  // to be its unique id.
  DCHECK_GE(id, 0);
  DCHECK_LT(id, static_cast<int>(conversation_starters_.size()));
  return conversation_starters_.at(id).get();
}

std::map<int, const chromeos::assistant::mojom::AssistantSuggestion*>
AssistantCacheModel::GetConversationStarters() const {
  std::map<int, const AssistantSuggestion*> conversation_starters;

  // We use index within our backing vector to represent unique id.
  int id = 0;
  for (const AssistantSuggestionPtr& starter : conversation_starters_)
    conversation_starters[id++] = starter.get();

  return conversation_starters;
}

void AssistantCacheModel::NotifyConversationStartersChanged() {
  const std::map<int, const AssistantSuggestion*> conversation_starters =
      GetConversationStarters();

  for (AssistantCacheModelObserver& observer : observers_)
    observer.OnConversationStartersChanged(conversation_starters);
}

}  // namespace ash
