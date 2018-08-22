// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_ASSISTANT_CACHE_CONTROLLER_H_
#define ASH_ASSISTANT_ASSISTANT_CACHE_CONTROLLER_H_

#include "ash/assistant/assistant_controller_observer.h"
#include "ash/assistant/model/assistant_cache_model.h"
#include "ash/assistant/model/assistant_ui_model_observer.h"
#include "ash/public/interfaces/voice_interaction_controller.mojom.h"
#include "base/macros.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace ash {

class AssistantCacheModelObserver;
class AssistantController;

class AssistantCacheController : public AssistantControllerObserver,
                                 public AssistantUiModelObserver,
                                 public mojom::VoiceInteractionObserver {
 public:
  explicit AssistantCacheController(AssistantController* assistant_controller);
  ~AssistantCacheController() override;

  // Returns a reference to the underlying model.
  const AssistantCacheModel* model() const { return &model_; }

  // Adds/removes the specified cache model |observer|.
  void AddModelObserver(AssistantCacheModelObserver* observer);
  void RemoveModelObserver(AssistantCacheModelObserver* observer);

  // AssistantControllerObserver:
  void OnAssistantControllerConstructed() override;
  void OnAssistantControllerDestroying() override;

  // AssistantUiModelObserver:
  void OnUiVisibilityChanged(bool visible, AssistantSource source) override;

 private:
  // mojom::VoiceInteractionObserver:
  void OnVoiceInteractionStatusChanged(
      mojom::VoiceInteractionState state) override {}
  void OnVoiceInteractionSettingsEnabled(bool enabled) override {}
  void OnVoiceInteractionContextEnabled(bool enabled) override;
  void OnVoiceInteractionHotwordEnabled(bool enabled) override {}
  void OnVoiceInteractionSetupCompleted(bool completed) override {}
  void OnAssistantFeatureAllowedChanged(
      mojom::AssistantAllowedState state) override {}
  void OnLocaleChanged(const std::string& locale) override {}

  void UpdateConversationStarters();

  AssistantController* const assistant_controller_;  // Owned by Shell.

  mojo::Binding<mojom::VoiceInteractionObserver> voice_interaction_binding_;

  AssistantCacheModel model_;

  DISALLOW_COPY_AND_ASSIGN(AssistantCacheController);
};

}  // namespace ash

#endif  // ASH_ASSISTANT_ASSISTANT_CACHE_CONTROLLER_H_
