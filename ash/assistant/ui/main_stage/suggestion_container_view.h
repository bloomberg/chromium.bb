// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_UI_MAIN_STAGE_SUGGESTION_CONTAINER_VIEW_H_
#define ASH_ASSISTANT_UI_MAIN_STAGE_SUGGESTION_CONTAINER_VIEW_H_

#include <map>

#include "ash/app_list/views/suggestion_chip_view.h"
#include "ash/assistant/model/assistant_interaction_model_observer.h"
#include "ash/assistant/ui/assistant_scroll_view.h"
#include "base/macros.h"
#include "chromeos/services/assistant/public/mojom/assistant.mojom.h"
#include "ui/views/controls/scroll_view.h"

namespace ash {

class AssistantController;

// SuggestionContainerView is the child of AssistantMainView concerned with
// laying out SuggestionChipViews in response to Assistant interaction model
// suggestion events.
class SuggestionContainerView : public AssistantScrollView,
                                public AssistantInteractionModelObserver,
                                public views::ButtonListener {
 public:
  using AssistantSuggestion = chromeos::assistant::mojom::AssistantSuggestion;

  explicit SuggestionContainerView(AssistantController* assistant_controller);
  ~SuggestionContainerView() override;

  // AssistantScrollView:
  gfx::Size CalculatePreferredSize() const override;
  int GetHeightForWidth(int width) const override;
  void OnContentsPreferredSizeChanged(views::View* content_view) override;

  // AssistantInteractionModelObserver:
  void OnResponseChanged(const AssistantResponse& response) override;
  void OnResponseCleared() override;

  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

 private:
  void InitLayout();

  // Invoked on suggestion chip icon downloaded event.
  void OnSuggestionChipIconDownloaded(int id, const gfx::ImageSkia& icon);

  AssistantController* const assistant_controller_;  // Owned by Shell.

  // Cache of suggestion chip views owned by the view hierarchy. The key for the
  // map is the unique identifier by which the Assistant interaction model
  // identifies the view's underlying suggestion.
  std::map<int, app_list::SuggestionChipView*> suggestion_chip_views_;

  // Weak pointer factory used for image downloading requests.
  base::WeakPtrFactory<SuggestionContainerView> download_request_weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(SuggestionContainerView);
};

}  // namespace ash

#endif  // ASH_ASSISTANT_UI_MAIN_STAGE_SUGGESTION_CONTAINER_VIEW_H_
