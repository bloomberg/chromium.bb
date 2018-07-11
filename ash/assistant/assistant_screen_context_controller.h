// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_ASSISTANT_SCREEN_CONTEXT_CONTROLLER_H_
#define ASH_ASSISTANT_ASSISTANT_SCREEN_CONTEXT_CONTROLLER_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/assistant/model/assistant_screen_context_model.h"
#include "ash/assistant/model/assistant_ui_model_observer.h"
#include "ash/highlighter/highlighter_controller.h"
#include "ash/public/interfaces/assistant_controller.mojom.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "chromeos/services/assistant/public/mojom/assistant.mojom.h"
#include "ui/gfx/geometry/rect.h"

namespace ui {
class LayerTreeOwner;
}  // namespace ui

namespace ash {

class AssistantInteractionController;
class AssistantScreenContextModelObserver;
class AssistantUiController;

class ASH_EXPORT AssistantScreenContextController
    : public AssistantUiModelObserver,
      public HighlighterController::Observer {
 public:
  AssistantScreenContextController();
  ~AssistantScreenContextController() override;

  // Provides a pointer to the |assistant| owned by AssistantController.
  void SetAssistant(chromeos::assistant::mojom::Assistant* assistant);

  // Provides a pointer to the |assistant_interaction_controller| owned by
  // AssistantController.
  void SetAssistantInteractionController(
      AssistantInteractionController* assistant_interaction_controller);

  // Provides a pointer to the |assistant_ui_controller| owned by
  // AssistantController.
  void SetAssistantUiController(AssistantUiController* assistant_ui_controller);

  // Returns a reference to the underlying model.
  const AssistantScreenContextModel* model() const {
    return &assistant_screen_context_model_;
  }

  // Adds/removes the specified screen context model |observer|.
  void AddModelObserver(AssistantScreenContextModelObserver* observer);
  void RemoveModelObserver(AssistantScreenContextModelObserver* observer);

  // Requests a screenshot for the region defined by |rect|. If an empty rect is
  // supplied, the entire screen is captured. Upon screenshot completion, the
  // specified |callback| is run.
  void RequestScreenshot(
      const gfx::Rect& rect,
      mojom::AssistantController::RequestScreenshotCallback callback);

  // AssistantUiModelObserver:
  void OnUiVisibilityChanged(bool visible, AssistantSource source) override;

  // HighlighterController::Observer:
  void OnHighlighterSelectionRecognized(const gfx::Rect& rect) override;

  // Invoked on screen context request finished event.
  void OnScreenContextRequestFinished();

  std::unique_ptr<ui::LayerTreeOwner> CreateLayerForAssistantSnapshotForTest();

 private:
  void RequestScreenContext(const gfx::Rect& rect);

  // Owned by AssistantController.
  chromeos::assistant::mojom::Assistant* assistant_ = nullptr;

  // Owned by AssistantController.
  AssistantInteractionController* assistant_interaction_controller_ = nullptr;

  // Owned by AssistantController.
  AssistantUiController* assistant_ui_controller_ = nullptr;

  AssistantScreenContextModel assistant_screen_context_model_;

  // Weak pointer factory used for screen context requests.
  base::WeakPtrFactory<AssistantScreenContextController>
      screen_context_request_factory_;

  DISALLOW_COPY_AND_ASSIGN(AssistantScreenContextController);
};

}  // namespace ash

#endif  // ASH_ASSISTANT_ASSISTANT_SCREEN_CONTEXT_CONTROLLER_H_
