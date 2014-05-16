// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/ui/choose_mobile_network_dialog.h"

#include "chrome/browser/chromeos/login/users/user_manager.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/common/url_constants.h"
#include "ui/gfx/size.h"

using content::WebContents;
using content::WebUIMessageHandler;

namespace {

// Default width/height of the dialog.
const int kDefaultWidth = 350;
const int kDefaultHeight = 225;

}  // namespace

namespace chromeos {

// static
void ChooseMobileNetworkDialog::ShowDialog(gfx::NativeWindow owning_window) {
  chrome::ShowWebDialog(owning_window,
                        ProfileManager::GetActiveUserProfile(),
                        new ChooseMobileNetworkDialog);
}

ChooseMobileNetworkDialog::ChooseMobileNetworkDialog() {
}

ui::ModalType ChooseMobileNetworkDialog::GetDialogModalType() const {
  return ui::MODAL_TYPE_SYSTEM;
}

base::string16 ChooseMobileNetworkDialog::GetDialogTitle() const {
  return base::string16();
}

GURL ChooseMobileNetworkDialog::GetDialogContentURL() const {
  return GURL(chrome::kChromeUIChooseMobileNetworkURL);
}

void ChooseMobileNetworkDialog::GetWebUIMessageHandlers(
    std::vector<WebUIMessageHandler*>* handlers) const {
}

void ChooseMobileNetworkDialog::GetDialogSize(gfx::Size* size) const {
  size->SetSize(kDefaultWidth, kDefaultHeight);
}

std::string ChooseMobileNetworkDialog::GetDialogArgs() const {
  return "[]";
}

void ChooseMobileNetworkDialog::OnDialogClosed(const std::string& json_retval) {
  delete this;
}

void ChooseMobileNetworkDialog::OnCloseContents(WebContents* source,
                                                bool* out_close_dialog) {
  if (out_close_dialog)
    *out_close_dialog = true;
}

bool ChooseMobileNetworkDialog::ShouldShowDialogTitle() const {
  return false;
}

bool ChooseMobileNetworkDialog::HandleContextMenu(
    const content::ContextMenuParams& params) {
  // Disable context menu.
  return true;
}

}  // namespace chromeos
