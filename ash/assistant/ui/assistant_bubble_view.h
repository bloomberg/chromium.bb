// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_UI_ASSISTANT_BUBBLE_VIEW_H_
#define ASH_ASSISTANT_UI_ASSISTANT_BUBBLE_VIEW_H_

#include <memory>

#include "ash/assistant/model/assistant_bubble_model_observer.h"
#include "base/macros.h"
#include "ui/views/bubble/bubble_dialog_delegate.h"

namespace ash {

class AssistantController;
class AssistantMainView;
class AssistantMiniView;
class AssistantWebView;

class AssistantBubbleView : public views::BubbleDialogDelegateView,
                            public AssistantBubbleModelObserver {
 public:
  explicit AssistantBubbleView(AssistantController* assistant_controller);
  ~AssistantBubbleView() override;

  // views::BubbleDialogDelegateView:
  int GetDialogButtons() const override;
  void ChildPreferredSizeChanged(views::View* child) override;
  void PreferredSizeChanged() override;
  void OnBeforeBubbleWidgetInit(views::Widget::InitParams* params,
                                views::Widget* widget) const override;
  void Init() override;
  void RequestFocus() override;

  // AssistantBubbleModelObserver:
  void OnUiModeChanged(AssistantUiMode ui_mode) override;

 private:
  void SetAnchor();

  AssistantController* const assistant_controller_;  // Owned by Shell.

  std::unique_ptr<AssistantMainView> assistant_main_view_;
  std::unique_ptr<AssistantMiniView> assistant_mini_view_;
  std::unique_ptr<AssistantWebView> assistant_web_view_;

  DISALLOW_COPY_AND_ASSIGN(AssistantBubbleView);
};

}  // namespace ash

#endif  // ASH_ASSISTANT_UI_ASSISTANT_BUBBLE_VIEW_H_
