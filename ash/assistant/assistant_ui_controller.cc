// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/assistant_ui_controller.h"

#include "ash/assistant/assistant_controller.h"
#include "ash/assistant/assistant_interaction_controller.h"
#include "ash/assistant/assistant_screen_context_controller.h"
#include "ash/assistant/ui/assistant_container_view.h"
#include "ash/assistant/util/deep_link_util.h"
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

// When hidden, Assistant will automatically close after |kAutoCloseThreshold|.
constexpr base::TimeDelta kAutoCloseThreshold = base::TimeDelta::FromMinutes(5);

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
    : assistant_controller_(assistant_controller), weak_factory_(this) {
  AddModelObserver(this);
  assistant_controller_->AddObserver(this);
  Shell::Get()->highlighter_controller()->AddObserver(this);
}

AssistantUiController::~AssistantUiController() {
  Shell::Get()->highlighter_controller()->RemoveObserver(this);
  assistant_controller_->RemoveObserver(this);
  RemoveModelObserver(this);
}

void AssistantUiController::SetAssistant(
    chromeos::assistant::mojom::Assistant* assistant) {
  assistant_ = assistant;
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
  // We need to update the model when the widget is destroyed as this may have
  // happened outside our control. This can occur as the result of pressing the
  // ESC key, for example.
  assistant_ui_model_.SetVisibility(AssistantVisibility::kClosed,
                                    AssistantSource::kUnspecified);

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
  // not already showing. We don't have enough information here to know what
  // the interaction source is, but at the moment we have no need to know.
  ShowUi(AssistantSource::kUnspecified);
}

void AssistantUiController::OnMicStateChanged(MicState mic_state) {
  UpdateUiMode();
}

void AssistantUiController::OnScreenContextRequestStateChanged(
    ScreenContextRequestState request_state) {
  if (assistant_ui_model_.visibility() != AssistantVisibility::kVisible)
    return;

  // Once screen context request state has become idle, it is safe to activate
  // the Assistant widget without causing complications.
  if (request_state == ScreenContextRequestState::kIdle)
    container_view_->GetWidget()->Activate();
}

void AssistantUiController::OnAssistantMiniViewPressed() {
  InputModality input_modality = assistant_controller_->interaction_controller()
                                     ->model()
                                     ->input_modality();

  // When not using stylus input modality, pressing the Assistant mini view
  // will cause the UI to expand.
  if (input_modality != InputModality::kStylus)
    UpdateUiMode(AssistantUiMode::kMainUi);
}

bool AssistantUiController::OnCaptionButtonPressed(CaptionButtonId id) {
  switch (id) {
    case CaptionButtonId::kBack:
      UpdateUiMode(AssistantUiMode::kMainUi);
      return true;
    case CaptionButtonId::kClose:
      return false;
    case CaptionButtonId::kMinimize:
      UpdateUiMode(AssistantUiMode::kMiniUi);
      return true;
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
      if (assistant_ui_model_.visibility() != AssistantVisibility::kVisible)
        ShowUi(AssistantSource::kStylus);
      break;
    case HighlighterEnabledState::kDisabledByUser:
      if (assistant_ui_model_.visibility() == AssistantVisibility::kVisible)
        HideUi(AssistantSource::kStylus);
      break;
    case HighlighterEnabledState::kDisabledBySessionComplete:
    case HighlighterEnabledState::kDisabledBySessionAbort:
      // No action necessary.
      break;
  }
}

void AssistantUiController::OnAssistantControllerConstructed() {
  assistant_controller_->interaction_controller()->AddModelObserver(this);
  assistant_controller_->screen_context_controller()->AddModelObserver(this);
}

void AssistantUiController::OnAssistantControllerDestroying() {
  assistant_controller_->screen_context_controller()->RemoveModelObserver(this);
  assistant_controller_->interaction_controller()->RemoveModelObserver(this);

  if (container_view_) {
    // Our view hierarchy should not outlive our controllers.
    container_view_->GetWidget()->CloseNow();
    DCHECK_EQ(nullptr, container_view_);
  }
}

void AssistantUiController::OnDeepLinkReceived(
    assistant::util::DeepLinkType type,
    const std::map<std::string, std::string>& params) {
  if (!assistant::util::IsWebDeepLinkType(type))
    return;

  ShowUi(AssistantSource::kDeepLink);
  UpdateUiMode(AssistantUiMode::kWebUi);
}

void AssistantUiController::OnUrlOpened(const GURL& url) {
  // We hide Assistant UI when opening a URL in a new tab.
  if (assistant_ui_model_.visibility() == AssistantVisibility::kVisible)
    HideUi(AssistantSource::kUnspecified);
}

