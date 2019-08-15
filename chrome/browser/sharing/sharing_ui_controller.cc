// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharing/sharing_ui_controller.h"

#include <utility>

#include "chrome/browser/sharing/sharing_constants.h"
#include "chrome/browser/sharing/sharing_device_info.h"
#include "chrome/browser/sharing/sharing_dialog.h"
#include "chrome/browser/sharing/sharing_service.h"
#include "chrome/browser/sharing/sharing_service_factory.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "ui/gfx/vector_icon_types.h"

namespace {

BrowserWindow* GetWindowFromWebContents(content::WebContents* web_contents) {
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents);
  return browser ? browser->window() : nullptr;
}

content::WebContents* GetCurrentWebContents(
    content::WebContents* web_contents) {
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents);
  return browser ? browser->tab_strip_model()->GetActiveWebContents() : nullptr;
}

}  // namespace

SharingUiController::App::App(const gfx::VectorIcon* vector_icon,
                              const gfx::Image& image,
                              base::string16 name,
                              std::string identifier)
    : vector_icon(vector_icon),
      image(image),
      name(std::move(name)),
      identifier(std::move(identifier)) {}

SharingUiController::App::App(App&& other) = default;

SharingUiController::App::~App() = default;

SharingUiController::SharingUiController(content::WebContents* web_contents)
    : web_contents_(web_contents),
      sharing_service_(SharingServiceFactory::GetForBrowserContext(
          web_contents->GetBrowserContext())) {}

SharingUiController::~SharingUiController() = default;

void SharingUiController::ShowNewDialog() {
  if (dialog_)
    dialog_->Hide();

  // Treat the dialog as closed as the process of closing the native widget
  // might be async.
  dialog_ = nullptr;

  BrowserWindow* window = GetWindowFromWebContents(web_contents_);
  if (!window)
    return;

  dialog_ = DoShowDialog(window);
  UpdateIcon();
}

std::vector<SharingDeviceInfo> SharingUiController::GetSyncedDevices() {
  return sharing_service_->GetDeviceCandidates(GetRequiredDeviceCapabilities());
}

void SharingUiController::UpdateIcon() {
  BrowserWindow* window = GetWindowFromWebContents(web_contents_);
  if (!window)
    return;

  auto* icon_container = window->GetOmniboxPageActionIconContainer();
  if (icon_container)
    icon_container->UpdatePageActionIcon(GetIconType());
}

void SharingUiController::OnDialogClosed(SharingDialog* dialog) {
  // Ignore already replaced dialogs.
  if (dialog != dialog_)
    return;

  dialog_ = nullptr;
  UpdateIcon();
}

void SharingUiController::SendMessageToDevice(
    const SharingDeviceInfo& device,
    chrome_browser_sharing::SharingMessage sharing_message) {
  last_dialog_id_++;
  is_loading_ = true;
  send_failed_ = false;
  UpdateIcon();

  sharing_service_->SendMessageToDevice(
      device.guid(), kSharingMessageTTL, std::move(sharing_message),
      base::Bind(&SharingUiController::OnMessageSentToDevice,
                 weak_ptr_factory_.GetWeakPtr(), last_dialog_id_));
}

void SharingUiController::OnMessageSentToDevice(int dialog_id, bool success) {
  if (dialog_id != last_dialog_id_)
    return;

  is_loading_ = false;
  send_failed_ = !success;
  UpdateIcon();

  if (send_failed_ && web_contents_ == GetCurrentWebContents(web_contents_))
    ShowNewDialog();
}

void SharingUiController::InvalidateOldDialog() {
  last_dialog_id_++;
  is_loading_ = false;
  send_failed_ = false;
  ShowNewDialog();
}
