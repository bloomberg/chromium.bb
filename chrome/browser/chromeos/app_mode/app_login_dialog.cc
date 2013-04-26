// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/app_mode/app_login_dialog.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/common/url_constants.h"
#include "googleurl/src/gurl.h"
#include "ui/gfx/size.h"

namespace chromeos {

// static
void AppLoginDialog::Show(Profile* profile) {
  chrome::ShowWebDialog(NULL, profile, new AppLoginDialog(profile));
}

AppLoginDialog::AppLoginDialog(Profile* profile)
    : profile_(profile) {
}

ui::ModalType AppLoginDialog::GetDialogModalType() const {
  return ui::MODAL_TYPE_SYSTEM;
}

string16 AppLoginDialog::GetDialogTitle() const {
  return string16();
}

GURL AppLoginDialog::GetDialogContentURL() const {
  return GURL(chrome::kChromeUIAppLoginURL);
}

void AppLoginDialog::GetWebUIMessageHandlers(
    std::vector<content::WebUIMessageHandler*>* handlers) const {
}

void AppLoginDialog::GetDialogSize(gfx::Size* size) const {
  size->SetSize(380, 290);
}

std::string AppLoginDialog::GetDialogArgs() const {
  return "[]";
}

void AppLoginDialog::OnDialogClosed(const std::string& json_retval) {
  delete this;
}

void AppLoginDialog::OnCloseContents(
    content::WebContents* source, bool* out_close_dialog) {
  if (out_close_dialog)
    *out_close_dialog = true;
}

bool AppLoginDialog::ShouldShowDialogTitle() const {
  return false;
}

bool AppLoginDialog::HandleContextMenu(
    const content::ContextMenuParams& params) {
  return true;
}

}  // namespace chromeos
