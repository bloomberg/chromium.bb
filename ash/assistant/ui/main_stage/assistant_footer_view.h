// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_UI_MAIN_STAGE_ASSISTANT_FOOTER_VIEW_H_
#define ASH_ASSISTANT_UI_MAIN_STAGE_ASSISTANT_FOOTER_VIEW_H_

#include "base/macros.h"
#include "ui/views/view.h"

namespace ash {

class AssistantController;
class SuggestionContainerView;

class AssistantFooterView : public views::View {
 public:
  explicit AssistantFooterView(AssistantController* assistant_controller);
  ~AssistantFooterView() override;

  // views::View:
  gfx::Size CalculatePreferredSize() const override;
  int GetHeightForWidth(int width) const override;
  void ChildPreferredSizeChanged(views::View* child) override;
  void ChildVisibilityChanged(views::View* child) override;

 private:
  void InitLayout();

  AssistantController* const assistant_controller_;  // Owned by Shell.

  SuggestionContainerView* suggestion_container_;  // Owned by view hierarchy.

  DISALLOW_COPY_AND_ASSIGN(AssistantFooterView);
};

}  // namespace ash

#endif  // ASH_ASSISTANT_UI_MAIN_STAGE_ASSISTANT_FOOTER_VIEW_H_
