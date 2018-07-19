// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_UI_MAIN_STAGE_ASSISTANT_HEADER_VIEW_H_
#define ASH_ASSISTANT_UI_MAIN_STAGE_ASSISTANT_HEADER_VIEW_H_

#include "ash/assistant/model/assistant_interaction_model_observer.h"
#include "ash/assistant/model/assistant_ui_model_observer.h"
#include "base/macros.h"
#include "ui/views/view.h"

namespace views {
class BoxLayout;
class Label;
}  // namespace views

namespace ash {

class AssistantController;
class AssistantProgressIndicator;

// AssistantHeaderView is the child of UiElementContainerView which provides
// the Assistant icon. On first launch, it also displays a greeting to the user.
class AssistantHeaderView : public views::View,
                            public AssistantInteractionModelObserver,
                            public AssistantUiModelObserver {
 public:
  explicit AssistantHeaderView(AssistantController* assistant_controller);
  ~AssistantHeaderView() override;

  // views::View:
  gfx::Size CalculatePreferredSize() const override;
  int GetHeightForWidth(int width) const override;
  void ChildVisibilityChanged(views::View* child) override;

  // AssistantInteractionModelObserver:
  void OnCommittedQueryChanged(const AssistantQuery& committed_query) override;
  void OnUiElementAdded(const AssistantUiElement* ui_element) override;

  // AssistantUiModelObserver:
  void OnUiVisibilityChanged(bool visible, AssistantSource source) override;

 private:
  void InitLayout();

  AssistantController* const assistant_controller_;  // Owned by Shell.

  views::BoxLayout* layout_manager_;  // Owned by view hierarchy.
  views::Label* greeting_label_;      // Owned by view hierarchy.
  AssistantProgressIndicator* progress_indicator_;  // Owned by view hierarchy.

  DISALLOW_COPY_AND_ASSIGN(AssistantHeaderView);
};

}  // namespace ash

#endif  // ASH_ASSISTANT_UI_MAIN_STAGE_ASSISTANT_HEADER_VIEW_H_
