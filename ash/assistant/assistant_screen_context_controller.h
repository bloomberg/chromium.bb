// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_ASSISTANT_SCREEN_CONTEXT_CONTROLLER_H_
#define ASH_ASSISTANT_ASSISTANT_SCREEN_CONTEXT_CONTROLLER_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/assistant/assistant_controller_observer.h"
#include "ash/assistant/model/assistant_screen_context_model.h"
#include "ash/assistant/model/assistant_ui_model_observer.h"
#include "ash/highlighter/highlighter_controller.h"
#include "ash/public/interfaces/assistant_controller.mojom.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "chromeos/services/assistant/public/mojom/assistant.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "ui/gfx/geometry/rect.h"

namespace ui {
class LayerTreeOwner;
}  // namespace ui

namespace ash {

class AssistantController;
class AssistantScreenContextModelObserver;

class ASH_EXPORT AssistantScreenContextController
    : public chromeos::assistant::mojom::AssistantScreenContextSubscriber,
      public AssistantControllerObserver,
      public AssistantUiModelObserver,
      public HighlighterController::Observer {
 public:
  explicit AssistantScreenContextController(
      AssistantController* assistant_controller);
  ~AssistantScreenContextController() override;

  // Provides a pointer to the |assistant| owned by AssistantController.
  void SetAssistant(chromeos::assistant::mojom::Assistant* assistant);

  // Returns a reference to the underlying model.
  const AssistantScreenContextModel* model() const {
    return &assistant_screen_context_model_;
  }

  // Adds/removes the specified screen context model |observer|.
  void AddModelObserver(AssistantScreenContextModelObserver* observer);
  void RemoveModelObserver(AssistantScreenContextModelObserver* observer);

  // Requests a screenshot for the region defined by |rect| (given in DP). If
  // an empty rect is supplied, the entire screen is captured. Upon screenshot
  // completion, the specified |callback| is run.
  void RequestScreenshot(
      const gfx::Rect& rect,
      mojom::AssistantController::RequestScreenshotCallback callback);

  // AssistantControllerObserver:
  void OnAssistantControllerConstructed() override;
  void OnAssistantControllerDestroying() override;

  // AssistantUiModelObserver:
  void OnUiVisibilityChanged(bool visible, AssistantSource source) override;

  // HighlighterController::Observer:
  void OnHighlighterSelectionRecognized(const gfx::Rect& rect) override;

  // Invoked on screen context request finished event.
  void OnScreenContextRequestFinished();

  // chromeos::assistant::mojom::AssistantScreenContextSubscriber:
  void OnContextualHtmlResponse(const std::string& html) override;

  std::unique_ptr<ui::LayerTreeOwner> CreateLayerForAssistantSnapshotForTest();

 private:
  void RequestScreenContext(const gfx::Rect& rect, bool from_user);

  AssistantController* const assistant_controller_;  // Owned by Shell.

  mojo::Binding<chromeos::assistant::mojom::AssistantScreenContextSubscriber>
      assistant_screen_context_subscriber_binding_;

  // Owned by AssistantController.
  chromeos::assistant::mojom::Assistant* assistant_ = nullptr;

  AssistantScreenContextModel assistant_screen_context_model_;

  // Weak pointer factory used for screen context requests.
  base::WeakPtrFactory<AssistantScreenContextController>
      screen_context_request_factory_;

  DISALLOW_COPY_AND_ASSIGN(AssistantScreenContextController);
};

}  // namespace ash

#endif  // ASH_ASSISTANT_ASSISTANT_SCREEN_CONTEXT_CONTROLLER_H_
