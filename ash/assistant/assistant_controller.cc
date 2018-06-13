// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/assistant_controller.h"

#include "ash/assistant/assistant_bubble_controller.h"
#include "ash/assistant/model/assistant_interaction_model_observer.h"
#include "ash/assistant/model/assistant_query.h"
#include "ash/assistant/model/assistant_ui_element.h"
#include "ash/new_window_controller.h"
#include "ash/session/session_controller.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/toast/toast_data.h"
#include "ash/system/toast/toast_manager.h"
#include "base/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/unguessable_token.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/snapshot/snapshot.h"

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

// AssistantController ---------------------------------------------------------

AssistantController::AssistantController()
    : assistant_event_subscriber_binding_(this),
      assistant_bubble_controller_(
          std::make_unique<AssistantBubbleController>(this)) {
  AddInteractionModelObserver(this);
  Shell::Get()->highlighter_controller()->AddObserver(this);
}

AssistantController::~AssistantController() {
  Shell::Get()->highlighter_controller()->RemoveObserver(this);
  RemoveInteractionModelObserver(this);
}

void AssistantController::BindRequest(
    mojom::AssistantControllerRequest request) {
  assistant_controller_bindings_.AddBinding(this, std::move(request));
}

void AssistantController::SetAssistant(
    chromeos::assistant::mojom::AssistantPtr assistant) {
  assistant_ = std::move(assistant);

  // Subscribe to Assistant events.
  chromeos::assistant::mojom::AssistantEventSubscriberPtr ptr;
  assistant_event_subscriber_binding_.Bind(mojo::MakeRequest(&ptr));
  assistant_->AddAssistantEventSubscriber(std::move(ptr));
}

void AssistantController::SetAssistantImageDownloader(
    mojom::AssistantImageDownloaderPtr assistant_image_downloader) {
  assistant_image_downloader_ = std::move(assistant_image_downloader);
}

void AssistantController::SetWebContentsManager(
    mojom::WebContentsManagerPtr web_contents_manager) {
  web_contents_manager_ = std::move(web_contents_manager);
}

void AssistantController::RequestScreenshot(
    const gfx::Rect& rect,
    RequestScreenshotCallback callback) {
  // TODO(muyuanli): handle multi-display when assistant's behavior is defined.
  auto* root_window = Shell::GetPrimaryRootWindow();
  gfx::Rect source_rect =
      rect.IsEmpty() ? gfx::Rect(root_window->bounds().size()) : rect;
  ui::GrabWindowSnapshotAsyncJPEG(
      root_window, source_rect,
      base::BindRepeating(
          [](RequestScreenshotCallback callback,
             scoped_refptr<base::RefCountedMemory> data) {
            std::move(callback).Run(std::vector<uint8_t>(
                data->front(), data->front() + data->size()));
          },
          base::Passed(&callback)));
}

void AssistantController::ManageWebContents(
    const base::UnguessableToken& id_token,
    mojom::ManagedWebContentsParamsPtr params,
    mojom::WebContentsManager::ManageWebContentsCallback callback) {
  DCHECK(web_contents_manager_);

  const mojom::UserSession* user_session =
      Shell::Get()->session_controller()->GetUserSession(0);

  if (!user_session) {
    LOG(WARNING) << "Unable to retrieve active user session.";
    std::move(callback).Run(base::nullopt);
    return;
  }

  // Supply account ID.
  params->account_id = user_session->user_info->account_id;

  // Specify that we will handle top level browser requests.
  ash::mojom::ManagedWebContentsOpenUrlDelegatePtr ptr;
  web_contents_open_url_delegate_bindings_.AddBinding(this,
                                                      mojo::MakeRequest(&ptr));
  params->open_url_delegate_ptr_info = ptr.PassInterface();

  web_contents_manager_->ManageWebContents(id_token, std::move(params),
                                           std::move(callback));
}

void AssistantController::ReleaseWebContents(
    const base::UnguessableToken& id_token) {
  web_contents_manager_->ReleaseWebContents(id_token);
}