void AssistantUiController::OnUiVisibilityChanged(
    AssistantVisibility new_visibility,
    AssistantVisibility old_visibility,
    AssistantSource source) {
  Shell::Get()->voice_interaction_controller()->NotifyStatusChanged(
      new_visibility == AssistantVisibility::kVisible
          ? mojom::VoiceInteractionState::RUNNING
          : mojom::VoiceInteractionState::STOPPED);

  if (new_visibility == AssistantVisibility::kHidden) {
    // When hiding the UI, start a timer to automatically close ourselves after
    // |kAutoCloseThreshold|. This is to give the user an opportunity to resume
    // their previous session before it is automatically finished.
    auto_close_timer_.Start(FROM_HERE, kAutoCloseThreshold,
                            base::BindRepeating(&AssistantUiController::CloseUi,
                                                weak_factory_.GetWeakPtr(),
                                                AssistantSource::kUnspecified));
  } else {
    auto_close_timer_.Stop();
  }

  // Metalayer should not be sticky. Disable when the UI is no longer visible.
  if (old_visibility == AssistantVisibility::kVisible)
    Shell::Get()->highlighter_controller()->AbortSession();
}

void AssistantUiController::ShowUi(AssistantSource source) {
  if (!Shell::Get()->voice_interaction_controller()->settings_enabled())
    return;

  // TODO(dmblack): Show a more helpful message to the user.
  if (Shell::Get()->voice_interaction_controller()->voice_interaction_state() ==
      mojom::VoiceInteractionState::NOT_READY) {
    ShowToast(kUnboundServiceToastId, IDS_ASH_ASSISTANT_ERROR_GENERIC);
    return;
  }

  if (!assistant_) {
    ShowToast(kUnboundServiceToastId, IDS_ASH_ASSISTANT_ERROR_GENERIC);
    return;
  }

  if (assistant_ui_model_.visibility() == AssistantVisibility::kVisible)
    return;

  if (!container_view_) {
    container_view_ = new AssistantContainerView(assistant_controller_);
    container_view_->GetWidget()->AddObserver(this);
  }

  // Note that we initially show the Assistant widget as inactive. This is
  // necessary due to limitations imposed by retrieving screen context. Once we
  // have finished retrieving screen context, the Assistant widget is activated.
  container_view_->GetWidget()->ShowInactive();
  assistant_ui_model_.SetVisibility(AssistantVisibility::kVisible, source);
}

void AssistantUiController::HideUi(AssistantSource source) {
  if (assistant_ui_model_.visibility() == AssistantVisibility::kHidden)
    return;

  if (container_view_)
    container_view_->GetWidget()->Hide();

  assistant_ui_model_.SetVisibility(AssistantVisibility::kHidden, source);
}

void AssistantUiController::CloseUi(AssistantSource source) {
  if (assistant_ui_model_.visibility() == AssistantVisibility::kClosed)
    return;

  assistant_ui_model_.SetVisibility(AssistantVisibility::kClosed, source);

  if (container_view_) {
    container_view_->GetWidget()->CloseNow();
    DCHECK_EQ(nullptr, container_view_);
  }
}

void AssistantUiController::ToggleUi(AssistantSource source) {
  // When not visible, toggling will show the UI.
  if (assistant_ui_model_.visibility() != AssistantVisibility::kVisible) {
    ShowUi(source);
    return;
  }

  // When in mini state, toggling will restore the main UI.
  if (assistant_ui_model_.ui_mode() == AssistantUiMode::kMiniUi) {
    UpdateUiMode(AssistantUiMode::kMainUi);
    return;
  }

  // In all other cases, toggling closes the UI.
  CloseUi(source);
}

void AssistantUiController::UpdateUiMode(
    base::Optional<AssistantUiMode> ui_mode) {
  // If a UI mode is provided, we will use it in lieu of updating UI mode on the
  // basis of interaction/widget visibility state.
  if (ui_mode.has_value()) {
    assistant_ui_model_.SetUiMode(ui_mode.value());
    return;
  }

  InputModality input_modality = assistant_controller_->interaction_controller()
                                     ->model()
                                     ->input_modality();

  // When stylus input modality is selected, we should be in mini UI mode.
  // Otherwise we fall back to main UI mode.
  assistant_ui_model_.SetUiMode(input_modality == InputModality::kStylus
                                    ? AssistantUiMode::kMiniUi
                                    : AssistantUiMode::kMainUi);
}

AssistantContainerView* AssistantUiController::GetViewForTest() {
  return container_view_;
}

}  // namespace ash
