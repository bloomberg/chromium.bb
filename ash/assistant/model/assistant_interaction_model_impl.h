// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_MODEL_ASSISTANT_INTERACTION_MODEL_IMPL_H_
#define ASH_ASSISTANT_MODEL_ASSISTANT_INTERACTION_MODEL_IMPL_H_

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/observer_list.h"
#include "ui/app_list/assistant_interaction_model.h"

namespace app_list {
class AssistantInteractionModelObserver;
class AssistantUiElement;
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
  void AddUiElement(
      std::unique_ptr<app_list::AssistantUiElement> ui_element) override;
  void ClearUiElements() override;
  void SetQuery(const app_list::Query& query) override;
  void ClearQuery() override;
  void AddSuggestions(const std::vector<std::string>& suggestions) override;
  void ClearSuggestions() override;

 private:
  void NotifyUiElementAdded(const app_list::AssistantUiElement* ui_element);
  void NotifyUiElementsCleared();
  void NotifyQueryChanged();
  void NotifyQueryCleared();
  void NotifySuggestionsAdded(const std::vector<std::string>& suggestions);
  void NotifySuggestionsCleared();

  app_list::Query query_;
  std::vector<std::string> suggestions_list_;
  std::vector<std::unique_ptr<app_list::AssistantUiElement>> ui_element_list_;

  base::ObserverList<app_list::AssistantInteractionModelObserver> observers_;

  DISALLOW_COPY_AND_ASSIGN(AssistantInteractionModelImpl);
};

}  // namespace ash

#endif  // ASH_ASSISTANT_MODEL_ASSISTANT_INTERACTION_MODEL_IMPL_H_