void AssistantController::ReleaseWebContents(
    const std::vector<base::UnguessableToken>& id_tokens) {
  web_contents_manager_->ReleaseAllWebContents(id_tokens);
}

void AssistantController::DownloadImage(
    const GURL& url,
    mojom::AssistantImageDownloader::DownloadCallback callback) {
  DCHECK(assistant_image_downloader_);

  const mojom::UserSession* user_session =
      Shell::Get()->session_controller()->GetUserSession(0);

  if (!user_session) {
    LOG(WARNING) << "Unable to retrieve active user session.";
    std::move(callback).Run(gfx::ImageSkia());
    return;
  }

  AccountId account_id = user_session->user_info->account_id;
  assistant_image_downloader_->Download(account_id, url, std::move(callback));
}

void AssistantController::AddInteractionModelObserver(
    AssistantInteractionModelObserver* observer) {
  assistant_interaction_model_.AddObserver(observer);
}

void AssistantController::RemoveInteractionModelObserver(
    AssistantInteractionModelObserver* observer) {
  assistant_interaction_model_.RemoveObserver(observer);
}

void AssistantController::StartInteraction() {
  if (!assistant_) {
    ShowToast(kUnboundServiceToastId, IDS_ASH_ASSISTANT_ERROR_GENERIC);
    return;
  }
  OnInteractionStarted();
}

void AssistantController::StopInteraction() {
  assistant_interaction_model_.SetInteractionState(InteractionState::kInactive);
}

void AssistantController::ToggleInteraction() {
  if (assistant_interaction_model_.interaction_state() ==
      InteractionState::kInactive) {
    StartInteraction();
  } else {
    StopInteraction();
  }
}

void AssistantController::OnInteractionStateChanged(
    InteractionState interaction_state) {
  if (interaction_state == InteractionState::kActive)
    return;

  // When the user-facing interaction is dismissed, we instruct the service to
  // terminate any listening, speaking, or query in flight.
  has_active_interaction_ = false;
  assistant_->StopActiveInteraction();

  assistant_interaction_model_.ClearInteraction();
  assistant_interaction_model_.SetInputModality(InputModality::kVoice);
}

void AssistantController::OnHighlighterEnabledChanged(
    HighlighterEnabledState state) {
  assistant_interaction_model_.SetInputModality(InputModality::kStylus);
  if (state == HighlighterEnabledState::kEnabled) {
    assistant_interaction_model_.SetInteractionState(InteractionState::kActive);
  } else if (state == HighlighterEnabledState::kDisabledByUser) {
    assistant_interaction_model_.SetInteractionState(
        InteractionState::kInactive);
  }
}

void AssistantController::OnInputModalityChanged(InputModality input_modality) {
  if (input_modality == InputModality::kVoice)
    return;

  // When switching to a non-voice input modality we instruct the underlying
  // service to terminate any listening, speaking, or in flight voice query. We
  // do not do this when switching to voice input modality because initiation of
  // a voice interaction will automatically interrupt any pre-existing activity.
  // Stopping the active interaction here for voice input modality would
  // actually have the undesired effect of stopping the voice interaction.
  if (assistant_interaction_model_.query().type() ==
      AssistantQueryType::kVoice) {
    has_active_interaction_ = false;
    assistant_->StopActiveInteraction();

    // Clear the interaction to reset the UI.
    assistant_interaction_model_.ClearInteraction();
  }
}

void AssistantController::OnInteractionStarted() {
  has_active_interaction_ = true;
  assistant_interaction_model_.SetInteractionState(InteractionState::kActive);
}

void AssistantController::OnInteractionFinished(
    AssistantInteractionResolution resolution) {
  has_active_interaction_ = false;

  // When a voice query is interrupted we do not receive any follow up speech
  // recognition events but the mic is closed.
  if (resolution == AssistantInteractionResolution::kInterruption) {
    assistant_interaction_model_.SetMicState(MicState::kClosed);
  }
}

void AssistantController::OnHtmlResponse(const std::string& response) {
  if (!has_active_interaction_)
    return;

  assistant_interaction_model_.AddUiElement(
      std::make_unique<AssistantCardElement>(response));
}

