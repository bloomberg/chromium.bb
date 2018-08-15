// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/assistant_controller.h"

#include "ash/assistant/assistant_controller_observer.h"
#include "ash/assistant/assistant_interaction_controller.h"
#include "ash/assistant/assistant_notification_controller.h"
#include "ash/assistant/assistant_screen_context_controller.h"
#include "ash/assistant/assistant_setup_controller.h"
#include "ash/assistant/assistant_ui_controller.h"
#include "ash/assistant/util/deep_link_util.h"
#include "ash/new_window_controller.h"
#include "ash/session/session_controller.h"
#include "ash/shell.h"
#include "ash/voice_interaction/voice_interaction_controller.h"
#include "base/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/unguessable_token.h"

namespace ash {

AssistantController::AssistantController()
    : assistant_volume_control_binding_(this),
      assistant_interaction_controller_(
          std::make_unique<AssistantInteractionController>(this)),
      assistant_notification_controller_(
          std::make_unique<AssistantNotificationController>(this)),
      assistant_screen_context_controller_(
          std::make_unique<AssistantScreenContextController>(this)),
      assistant_setup_controller_(
          std::make_unique<AssistantSetupController>(this)),
      assistant_ui_controller_(std::make_unique<AssistantUiController>(this)),
      voice_interaction_binding_(this),
      weak_factory_(this) {
  mojom::VoiceInteractionObserverPtr ptr;
  voice_interaction_binding_.Bind(mojo::MakeRequest(&ptr));
  Shell::Get()->voice_interaction_controller()->AddObserver(std::move(ptr));

  AddObserver(this);
  NotifyConstructed();
  chromeos::CrasAudioHandler::Get()->AddAudioObserver(this);
}

AssistantController::~AssistantController() {
  chromeos::CrasAudioHandler::Get()->RemoveAudioObserver(this);
  NotifyDestroying();
  RemoveObserver(this);
}

void AssistantController::BindRequest(
    mojom::AssistantControllerRequest request) {
  assistant_controller_bindings_.AddBinding(this, std::move(request));
}

void AssistantController::BindRequest(
    mojom::AssistantVolumeControlRequest request) {
  assistant_volume_control_binding_.Bind(std::move(request));
}

void AssistantController::AddObserver(AssistantControllerObserver* observer) {
  observers_.AddObserver(observer);
}

void AssistantController::RemoveObserver(
    AssistantControllerObserver* observer) {
  observers_.RemoveObserver(observer);
}

void AssistantController::SetAssistant(
    chromeos::assistant::mojom::AssistantPtr assistant) {
  assistant_ = std::move(assistant);

  // Provide reference to sub-controllers.
  assistant_interaction_controller_->SetAssistant(assistant_.get());
  assistant_notification_controller_->SetAssistant(assistant_.get());
  assistant_screen_context_controller_->SetAssistant(assistant_.get());
  assistant_ui_controller_->SetAssistant(assistant_.get());
}

void AssistantController::SetAssistantImageDownloader(
    mojom::AssistantImageDownloaderPtr assistant_image_downloader) {
  assistant_image_downloader_ = std::move(assistant_image_downloader);
}

// TODO(dmblack): Call SetAssistantSetup directly on AssistantSetupController
// instead of going through AssistantController.
void AssistantController::SetAssistantSetup(
    mojom::AssistantSetupPtr assistant_setup) {
  assistant_setup_ = std::move(assistant_setup);
  assistant_setup_controller_->SetAssistantSetup(assistant_setup_.get());
}

void AssistantController::SetWebContentsManager(
    mojom::WebContentsManagerPtr web_contents_manager) {
  web_contents_manager_ = std::move(web_contents_manager);
}

// TODO(dmblack): Expose AssistantScreenContextController over mojo rather
// than implementing RequestScreenshot here in AssistantController.
void AssistantController::RequestScreenshot(
    const gfx::Rect& rect,
    RequestScreenshotCallback callback) {
  assistant_screen_context_controller_->RequestScreenshot(rect,
                                                          std::move(callback));
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

void AssistantController::NavigateWebContentsBack(
    const base::UnguessableToken& id_token,
    mojom::WebContentsManager::NavigateWebContentsBackCallback callback) {
  web_contents_manager_->NavigateWebContentsBack(id_token, std::move(callback));
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

void AssistantController::OnDeepLinkReceived(
    assistant::util::DeepLinkType type,
    const std::map<std::string, std::string>& params) {
  using namespace assistant::util;

  switch (type) {
    case DeepLinkType::kFeedback:
      // TODO(dmblack): Possibly use a new FeedbackSource (this method defaults
      // to kFeedbackSourceAsh). This may be useful for differentiating feedback
      // UI and behavior for Assistant.
      Shell::Get()->new_window_controller()->OpenFeedbackPage();
      break;
    case DeepLinkType::kUnsupported:
    case DeepLinkType::kExplore:
    case DeepLinkType::kOnboarding:
    case DeepLinkType::kQuery:
    case DeepLinkType::kReminders:
    case DeepLinkType::kSettings:
      // No action needed.
      break;
  }
}

void AssistantController::OnOpenUrlFromTab(const GURL& url) {
  OpenUrl(url);
}

void AssistantController::SetVolume(int volume, bool user_initiated) {
  volume = std::min(100, volume);
  volume = std::max(volume, 0);
  chromeos::CrasAudioHandler::Get()->SetOutputVolumePercent(volume);
}

void AssistantController::SetMuted(bool muted) {
  chromeos::CrasAudioHandler::Get()->SetOutputMute(muted);
}

void AssistantController::AddVolumeObserver(mojom::VolumeObserverPtr observer) {
  volume_observer_.AddPtr(std::move(observer));

  int output_volume =
      chromeos::CrasAudioHandler::Get()->GetOutputVolumePercent();
  bool mute = chromeos::CrasAudioHandler::Get()->IsOutputMuted();
  OnOutputMuteChanged(mute, false /* system_adjust */);
  OnOutputNodeVolumeChanged(0 /* node */, output_volume);
}

void AssistantController::OnOutputMuteChanged(bool mute_on,
                                              bool system_adjust) {
  volume_observer_.ForAllPtrs([mute_on](mojom::VolumeObserver* observer) {
    observer->OnMuteStateChanged(mute_on);
  });
}

void AssistantController::OnOutputNodeVolumeChanged(uint64_t node, int volume) {
  // |node| refers to the active volume device, which we don't care here.
  volume_observer_.ForAllPtrs([volume](mojom::VolumeObserver* observer) {
    observer->OnVolumeChanged(volume);
  });
}

void AssistantController::OpenUrl(const GURL& url) {
  if (assistant::util::IsDeepLinkUrl(url)) {
    NotifyDeepLinkReceived(url);
    return;
  }

  Shell::Get()->new_window_controller()->NewTabWithUrl(url);
  NotifyUrlOpened(url);
}

void AssistantController::ShutDown() {
  // Controllers can be added to handle this event as needed.
  assistant_ui_controller_->ShutDown();
}

void AssistantController::NotifyConstructed() {
  for (AssistantControllerObserver& observer : observers_)
    observer.OnAssistantControllerConstructed();
}

void AssistantController::NotifyDestroying() {
  for (AssistantControllerObserver& observer : observers_)
    observer.OnAssistantControllerDestroying();
}

void AssistantController::NotifyDeepLinkReceived(const GURL& deep_link) {
  using namespace assistant::util;

  // Retrieve deep link type and parsed parameters.
  DeepLinkType type = GetDeepLinkType(deep_link);
  const std::map<std::string, std::string> params =
      GetDeepLinkParams(deep_link);

  for (AssistantControllerObserver& observer : observers_)
    observer.OnDeepLinkReceived(type, params);
}

void AssistantController::NotifyUrlOpened(const GURL& url) {
  for (AssistantControllerObserver& observer : observers_)
    observer.OnUrlOpened(url);
}

void AssistantController::OnVoiceInteractionStatusChanged(
    mojom::VoiceInteractionState state) {
  if (state == mojom::VoiceInteractionState::NOT_READY)
    assistant_ui_controller_->HideUi(AssistantSource::kUnspecified);
}

base::WeakPtr<AssistantController> AssistantController::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

}  // namespace ash
