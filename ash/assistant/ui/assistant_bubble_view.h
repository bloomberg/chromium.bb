// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_UI_ASSISTANT_BUBBLE_VIEW_H_
#define ASH_ASSISTANT_UI_ASSISTANT_BUBBLE_VIEW_H_

#include "ash/assistant/model/assistant_interaction_model_observer.h"
#include "base/macros.h"
#include "ui/views/view.h"

namespace views {
class BoxLayout;
}  // namespace views

namespace ash {

class AssistantController;
class DialogPlate;
class SuggestionContainerView;
class UiElementContainerView;

namespace {
class InteractionContainer;
}  // namespace

class AssistantBubbleView : public views::View,
                            public AssistantInteractionModelObserver {
 public:
  explicit AssistantBubbleView(AssistantController* assistant_controller);
  ~AssistantBubbleView() override;

  // views::View:
  gfx::Size CalculatePreferredSize() const override;
  void ChildPreferredSizeChanged(views::View* child) override;
  void ChildVisibilityChanged(views::View* child) override;
  void OnBoundsChanged(const gfx::Rect& prev_bounds) override;
  void RequestFocus() override;

  // AssistantInteractionModelObserver:
  void OnInputModalityChanged(InputModality input_modality) override;
  void OnInteractionStateChanged(InteractionState interaction_state) override;
  void OnQueryChanged(const AssistantQuery& query) override;
  void OnQueryCleared() override;

 private:
  void InitLayout();

  AssistantController* const assistant_controller_;  // Owned by Shell.

  views::View* caption_bar_;                     // Owned by view hierarchy.
  InteractionContainer* interaction_container_;  // Owned by view hierarchy.
  UiElementContainerView* ui_element_container_;    // Owned by view hierarchy.
  SuggestionContainerView* suggestions_container_;  // Owned by view hierarchy.
  DialogPlate* dialog_plate_;                    // Owned by view hierarchy.

  views::BoxLayout* layout_manager_ = nullptr;  // Owned by view hierarchy.

  int min_height_dip_;

  DISALLOW_COPY_AND_ASSIGN(AssistantBubbleView);
};

}  // namespace ash

#endif  // ASH_ASSISTANT_UI_ASSISTANT_BUBBLE_VIEW_H_
