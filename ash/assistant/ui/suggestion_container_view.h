// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_UI_SUGGESTION_CONTAINER_VIEW_H_
#define ASH_ASSISTANT_UI_SUGGESTION_CONTAINER_VIEW_H_

#include "ash/assistant/model/assistant_interaction_model_observer.h"
#include "base/macros.h"
#include "ui/app_list/views/suggestion_chip_view.h"
#include "ui/views/view.h"

namespace ash {

class AssistantController;

class SuggestionContainerView : public views::View,
                                public AssistantInteractionModelObserver,
                                public app_list::SuggestionChipListener {
 public:
  using AssistantSuggestion = chromeos::assistant::mojom::AssistantSuggestion;

  explicit SuggestionContainerView(AssistantController* assistant_controller);
  ~SuggestionContainerView() override;

  // views::View:
  gfx::Size CalculatePreferredSize() const override;
  int GetHeightForWidth(int width) const override;
  void Layout() override;

  // AssistantInteractionModelObserver:
  void OnSuggestionsAdded(
      const std::map<int, AssistantSuggestion*>& suggestions) override;
  void OnSuggestionsCleared() override;

  // app_list::SuggestionChipListener:
  void OnSuggestionChipPressed(
      app_list::SuggestionChipView* suggestion_chip_view) override;

 private:
  AssistantController* const assistant_controller_;  // Owned by Shell.

  DISALLOW_COPY_AND_ASSIGN(SuggestionContainerView);
};

}  // namespace ash

#endif  // ASH_ASSISTANT_UI_SUGGESTION_CONTAINER_VIEW_H_
