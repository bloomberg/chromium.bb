// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/assistant_ui_controller.h"

#include "ash/assistant/assistant_controller.h"
#include "ash/assistant/assistant_interaction_controller.h"
#include "ash/assistant/ui/assistant_container_view.h"
#include "ash/assistant/util/deep_link_util.h"
#include "ash/public/interfaces/assistant_setup.mojom.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/toast/toast_data.h"
#include "ash/system/toast/toast_manager.h"
#include "ash/voice_interaction/voice_interaction_controller.h"
#include "base/optional.h"
#include "chromeos/services/assistant/public/mojom/assistant.mojom.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash {

namespace {

// Toast -----------------------------------------------------------------------

constexpr int kToastDurationMs = 2500;
constexpr char kUnboundServiceToastId[] =
    "assistant_controller_unbound_service";

void ShowToast(const std::string& id, int message_id) {
  ToastData toast(id, l10n_util::GetStringUTF16(message_id), kToastDurationMs,
                  base::nullopt);
  Shell::Get()->toast_manager()->Show(toast);
}

}  // namespace

// AssistantUiController -------------------------------------------------------

AssistantUiController::AssistantUiController(
    AssistantController* assistant_controller)
    : assistant_controller_(assistant_controller) {
  assistant_controller_->AddObserver(this);
  Shell::Get()->highlighter_controller()->AddObserver(this);
}

AssistantUiController::~AssistantUiController() {
  Shell::Get()->highlighter_controller()->RemoveObserver(this);
  assistant_controller_->RemoveObserver(this);

  if (container_view_)
    container_view_->GetWidget()->RemoveObserver(this);
}

void AssistantUiController::SetAssistant(
    chromeos::assistant::mojom::Assistant* assistant) {
  assistant_ = assistant;
}

void AssistantUiController::SetAssistantInteractionController(
    AssistantInteractionController* assistant_interaction_controller) {
  if (assistant_interaction_controller_)
    assistant_interaction_controller_->RemoveModelObserver(this);

  assistant_interaction_controller_ = assistant_interaction_controller;

  if (assistant_interaction_controller_)
    assistant_interaction_controller_->AddModelObserver(this);
}

void AssistantUiController::SetAssistantSetup(
    mojom::AssistantSetup* assistant_setup) {
  assistant_setup_ = assistant_setup;
}

void AssistantUiController::AddModelObserver(
    AssistantUiModelObserver* observer) {
  assistant_ui_model_.AddObserver(observer);
}

void AssistantUiController::RemoveModelObserver(
    AssistantUiModelObserver* observer) {
  assistant_ui_model_.RemoveObserver(observer);
}

void AssistantUiController::OnWidgetActivationChanged(views::Widget* widget,
                                                      bool active) {
  if (active)
    container_view_->RequestFocus();
}

void AssistantUiController::OnWidgetVisibilityChanged(views::Widget* widget,
                                                      bool visible) {
  UpdateUiMode();
}

void AssistantUiController::OnWidgetDestroying(views::Widget* widget) {
  // We need to update the model when the widget is destroyed as this may not
  // have occurred due to a call to HideUi/ToggleUi. This can occur as the
  // result of pressing the ESC key.
  assistant_ui_model_.SetVisible(false, AssistantSource::kUnspecified);

  container_view_->GetWidget()->RemoveObserver(this);
  container_view_ = nullptr;
}

void AssistantUiController::OnInputModalityChanged(
    InputModality input_modality) {
  UpdateUiMode();
}

void AssistantUiController::OnInteractionStateChanged(
    InteractionState interaction_state) {
  if (interaction_state != InteractionState::kActive)
    return;

  // If there is an active interaction, we need to show Assistant UI if it is
  // not already showing. An interaction can only be started when the Assistant
  // UI is hidden if the entry point is hotword.
  ShowUi(AssistantSource::kHotword);
}

void AssistantUiController::OnMicStateChanged(MicState mic_state) {
  UpdateUiMode();
}

bool AssistantUiController::OnCaptionButtonPressed(CaptionButtonId id) {
  switch (id) {
    case CaptionButtonId::kMinimize:
      UpdateUiMode(AssistantUiMode::kMiniUi);
      return true;
    case CaptionButtonId::kClose:
      return false;
  }
  return false;
}

// TODO(dmblack): This event doesn't need to be handled here anymore. Move it
// out of AssistantUiController.
void AssistantUiController::OnDialogPlateButtonPressed(DialogPlateButtonId id) {
  if (id != DialogPlateButtonId::kSettings)
    return;

  // Launch Assistant Settings via deep link.
  assistant_controller_->OpenUrl(
      assistant::util::CreateAssistantSettingsDeepLink());
}

void AssistantUiController::OnHighlighterEnabledChanged(
    HighlighterEnabledState state) {
  switch (state) {
    case HighlighterEnabledState::kEnabled:
      if (!assistant_ui_model_.visible())
        ShowUi(AssistantSource::kStylus);
      break;
    case HighlighterEnabledState::kDisabledByUser:
      if (assistant_ui_model_.visible())
        HideUi(AssistantSource::kStylus);
      break;
    case HighlighterEnabledState::kDisabledBySessionEnd:
      // No action necessary.
      break;
  }
}

void AssistantUiController::OnDeepLinkReceived(const GURL& url) {
  if (!assistant::util::IsWebDeepLink(url))
    return;

  ShowUi(AssistantSource::kDeepLink);
  UpdateUiMode(AssistantUiMode::kWebUi);
}

void AssistantUiController::OnUrlOpened(const GURL& url) {
  // We close Assistant UI when opening a URL in a new tab.
  HideUi(AssistantSource::kUnspecified);
}

void AssistantUiController::ShowUi(AssistantSource source) {
  if (!Shell::Get()->voice_interaction_controller()->setup_completed()) {
    assistant_setup_->StartAssistantOptInFlow();
    return;
  }

  if (!Shell::Get()->voice_interaction_controller()->settings_enabled())
    return;

  if (!assistant_) {
    ShowToast(kUnboundServiceToastId, IDS_ASH_ASSISTANT_ERROR_GENERIC);
    return;
  }

  if (assistant_ui_model_.visible())
    return;

  if (!container_view_) {
    container_view_ = new AssistantContainerView(assistant_controller_);
    container_view_->GetWidget()->AddObserver(this);
  }

  container_view_->GetWidget()->Show();
  assistant_ui_model_.SetVisible(true, source);
}

void AssistantUiController::HideUi(AssistantSource source) {
  if (!assistant_ui_model_.visible())
    return;

  if (container_view_)
    container_view_->GetWidget()->Hide();

  assistant_ui_model_.SetVisible(false, source);
}

void AssistantUiController::ToggleUi(AssistantSource source) {
  if (assistant_ui_model_.visible())
    HideUi(source);
  else
    ShowUi(source);
}

void AssistantUiController::UpdateUiMode(
    base::Optional<AssistantUiMode> ui_mode) {
  // If a UI mode is provided, we will use it in lieu of updating UI mode on the
  // basis of interaction/widget visibility state.
  if (ui_mode.has_value()) {
    assistant_ui_model_.SetUiMode(ui_mode.value());
    return;
  }

  // When stylus input modality is selected, we should be in mini UI mode.
  // Otherwise we fall back to main UI mode.
  assistant_ui_model_.SetUiMode(
      assistant_interaction_controller_->model()->input_modality() ==
              InputModality::kStylus
          ? AssistantUiMode::kMiniUi
          : AssistantUiMode::kMainUi);
}

}  // namespace ash
