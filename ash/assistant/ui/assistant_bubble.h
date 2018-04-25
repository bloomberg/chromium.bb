// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_UI_ASSISTANT_BUBBLE_H_
#define ASH_ASSISTANT_UI_ASSISTANT_BUBBLE_H_

#include "base/macros.h"
#include "ui/views/widget/widget_observer.h"

namespace views {
class Widget;
}  // namespace views

namespace ash {

class AshAssistantController;

namespace {
class AssistantContainerView;
}  // namespace

class AssistantBubble : public views::WidgetObserver {
 public:
  explicit AssistantBubble(AshAssistantController* assistant_controller);
  ~AssistantBubble() override;

  // views::WidgetObserver:
  void OnWidgetClosing(views::Widget* widget) override;

  void Show();
  void Dismiss();

 private:
  AshAssistantController* const assistant_controller_;  // Owned by Shell.

  // Owned by view hierarchy.
  AssistantContainerView* container_view_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(AssistantBubble);
};

}  // namespace ash

#endif  // ASH_ASSISTANT_UI_ASSISTANT_BUBBLE_H_
