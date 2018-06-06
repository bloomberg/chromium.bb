// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_ASSISTANT_BUBBLE_CONTROLLER_H_
#define ASH_ASSISTANT_ASSISTANT_BUBBLE_CONTROLLER_H_

#include "ash/ash_export.h"
#include "ash/assistant/model/assistant_bubble_model.h"
#include "ash/assistant/model/assistant_interaction_model_observer.h"
#include "ash/assistant/ui/caption_bar.h"
#include "base/macros.h"
#include "ui/views/widget/widget_observer.h"

namespace views {
class Widget;
}  // namespace views

namespace ash {

class AssistantBubbleView;
class AssistantController;

class ASH_EXPORT AssistantBubbleController
    : public views::WidgetObserver,
      public AssistantInteractionModelObserver,
      public CaptionBarDelegate {
 public:
  explicit AssistantBubbleController(AssistantController* assistant_controller);
  ~AssistantBubbleController() override;

  // Returns the underlying model.
  const AssistantBubbleModel* model() const { return &assistant_bubble_model_; }

  // Adds/removes the specified model |observer|.
  void AddModelObserver(AssistantBubbleModelObserver* observer);
  void RemoveModelObserver(AssistantBubbleModelObserver* observer);

  // views::WidgetObserver:
  void OnWidgetActivationChanged(views::Widget* widget, bool active) override;
  void OnWidgetVisibilityChanged(views::Widget* widget, bool visible) override;
  void OnWidgetDestroying(views::Widget* widget) override;

  // AssistantInteractionModelObserver:
  void OnInputModalityChanged(InputModality input_modality) override;
  void OnInteractionStateChanged(InteractionState interaction_state) override;
  void OnMicStateChanged(MicState mic_state) override;

  // CaptionBarDelegate:
  bool OnCaptionButtonPressed(CaptionButtonId id) override;

  // Returns true if assistant bubble is visible, otherwise false.
  bool IsVisible() const;

 private:
  void Show();
  void Dismiss();

  // Updates UI mode to |ui_mode| if specified. Otherwise UI mode is updated on
  // the basis of interaction/widget visibility state.
  void UpdateUiMode(base::Optional<AssistantUiMode> ui_mode = base::nullopt);

  AssistantBubbleModel assistant_bubble_model_;
  AssistantController* const assistant_controller_;  // Owned by Shell.

  AssistantBubbleView* bubble_view_ = nullptr;  // Owned by view hierarchy.

  DISALLOW_COPY_AND_ASSIGN(AssistantBubbleController);
};

}  // namespace ash

#endif  // ASH_ASSISTANT_ASSISTANT_BUBBLE_CONTROLLER_H_