void AssistantController::OnSuggestionChipPressed(int id) {
  const AssistantSuggestion* suggestion =
      assistant_interaction_model_.GetSuggestionById(id);

  DCHECK(suggestion);

  // If the suggestion contains a non-empty action url, we will handle the
  // suggestion chip pressed event by launching the action url in the browser.
  if (!suggestion->action_url.is_empty()) {
    OpenUrl(suggestion->action_url);
    return;
  }

  // Otherwise, we will submit a simple text query using the suggestion text.
  const std::string text = suggestion->text;

  assistant_interaction_model_.ClearInteraction();
  assistant_interaction_model_.SetQuery(
      std::make_unique<AssistantTextQuery>(text));

  DCHECK(assistant_);
  assistant_->SendTextQuery(text);
}

void AssistantController::OnSuggestionsResponse(
    std::vector<AssistantSuggestionPtr> response) {
  if (!has_active_interaction_)
    return;

  assistant_interaction_model_.AddSuggestions(std::move(response));
}

void AssistantController::OnTextResponse(const std::string& response) {
  if (!has_active_interaction_)
    return;

  assistant_interaction_model_.AddUiElement(
      std::make_unique<AssistantTextElement>(response));
}

void AssistantController::OnSpeechRecognitionStarted() {
  assistant_interaction_model_.ClearInteraction();
  assistant_interaction_model_.SetInputModality(InputModality::kVoice);
  assistant_interaction_model_.SetMicState(MicState::kOpen);
  assistant_interaction_model_.SetQuery(
      std::make_unique<AssistantVoiceQuery>());
}

void AssistantController::OnSpeechRecognitionIntermediateResult(
    const std::string& high_confidence_text,
    const std::string& low_confidence_text) {
  assistant_interaction_model_.SetQuery(std::make_unique<AssistantVoiceQuery>(
      high_confidence_text, low_confidence_text));
}

void AssistantController::OnSpeechRecognitionEndOfUtterance() {
  assistant_interaction_model_.SetMicState(MicState::kClosed);
}

void AssistantController::OnSpeechRecognitionFinalResult(
    const std::string& final_result) {
  assistant_interaction_model_.SetQuery(
      std::make_unique<AssistantVoiceQuery>(final_result));
}

void AssistantController::OnSpeechLevelUpdated(float speech_level) {
  assistant_interaction_model_.SetSpeechLevel(speech_level);
}

void AssistantController::OnOpenUrlResponse(const GURL& url) {
  if (!has_active_interaction_)
    return;

  OpenUrl(url);
}

void AssistantController::OnOpenUrlFromTab(const GURL& url) {
  OpenUrl(url);
}

void AssistantController::OnDialogPlateButtonPressed(DialogPlateButtonId id) {
  if (id == DialogPlateButtonId::kKeyboardInputToggle) {
    assistant_interaction_model_.SetInputModality(InputModality::kKeyboard);
    return;
  }

  DCHECK_EQ(id, DialogPlateButtonId::kVoiceInputToggle);

  switch (assistant_interaction_model_.mic_state()) {
    case MicState::kClosed:
      assistant_->StartVoiceInteraction();
      break;
    case MicState::kOpen:
      has_active_interaction_ = false;
      assistant_->StopActiveInteraction();

      // Clear the interaction to reset the UI.
      assistant_interaction_model_.ClearInteraction();
      break;
  }
}

void AssistantController::OnDialogPlateContentsCommitted(
    const std::string& text) {
  // TODO(dmblack): Handle an empty text query more gracefully by showing a
  // helpful message to the user. Currently we just reset state and pretend as
  // if nothing happened.
  if (text.empty()) {
    assistant_interaction_model_.ClearInteraction();
    return;
  }

  assistant_interaction_model_.ClearInteraction();
  assistant_interaction_model_.SetQuery(
      std::make_unique<AssistantTextQuery>(text));

  assistant_->SendTextQuery(text);
}

void AssistantController::OpenUrl(const GURL& url) {
  Shell::Get()->new_window_controller()->NewTabWithUrl(url);
  StopInteraction();
}

}  // namespace ash
