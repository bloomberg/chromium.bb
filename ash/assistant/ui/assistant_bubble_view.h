// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_UI_ASSISTANT_BUBBLE_VIEW_H_
#define ASH_ASSISTANT_UI_ASSISTANT_BUBBLE_VIEW_H_

#include "base/macros.h"
#include "ui/views/bubble/bubble_dialog_delegate.h"

namespace ash {

class AssistantController;
class AssistantMainView;

class AssistantBubbleView : public views::BubbleDialogDelegateView {
 public:
  explicit AssistantBubbleView(AssistantController* assistant_controller);
  ~AssistantBubbleView() override;

  // views::BubbleDialogDelegateView:
  void ChildPreferredSizeChanged(views::View* child) override;
  int GetDialogButtons() const override;
  void OnBeforeBubbleWidgetInit(views::Widget::InitParams* params,
                                views::Widget* widget) const override;
  void Init() override;
  void RequestFocus() override;

 private:
  void SetAnchor();

  AssistantController* const assistant_controller_;  // Owned by Shell.

  AssistantMainView* assistant_main_view_;  // Owned by view hierarchy.

  DISALLOW_COPY_AND_ASSIGN(AssistantBubbleView);
};

}  // namespace ash

#endif  // ASH_ASSISTANT_UI_ASSISTANT_BUBBLE_VIEW_H_
