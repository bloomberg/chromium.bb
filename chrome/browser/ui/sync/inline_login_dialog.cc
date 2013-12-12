// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/sync/inline_login_dialog.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/common/url_constants.h"
#include "ui/gfx/size.h"
#include "url/gurl.h"

// static
void InlineLoginDialog::Show(Profile* profile) {
  chrome::ShowWebDialog(NULL, profile, new InlineLoginDialog(profile));
}

InlineLoginDialog::InlineLoginDialog(Profile* profile)
    : profile_(profile) {
}

ui::ModalType InlineLoginDialog::GetDialogModalType() const {
  return ui::MODAL_TYPE_SYSTEM;
}

base::string16 InlineLoginDialog::GetDialogTitle() const {
  return base::string16();
}

GURL InlineLoginDialog::GetDialogContentURL() const {
  return GURL(chrome::kChromeUIChromeSigninURL);
}

void InlineLoginDialog::GetWebUIMessageHandlers(
    std::vector<content::WebUIMessageHandler*>* handlers) const {
}

void InlineLoginDialog::GetDialogSize(gfx::Size* size) const {
  size->SetSize(380, 290);
}

std::string InlineLoginDialog::GetDialogArgs() const {
  return "[]";
}

void InlineLoginDialog::OnDialogClosed(const std::string& json_retval) {
  delete this;
}

void InlineLoginDialog::OnCloseContents(
    content::WebContents* source, bool* out_close_dialog) {
  if (out_close_dialog)
    *out_close_dialog = true;
}

bool InlineLoginDialog::ShouldShowDialogTitle() const {
  return false;
}

bool InlineLoginDialog::HandleContextMenu(
    const content::ContextMenuParams& params) {
  return true;
}
