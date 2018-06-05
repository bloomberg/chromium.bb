// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_UI_ASSISTANT_BUBBLE_H_
#define ASH_ASSISTANT_UI_ASSISTANT_BUBBLE_H_

#include "ash/ash_export.h"
#include "ash/assistant/model/assistant_bubble_model.h"
#include "ash/assistant/model/assistant_interaction_model_observer.h"
#include "base/macros.h"
#include "ui/views/widget/widget_observer.h"

namespace views {
class Widget;
}  // namespace views

namespace ash {

class AssistantBubbleView;
class AssistantController;

class ASH_EXPORT AssistantBubble : public views::WidgetObserver,
                                   public AssistantInteractionModelObserver {
 public:
  explicit AssistantBubble(AssistantController* assistant_controller);
  ~AssistantBubble() override;

  // Returns the underlying model.
  const AssistantBubbleModel* model() const { return &assistant_bubble_model_; }

  // Adds/removes the specified model |observer|.
  void AddModelObserver(AssistantBubbleModelObserver* observer);
  void RemoveModelObserver(AssistantBubbleModelObserver* observer);

  // views::WidgetObserver:
  void OnWidgetActivationChanged(views::Widget* widget, bool active) override;
  void OnWidgetDestroying(views::Widget* widget) override;

  // AssistantInteractionModelObserver:
  void OnInputModalityChanged(InputModality input_modality) override;
  void OnInteractionStateChanged(InteractionState interaction_state) override;

  // Returns true if assistant bubble is visible, otherwise false.
  bool IsVisible() const;

 private:
  void Show();
  void Dismiss();

  AssistantBubbleModel assistant_bubble_model_;
  AssistantController* const assistant_controller_;  // Owned by Shell.

  AssistantBubbleView* bubble_view_ = nullptr;  // Owned by view hierarchy.

  DISALLOW_COPY_AND_ASSIGN(AssistantBubble);
};

}  // namespace ash

#endif  // ASH_ASSISTANT_UI_ASSISTANT_BUBBLE_H_
