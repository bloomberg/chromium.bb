// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_ASSISTANT_UI_CONTROLLER_H_
#define ASH_ASSISTANT_ASSISTANT_UI_CONTROLLER_H_

#include <map>
#include <string>

#include "ash/ash_export.h"
#include "ash/assistant/assistant_controller_observer.h"
#include "ash/assistant/model/assistant_interaction_model_observer.h"
#include "ash/assistant/model/assistant_screen_context_model_observer.h"
#include "ash/assistant/model/assistant_ui_model.h"
#include "ash/assistant/model/assistant_ui_model_observer.h"
#include "ash/assistant/ui/assistant_mini_view.h"
#include "ash/assistant/ui/caption_bar.h"
#include "ash/assistant/ui/dialog_plate/dialog_plate.h"
#include "ash/highlighter/highlighter_controller.h"
#include "base/macros.h"
#include "base/timer/timer.h"
#include "ui/views/widget/widget_observer.h"

namespace chromeos {
namespace assistant {
namespace mojom {
class Assistant;
}  // namespace mojom
}  // namespace assistant
}  // namespace chromeos

namespace views {
class Widget;
}  // namespace views

namespace ash {

class AssistantContainerView;
class AssistantController;

class ASH_EXPORT AssistantUiController
    : public views::WidgetObserver,
      public AssistantControllerObserver,
      public AssistantInteractionModelObserver,
      public AssistantScreenContextModelObserver,
      public AssistantUiModelObserver,
      public AssistantMiniViewDelegate,
      public CaptionBarDelegate,
      public DialogPlateObserver,
      public HighlighterController::Observer {
 public:
  explicit AssistantUiController(AssistantController* assistant_controller);
  ~AssistantUiController() override;

  // Provides a pointer to the |assistant| owned by AssistantController.
  void SetAssistant(chromeos::assistant::mojom::Assistant* assistant);

  // Returns the underlying model.
  const AssistantUiModel* model() const { return &assistant_ui_model_; }

  // Adds/removes the specified model |observer|.
  void AddModelObserver(AssistantUiModelObserver* observer);
  void RemoveModelObserver(AssistantUiModelObserver* observer);

  // views::WidgetObserver:
  void OnWidgetActivationChanged(views::Widget* widget, bool active) override;
  void OnWidgetVisibilityChanged(views::Widget* widget, bool visible) override;
  void OnWidgetDestroying(views::Widget* widget) override;

  // AssistantInteractionModelObserver:
  void OnInputModalityChanged(InputModality input_modality) override;
  void OnInteractionStateChanged(InteractionState interaction_state) override;
  void OnMicStateChanged(MicState mic_state) override;

  // AssistantScreenContextModelObserver:
  void OnScreenContextRequestStateChanged(
      ScreenContextRequestState request_state) override;

  // AssistantMiniViewDelegate:
  void OnAssistantMiniViewPressed() override;

  // CaptionBarDelegate:
  bool OnCaptionButtonPressed(CaptionButtonId id) override;

  // DialogPlateObserver:
  void OnDialogPlateButtonPressed(DialogPlateButtonId id) override;

  // HighlighterController::Observer:
  void OnHighlighterEnabledChanged(HighlighterEnabledState state) override;

  // AssistantControllerObserver:
  void OnAssistantControllerConstructed() override;
  void OnAssistantControllerDestroying() override;
  void OnDeepLinkReceived(
      assistant::util::DeepLinkType type,
      const std::map<std::string, std::string>& params) override;
  void OnUrlOpened(const GURL& url) override;

  // AssistantUiModelObserver:
  void OnUiVisibilityChanged(AssistantVisibility new_visibility,
                             AssistantVisibility old_visibility,
                             AssistantSource source) override;

  void ShowUi(AssistantSource source);
  void HideUi(AssistantSource source);
  void CloseUi(AssistantSource source);
  void ToggleUi(AssistantSource source);

  AssistantContainerView* GetViewForTest();

 private:
  // Updates UI mode to |ui_mode| if specified. Otherwise UI mode is updated on
  // the basis of interaction/widget visibility state.
  void UpdateUiMode(base::Optional<AssistantUiMode> ui_mode = base::nullopt);

  AssistantController* const assistant_controller_;  // Owned by Shell.

  // Owned by AssistantController.
  chromeos::assistant::mojom::Assistant* assistant_ = nullptr;

  AssistantUiModel assistant_ui_model_;

  AssistantContainerView* container_view_ =
      nullptr;  // Owned by view hierarchy.

  // When hidden, Assistant automatically closes itself to finish the previous
  // session. We delay this behavior to allow the user an opportunity to resume.
  base::OneShotTimer auto_close_timer_;

  base::WeakPtrFactory<AssistantUiController> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(AssistantUiController);
};

}  // namespace ash

#endif  // ASH_ASSISTANT_ASSISTANT_UI_CONTROLLER_H_
