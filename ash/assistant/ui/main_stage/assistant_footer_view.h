// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_UI_MAIN_STAGE_ASSISTANT_FOOTER_VIEW_H_
#define ASH_ASSISTANT_UI_MAIN_STAGE_ASSISTANT_FOOTER_VIEW_H_

#include <memory>

#include "ash/public/interfaces/voice_interaction_controller.mojom.h"
#include "base/macros.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "ui/views/view.h"

namespace ui {
class CallbackLayerAnimationObserver;
}  // namespace ui

namespace ash {

class AssistantController;
class AssistantOptInView;
class SuggestionContainerView;

class AssistantFooterView : public views::View,
                            mojom::VoiceInteractionObserver {
 public:
  explicit AssistantFooterView(AssistantController* assistant_controller);
  ~AssistantFooterView() override;

  // views::View:
  gfx::Size CalculatePreferredSize() const override;
  int GetHeightForWidth(int width) const override;
  void ChildPreferredSizeChanged(views::View* child) override;
  void ChildVisibilityChanged(views::View* child) override;

  // mojom::VoiceInteractionObserver:
  void OnVoiceInteractionStatusChanged(
      mojom::VoiceInteractionState state) override {}
  void OnVoiceInteractionSettingsEnabled(bool enabled) override {}
  void OnVoiceInteractionContextEnabled(bool enabled) override {}
  void OnVoiceInteractionHotwordEnabled(bool enabled) override {}
  void OnVoiceInteractionSetupCompleted(bool completed) override;
  void OnAssistantFeatureAllowedChanged(
      mojom::AssistantAllowedState state) override {}
  void OnLocaleChanged(const std::string& locale) override {}

 private:
  void InitLayout();

  void OnAnimationStarted(const ui::CallbackLayerAnimationObserver& observer);
  bool OnAnimationEnded(const ui::CallbackLayerAnimationObserver& observer);

  AssistantController* const assistant_controller_;  // Owned by Shell.

  SuggestionContainerView* suggestion_container_;  // Owned by view hierarchy.
  AssistantOptInView* opt_in_view_;                // Owned by view hierarchy.

  std::unique_ptr<ui::CallbackLayerAnimationObserver> animation_observer_;

  mojo::Binding<mojom::VoiceInteractionObserver>
      voice_interaction_observer_binding_;

  DISALLOW_COPY_AND_ASSIGN(AssistantFooterView);
};

}  // namespace ash

#endif  // ASH_ASSISTANT_UI_MAIN_STAGE_ASSISTANT_FOOTER_VIEW_H_
