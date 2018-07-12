// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/assistant_controller.h"

#include "ash/assistant/assistant_controller_observer.h"
#include "ash/assistant/assistant_interaction_controller.h"
#include "ash/assistant/assistant_notification_controller.h"
#include "ash/assistant/assistant_screen_context_controller.h"
#include "ash/assistant/assistant_ui_controller.h"
#include "ash/assistant/util/deep_link_util.h"
#include "ash/new_window_controller.h"
#include "ash/session/session_controller.h"
#include "ash/shell.h"
#include "base/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/unguessable_token.h"

namespace ash {

AssistantController::AssistantController()
    : assistant_interaction_controller_(
          std::make_unique<AssistantInteractionController>(this)),
      assistant_notification_controller_(
          std::make_unique<AssistantNotificationController>(this)),
      assistant_screen_context_controller_(
          std::make_unique<AssistantScreenContextController>(this)),
      assistant_ui_controller_(std::make_unique<AssistantUiController>(this)),
      weak_factory_(this) {
  NotifyConstructed();
}

AssistantController::~AssistantController() {
  NotifyDestroying();
}

void AssistantController::BindRequest(
    mojom::AssistantControllerRequest request) {
  assistant_controller_bindings_.AddBinding(this, std::move(request));
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

void AssistantController::SetAssistantSetup(
    mojom::AssistantSetupPtr assistant_setup) {
  assistant_setup_ = std::move(assistant_setup);

  // Provide reference to UI controller.
  assistant_ui_controller_->SetAssistantSetup(assistant_setup_.get());
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

void AssistantController::OnOpenUrlFromTab(const GURL& url) {
  OpenUrl(url);
}

void AssistantController::OpenUrl(const GURL& url) {
  if (assistant::util::IsDeepLinkUrl(url)) {
    NotifyDeepLinkReceived(url);
    return;
  }

  Shell::Get()->new_window_controller()->NewTabWithUrl(url);
  NotifyUrlOpened(url);
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
  for (AssistantControllerObserver& observer : observers_)
    observer.OnDeepLinkReceived(deep_link);
}

void AssistantController::NotifyUrlOpened(const GURL& url) {
  for (AssistantControllerObserver& observer : observers_)
    observer.OnUrlOpened(url);
}

base::WeakPtr<AssistantController> AssistantController::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

}  // namespace ash
