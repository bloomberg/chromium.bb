// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_MODEL_ASSISTANT_INTERACTION_MODEL_IMPL_H_
#define ASH_ASSISTANT_MODEL_ASSISTANT_INTERACTION_MODEL_IMPL_H_

#include <string>
#include <vector>

#include "base/macros.h"
#include "base/observer_list.h"
#include "ui/app_list/assistant_interaction_model.h"

namespace app_list {
class AssistantInteractionModelObserver;
}  // namespace app_list

namespace ash {

class AssistantInteractionModelImpl
    : public app_list::AssistantInteractionModel {
 public:
  AssistantInteractionModelImpl();
  ~AssistantInteractionModelImpl() override;

  // app_list::AssistantInteractionModel:
  void AddObserver(
      app_list::AssistantInteractionModelObserver* observer) override;
  void RemoveObserver(
      app_list::AssistantInteractionModelObserver* observer) override;
  void ClearInteraction() override;
  void SetCard(const std::string& html) override;
  void ClearCard() override;
  void SetRecognizedSpeech(
      const app_list::RecognizedSpeech& recognized_speech) override;
  void ClearRecognizedSpeech() override;
  void AddSuggestions(const std::vector<std::string>& suggestions) override;
  void ClearSuggestions() override;
  void AddText(const std::string& text) override;
  void ClearText() override;

 private:
  void NotifyCardChanged();
  void NotifyCardCleared();
  void NotifyRecognizedSpeechChanged();
  void NotifyRecognizedSpeechCleared();
  void NotifySuggestionsAdded(const std::vector<std::string>& suggestions);
  void NotifySuggestionsCleared();
  void NotifyTextAdded(const std::string& text);
  void NotifyTextCleared();

  std::string card_;
  app_list::RecognizedSpeech recognized_speech_;
  std::vector<std::string> suggestions_list_;
  std::vector<std::string> text_list_;

  base::ObserverList<app_list::AssistantInteractionModelObserver> observers_;

  DISALLOW_COPY_AND_ASSIGN(AssistantInteractionModelImpl);
};

}  // namespace ash

#endif  // ASH_ASSISTANT_MODEL_ASSISTANT_INTERACTION_MODEL_IMPL_H_
